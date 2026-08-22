#!/usr/bin/env python3
# run_wasm_bench.py — WASM runner for the CSharpBench Godot host (Mono 6.12
# interpreter-in-WASM).
#
# Serves bin/exports/web/web_test over HTTP (pre-compressed .br/.gz, COOP/COEP,
# CSP headers), loads index.html in headless Chromium (playwright), and
# harvests the benchmark result rows the C# host prints to the console:
#
#   [csbench-probe] name=PASS/FAIL ...          capability probes
#   [csbench-row-cold] {json}                   one cold result row
#   [csbench-row-warm] {json}                   one warm result row
#   [csbench-godot] ...                         progress / config lines
#   [csbench-manifest] warm|Name|size           planned units (web only)
#   [csbench-done]                              all units finished
#
# Modes:
#   --smoke           exit as soon as probes are printed AND >= --rows result
#                     rows arrived (no need to finish the whole suite)
#   (default)         run to [csbench-done] or --timeout, then write JSON.
#
# Output (default under csharp_bench/BenchResults/):
#   csbench_godot_wasm.json / csbench_godot_wasm_cold.json
#   log_wasm_bench.txt (full console log)
#
# Resume across crashes: the host cannot be parameterized on WASM (no env
# vars, no cmdline channel), so every page load reruns everything. If the
# runtime dies mid-run, rerun this script; rows are merged per (name,size)
# with the newest value winning, so a crashed category is retried while
# completed categories keep their previous rows.

import argparse
import http.server
import json
import os
import re
import socketserver
import sys
import threading
import time
from pathlib import Path
from urllib.parse import unquote

HERE = Path(__file__).resolve().parent
DEFAULT_WEB_DIR = HERE / "bin" / "exports" / "web" / "web_test"
DEFAULT_OUT_DIR = HERE / "csharp_bench" / "BenchResults"

MIME_TYPES = {
    ".wasm": "application/wasm",
    ".js": "application/javascript",
    ".html": "text/html",
    ".pck": "application/octet-stream",
    ".png": "image/png",
    ".css": "text/css",
    ".json": "application/json",
}
COMPRESSIBLE_EXT = {".wasm", ".pck", ".js", ".html"}


class BenchHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *a):
        pass

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        # CSP required by Godot web export (inline bootstrap script) and the
        # Mono WASM interpreter (eval + wasm compilation)
        self.send_header("Content-Security-Policy",
                         "default-src 'self' 'unsafe-inline' data: blob:; "
                         "script-src 'self' 'unsafe-inline' 'unsafe-eval' "
                         "'wasm-unsafe-eval'; "
                         "worker-src 'self' blob:; "
                         "connect-src 'self' data: blob:; "
                         "media-src 'self' data: blob:; "
                         "img-src 'self' data: blob:")
        super().end_headers()

    def do_GET(self):
        path = unquote(self.path.split("?")[0].lstrip("/")) or "index.html"
        fs = Path(self.directory) / path
        if not fs.is_file():
            self.send_error(404)
            return
        ext = fs.suffix.lower()
        mime = "application/javascript" if fs.name.endswith(".worklet.js") \
            else MIME_TYPES.get(ext, "application/octet-stream")
        accept = self.headers.get("Accept-Encoding", "")
        serve, enc = fs, None
        if ext in COMPRESSIBLE_EXT:
            br, gz = Path(str(fs) + ".br"), Path(str(fs) + ".gz")
            if "br" in accept.lower() and br.is_file():
                serve, enc = br, "br"
            elif "gzip" in accept.lower() and gz.is_file():
                serve, enc = gz, "gzip"
        data = serve.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(data)))
        if enc:
            self.send_header("Content-Encoding", "br" if enc == "br" else "gzip")
        self.end_headers()
        self.wfile.write(data)


