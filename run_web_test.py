#!/usr/bin/env python3
# run_web_test.py — WASM runner for the csharp_test 25-scenario C# workflow suite.
#
# Serves bin/exports/web/web_test over HTTP, loads index.html in headless
# Chromium (playwright), captures the browser console, and waits for the
# suite summary ("All 24 scenarios complete."). Verdict:
#   1. Exactly 24 "[TEST RESULT]" lines (scenarios 1-24; scenario 0 is
#      desktop-only), none with ": FAIL".
#   2. No "[TEST FAIL]" assertion lines.
#   3. No runtime abort (Aborted()/RuntimeError) in console.
#
# On WASM the suite intentionally does NOT quit (state 26 idle) so this
# runner can observe the Debug UI/console output — see Test.cs state 25.
#
# Usage:  python run_web_test.py [--dir bin/exports/web/web_test] [--port 8123] [--timeout 300]
# Exit:   0 = PASS, 1 = FAIL/timeout/crash.

import argparse
import http.server
import os
import re
import socketserver
import sys
import threading
import time

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=os.path.join("bin", "exports", "web", "web_test"))
    ap.add_argument("--port", type=int, default=8123)
    ap.add_argument("--timeout", type=int, default=600)
    ap.add_argument("--expected", type=int, default=24,
                    help="expected [TEST RESULT] lines (24 on web; scenario 0 is desktop-only)")
    args = ap.parse_args()
    EXPECTED = args.expected

    web_dir = os.path.abspath(args.dir)
    if not os.path.isfile(os.path.join(web_dir, "index.html")):
        print("FAIL: index.html not found in %s" % web_dir)
        return 1

    console_lines = []
    EXPECTED = 24  # overridden by --expected below

    class QuietHandler(http.server.SimpleHTTPRequestHandler):
        def log_message(self, fmt, *a):
            pass
        def end_headers(self):
            # Godot web export requires COOP/COEP for some features; harmless otherwise.
            self.send_header("Cross-Origin-Opener-Policy", "same-origin")
            self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
            super().end_headers()

    os.chdir(web_dir)
    httpd = socketserver.ThreadingTCPServer(("127.0.0.1", args.port), QuietHandler)
    httpd.allow_reuse_address = True
    server_thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    server_thread.start()
    print("[web-test] serving %s on http://127.0.0.1:%d" % (web_dir, args.port))

    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        print("FAIL: playwright not installed (python -m pip install playwright && python -m playwright install chromium)")
        return 1

    done = threading.Event()

    with sync_playwright() as pw:
        browser = pw.chromium.launch(headless=True)
        page = browser.new_page()

        def on_console(msg):
            text = msg.text
            console_lines.append(text)
            # Completion = all expected scenario results seen. (The summary
            # line goes to the on-canvas Debug UI label, not the console;
            # scenario 0 Reflection is desktop-only, hence 24 on web.)
            if text.count("[TEST RESULT]") and len([l for l in console_lines if "[TEST RESULT]" in l]) >= EXPECTED:
                done.set()

        page.on("console", on_console)
        page.on("pageerror", lambda err: console_lines.append("[PAGEERROR] %s" % err))

        print("[web-test] loading page ...")
        try:
            page.goto("http://127.0.0.1:%d/index.html" % args.port, timeout=60000)
        except Exception as e:
            print("FAIL: page load error: %s" % e)
            browser.close()
            return 1

        ok = done.wait(timeout=args.timeout)
        # Give straggler console lines a moment to arrive.
        time.sleep(2)
        browser.close()

    httpd.shutdown()

    all_log = "\n".join(console_lines)
    results = re.findall(r"\[TEST RESULT\].*", all_log)
    fails = re.findall(r"\[TEST FAIL\].*", all_log)
    scenario_fails = [r for r in results if ": FAIL" in r]
    aborted = ("Aborted(" in all_log) or ("RuntimeError" in all_log)

    print("")
    print("=== [TEST RESULT] lines (%d) ===" % len(results))
    for r in results:
        print(r)

    print("")
    print("=== Verification ===")
    failed = 0
    if len(results) == EXPECTED:
        print("PASS: %d/%d scenarios reported" % (len(results), EXPECTED))
    else:
        print("FAIL: expected %d scenario results, got %d" % (EXPECTED, len(results)))
        failed += 1
    if not fails:
        print("PASS: no [TEST FAIL] assertions")
    else:
        print("FAIL: %d failed assertions:" % len(fails))
        for f in fails:
            print("  " + f)
        failed += 1
    if not scenario_fails:
        print("PASS: no scenario FAIL verdict")
    else:
        print("FAIL: scenario FAIL verdicts present")
        failed += 1
    if not aborted:
        print("PASS: no runtime abort")
    else:
        print("FAIL: runtime abort detected")
        failed += 1
    if not ok and len(results) != EXPECTED:
        print("FAIL: expected results not seen within %ds (timeout or early crash)" % args.timeout)
        failed += 1

    print("")
    if failed == 0:
        print("=== RESULT: PASS ===")
        return 0
    print("=== RESULT: FAIL (%d checks failed) ===" % failed)
    return 1

if __name__ == "__main__":
    sys.exit(main())
