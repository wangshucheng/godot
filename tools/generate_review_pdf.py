"""
Generate team-review PDF from the crafted review_for_team.html (V1) or
review_for_team_v2.html (V2, accurate full data + SpanMemory fix).
Usage:  python generate_review_pdf.py [v2]
Uses Playwright Chromium headless: native Chinese font support, preserves SVG vector,
A4 page with 18mm margins respects @page rules.
"""
import asyncio
import sys
from pathlib import Path

BASE = Path(r"c:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47fad25801ac16b9570799\godot4.7_mono\csharp_bench\BenchResults")
V2 = len(sys.argv) > 1 and sys.argv[1].lower() == "v2"
HTML = BASE / ("review_for_team_v2.html" if V2 else "review_for_team.html")
PDF = BASE / ("CSharpBench_TeamReview_V2_20260819.pdf" if V2 else "CSharpBench_MonoFix_TeamReview_20260819.pdf")

assert HTML.exists(), f"Missing source HTML: {HTML}"


async def main() -> int:
    try:
        from playwright.async_api import async_playwright
    except ImportError:
        print("playwright not installed; pip install playwright && playwright install chromium")
        return 1

    async with async_playwright() as p:
        browser = await p.chromium.launch(headless=True)
        page = await browser.new_page(device_scale_factor=1.5)
        await page.goto(HTML.resolve().as_uri(), wait_until="load", timeout=30000)
        await page.wait_for_timeout(300)  # let fonts/layout settle
        pdf_bytes = await page.pdf(
            format="A4",
            margin={"top": "11mm", "bottom": "11mm", "left": "13mm", "right": "13mm"},
            print_background=True,
            prefer_css_page_size=True,
        )
        PDF.write_bytes(pdf_bytes)
        await browser.close()

    size_kb = PDF.stat().st_size / 1024
    print(f"PDF written: {PDF}")
    print(f"Size: {size_kb:,.1f} KB")
    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