class Collector:
    def __init__(self):
        self.rows = {}          # (name, size) -> row dict  (warm)
        self.cold_rows = {}     # (name, size) -> row dict
        self.probes = []
        self.manifest = []
        self.info = []
        self.done = False
        self.crashed = False
        self.last_activity = time.time()
        self.row_count = 0

    def feed(self, text):
        self.last_activity = time.time()
        if "[csbench-done]" in text:
            self.done = True
        elif text.startswith("[csbench-probe] "):
            self.probes.append(text[len("[csbench-probe] "):].strip())
        elif text.startswith("[csbench-row-warm] "):
            self._row(json.loads(text[len("[csbench-row-warm] "):]), False)
        elif text.startswith("[csbench-row-cold] "):
            self._row(json.loads(text[len("[csbench-row-cold] "):]), True)
        elif text.startswith("[csbench-manifest] "):
            self.manifest.append(text[len("[csbench-manifest] "):].strip())
        elif text.startswith("[csbench-godot]"):
            self.info.append(text.strip())
        if ("RuntimeError" in text or "Aborted(" in text
                or "[PAGEERROR]" in text):
            # Known-benign page errors: headless Chromium has no real audio
            # context, so Godot's AudioWorkletNode construction always fails.
            # It does not affect the benchmark runtime.
            if "AudioWorkletNode" in text:
                self.info.append("[driver-benign] " + text.strip())
            else:
                self.crashed = True
                self.info.append("[driver] " + text.strip())

    def _row(self, row, cold):
        key = (row.get("name"), row.get("size"))
        if cold:
            self.cold_rows[key] = row
        else:
            self.rows[key] = row
        self.row_count += 1


