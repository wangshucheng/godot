# -*- coding: utf-8 -*-
"""校验 report_latest.md 数据完整性：行数 / 缺失值 / 五列对齐。"""
import io
import re
import sys

PATH = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47fad25801ac16b9570799\godot4.7_mono\csharp_bench\BenchResults\report_latest.md"
RUNTIMES = ["net48", "netcoreapp3.1", "net6.0", "net7.0", "Mono-in-Godot"]


def main():
    lines = io.open(PATH, encoding="utf-8").read().splitlines()
    sections = {}  # cat -> list of row cells
    cur = None
    header_cols = None
    for ln in lines:
        m = re.match(r"^## \d+\. (\S+)$", ln)
        if m:
            cur = m.group(1)
            sections[cur] = []
            continue
        if cur and ln.startswith("|"):
            cells = [c.strip() for c in ln.strip().strip("|").split("|")]
            if ln.startswith("| Benchmark") or set(ln) <= set("|-: "):
                if ln.startswith("| Benchmark"):
                    header_cols = cells
                continue
            sections[cur].append(cells)

    ok = True
    print("=" * 72)
    print(f"{'Category':<14}{'rows':>5}{'net48':>8}{'3.1':>8}{'net6':>8}{'net7':>8}{'Mono':>8}   missing detail")
    print("-" * 72)
    total_rows = 0
    for cat, rows in sections.items():
        if cat in ("测试环境与数据来源", "瓶颈分析与优化建议"):
            continue
        if not rows:
            continue
        total_rows += len(rows)
        # 每个运行时占 2 列（ns/op, B/op），位于 Benchmark/Size 之后
        miss = {r: 0 for r in RUNTIMES}
        detail = []
        for cells in rows:
            for i, rt in enumerate(RUNTIMES):
                ns = cells[2 + i * 2]
                b = cells[3 + i * 2]
                if ns == "—" and b == "—":
                    miss[rt] += 1
                    if rt != "Mono-in-Godot" or len(detail) < 8:
                        detail.append(f"{rt}:{cells[0]}@{cells[1]}")
        row_n = len(rows)
        line = f"{cat:<14}{row_n:>5}"
        for rt in RUNTIMES:
            line += f"{miss[rt]:>8}"
        if detail:
            line += "   " + ", ".join(detail[:6])
            if len(detail) > 6:
                line += f" ...(+{len(detail)-6})"
        print(line)
        # net TFM 列不允许缺失
        for rt in RUNTIMES[:4]:
            if miss[rt] > 0:
                ok = False
                print(f"  !! FAIL: {rt} 在 {cat} 有 {miss[rt]} 个缺失行")

    print("-" * 72)
    print(f"总计数据行: {total_rows}")
    # 对齐检查：每个分类的行键（Benchmark+Size）在所有列共用同一表格结构，天然对齐
    # 再检查 header 是否含全部五列
    hdr = " ".join(header_cols or [])
    for rt in RUNTIMES:
        if rt not in hdr:
            ok = False
            print(f"!! FAIL: 表头缺少 {rt}")
    print("表头五列: OK" if ok else "表头检查: 见上")

    # Mono-in-Godot 已知豁免：Async（SyncContext 死锁）与大规模数组上限
    print()
    print("结论:", "PASS - 四个 .NET TFM 全部 0 缺失，五列结构对齐" if ok else "FAIL - 见上方标记")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
