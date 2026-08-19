"""Quick PDF integrity check: page count, per-page text snippets, PNG size.
Usage:  python verify_review_pdf.py [v2]
"""
import sys
from pathlib import Path

_BASE = Path(r"c:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47fad25801ac16b9570799\godot4.7_mono\csharp_bench\BenchResults")
V2 = len(sys.argv) > 1 and sys.argv[1].lower() == "v2"
PDF = _BASE / ("CSharpBench_TeamReview_V2_20260819.pdf" if V2 else "CSharpBench_MonoFix_TeamReview_20260819.pdf")

def try_pypdf2():
    try:
        from PyPDF2 import PdfReader
    except ImportError:
        return None
    r = PdfReader(str(PDF))
    out = {"pages": len(r.pages), "snippets": [], "fulltexts": {}}
    for i, p in enumerate(r.pages):
        t = (p.extract_text() or "")
        out["fulltexts"][i + 1] = t
        # keep first 120 non-space chars
        snippet = " ".join(t.split())[:140]
        out["snippets"].append((i + 1, snippet))
    return out

def try_pdfminer():
    try:
        from pdfminer.high_level import extract_text
    except ImportError:
        return None
    text = extract_text(str(PDF))
    # page breaks: \f
    pages = text.split("\f")
    out = {"pages": len([p for p in pages if p.strip()]), "snippets": [], "fulltexts": {}}
    for i, p in enumerate(p for p in pages if p.strip()):
        out["fulltexts"][i + 1] = p
        snippet = " ".join(p.split())[:140]
        out["snippets"].append((i + 1, snippet))
    return out

def main() -> int:
    size_kb = PDF.stat().st_size / 1024
    print(f"File: {PDF.name}  size={size_kb:,.1f} KB")
    info = try_pypdf2() or try_pdfminer()
    if not info:
        print("No PDF text-parser (pypdf2/pdfminer) available; visual check required.")
        return 2
    print(f"Pages detected: {info['pages']}")
    if V2:
        exp = 6
        markers = [
            ("P1-封面", ["C# 基准测试", "v2.0", "2026-08-19"]),
            ("P2-结论", ["关键结论摘要", "22/22", "266×4"]),
            ("P3-反射验证", ["反射崩溃修复验证", "Property_GetValue_ViaGetter", "0 SKIP"]),
            ("P4-SpanMemory修复", ["SpanMemory 栈溢出根因修复", "stackalloc", "0xC00000FD"]),
            ("P5-性能对比", ["核心性能跨运行时对比", "net48", "netcore3.1", "net6.0", "net7.0", "Mono-Godot"]),
            ("P6-建议", ["编码建议与后续行动", "行动项", "P0", "P1", "P2"]),
        ]
    else:
        exp = 5
        markers = [
            ("P1-封面", ["C# 基准测试", "Mono 反射崩溃修复", "2026-08-19"]),
            ("P2-结论", ["关键结论摘要", "22/22", "210/212"]),
            ("P3-反射验证", ["修复前后对比", "Property_GetValue_ViaGetter", "0 skip"]),
            ("P4-性能对比", ["核心性能跨运行时对比", "net48", "netcore3.1", "net6.0", "net7.0", "Mono-Godot"]),
            ("P5-建议", ["编码建议与后续行动", "行动项", "P0", "P1", "P2"]),
        ]
    status = "OK" if info["pages"] == exp else f"WARN (expected {exp})"
    print(f"Page count: {status}")
    # Build page full-text index (prefer non-truncated fulltexts from the parser used)
    page_texts = dict(info.get("fulltexts") or {})
    if not page_texts:
        page_texts = {pn: sn for (pn, sn) in info["snippets"]}
    all_ok = True
    for label, keywords in markers:
        hits = []
        for pn in sorted(page_texts):
            if all(kw in page_texts[pn] for kw in keywords):
                hits.append(pn)
        if hits:
            print(f"  {label}: found on page(s) {hits} ✔")
        else:
            print(f"  {label}: NOT FOUND (keywords={keywords}) ✘")
            all_ok = False
    print("\nPer-page snippet (first 140 chars):")
    for (pn, sn) in info["snippets"]:
        print(f"  [P{pn}] {sn}")
    return 0 if (all_ok and info["pages"] == exp) else 3

if __name__ == "__main__":
    sys.exit(main())