def merge_json(path, rows, extra):
    """Write rows list to path, merging with any existing file (newest wins)."""
    merged = {}
    if os.path.isfile(path):
        try:
            with open(path, "r", encoding="utf-8") as f:
                old = json.load(f)
            for r in old.get("results", []):
                merged[(r.get("name"), r.get("size"))] = r
        except Exception:
            pass
    for r in rows:
        merged[(r.get("name"), r.get("size"))] = r
    doc = dict(extra)
    doc["results"] = [merged[k] for k in sorted(merged, key=lambda k: (str(k[0]), k[1] or 0))]
    with open(path, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=1)
    return len(doc["results"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=str(DEFAULT_WEB_DIR))
    ap.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR))
    ap.add_argument("--port", type=int, default=8124)
    ap.add_argument("--timeout", type=int, default=5400,
                    help="overall wall-clock timeout in seconds")
    ap.add_argument("--stall", type=int, default=600,
                    help="no-console-output stall timeout in seconds")
    ap.add_argument("--smoke", action="store_true",
                    help="exit after probes + --rows rows (no full run)")
    ap.add_argument("--rows", type=int, default=4,
                    help="smoke mode: result rows needed")
    ap.add_argument("--tag", default="wasm",
                    help="output file suffix (csbench_godot_<tag>.json)")
    args = ap.parse_args()

    web_dir = Path(args.dir).resolve()
    if not (web_dir / "index.html").is_file():
        print("FAIL: index.html not found in %s" % web_dir)
        return 1
    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    log_path = out_dir / ("log_wasm_bench_%s.txt" % args.tag)
    warm_path = out_dir / ("csbench_godot_%s.json" % args.tag)
    cold_path = out_dir / ("csbench_godot_%s_cold.json" % args.tag)

    os.chdir(web_dir)
    handler = lambda *a, **kw: BenchHandler(*a, directory=str(web_dir), **kw)
    httpd = socketserver.ThreadingTCPServer(("127.0.0.1", args.port), handler)
    httpd.allow_reuse_address = True
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    print("[wasm-bench] serving %s on http://127.0.0.1:%d" % (web_dir, args.port))

    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        print("FAIL: playwright not installed")
        return 1

    col = Collector()
    full_log = []
    t0 = time.time()

    with sync_playwright() as pw:
        browser = pw.chromium.launch(headless=True)
        page = browser.new_page()
        page.on("console", lambda m: (full_log.append(m.text), col.feed(m.text)))
        page.on("pageerror", lambda e: (full_log.append("[PAGEERROR] %s" % e),
                                        col.feed("[PAGEERROR] %s" % e)))
        print("[wasm-bench] loading page ...")
        try:
            page.goto("http://127.0.0.1:%d/index.html" % args.port,
                      timeout=120000)
        except Exception as e:
            print("FAIL: page load error: %s" % e)
            browser.close()
            return 1

        # Wait loop: smoke (probes + N rows) or full ([csbench-done]).
        last_report = 0.0
        while True:
            elapsed = time.time() - t0
            if elapsed - last_report > 30:
                last_report = elapsed
                print("[wasm-bench] %4ds probes=%d rows=%d manifest=%d "
                      "crashed=%s done=%s"
                      % (elapsed, len(col.probes), col.row_count,
                         len(col.manifest), col.crashed, col.done))
            if args.smoke:
                need_probe = len(col.probes) >= 3  # sync_context + yield + taskrun
                if need_probe and col.row_count >= args.rows:
                    print("[wasm-bench] smoke criteria met "
                          "(probes=%d rows=%d)" % (len(col.probes), col.row_count))
                    break
            else:
                if col.done:
                    print("[wasm-bench] host reported [csbench-done]")
                    break
            if col.crashed:
                print("[wasm-bench] runtime crash detected")
                break
            if elapsed > args.timeout:
                print("[wasm-bench] overall timeout (%ds)" % args.timeout)
                break
            if time.time() - col.last_activity > args.stall:
                # Console may be silent for minutes while the interpreter grinds
                # through a slow unit (e.g. Expression_Compile size=10000) and
                # Chromium can also throttle console events. Probe the page: a
                # responsive page with a live benchmark means we keep waiting.
                alive = False
                try:
                    alive = page.evaluate("(() => { try { return Date.now() > 0; } catch (e) { return false; } })()")
                except Exception:
                    alive = False
                if alive:
                    print("[wasm-bench] %ds console silence but page alive "
                          "(slow unit?) - extending stall window"
                          % (time.time() - col.last_activity))
                    col.last_activity = time.time()
                else:
                    print("[wasm-bench] stalled %ds without console output "
                          "and page unresponsive"
                          % args.stall)
                    break
            time.sleep(2)
        time.sleep(3)  # let straggler rows arrive
        try:
            browser.close()
        except Exception:
            pass
    httpd.shutdown()

    with open(log_path, "w", encoding="utf-8") as f:
        f.write("\n".join(full_log))

    runtime = "Mono 6.12 (Godot 4.7 embedded, WASM interpreter)"
    n_warm = merge_json(str(warm_path), list(col.rows.values()), {
        "schema": "csbench/1", "engine": "godot-wasm", "host": "godot-wasm",
        "runtime": runtime, "tfm": "mono6.12-godot-wasm",
        "runtimeId": "mono6.12-godot-wasm", "mode": "quick", "pass": "warm"})
    n_cold = merge_json(str(cold_path), list(col.cold_rows.values()), {
        "schema": "csbench/1", "engine": "godot-wasm", "host": "godot-wasm",
        "runtime": runtime, "tfm": "mono6.12-godot-wasm",
        "runtimeId": "mono6.12-godot-wasm", "mode": "quick", "pass": "cold"})

    print("\n===== probes =====")
    for p in col.probes:
        print("  " + p)
    print("===== summary =====")
    print("  console lines : %d" % len(full_log))
    print("  probes        : %d" % len(col.probes))
    print("  manifest rows : %d" % len(col.manifest))
    print("  warm rows     : %d (merged total %d -> %s)"
          % (len(col.rows), n_warm, warm_path.name))
    print("  cold rows     : %d (merged total %d -> %s)"
          % (len(col.cold_rows), n_cold, cold_path.name))
    skipped = [r for r in list(col.rows.values()) + list(col.cold_rows.values())
               if r.get("skipped")]
    if skipped:
        print("  skipped rows  : %d" % len(skipped))
        for r in skipped:
            print("    %s/%s size=%s reason=%s"
                  % (r.get("category"), r.get("name"), r.get("size"),
                     r.get("skipReason")))
    print("  crashed       : %s" % col.crashed)
    print("  wall seconds  : %.0f" % (time.time() - t0))
    print("  full log      : %s" % log_path)

    if args.smoke:
        ok = (not col.crashed) and len(col.probes) >= 3 \
            and col.row_count >= args.rows
    else:
        ok = (not col.crashed) and col.done
    print("VERDICT: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
