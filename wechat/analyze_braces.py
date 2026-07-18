#!/usr/bin/env python3
"""分析 index.js 的大括号/括号平衡情况"""
import re
import sys

def analyze(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    lines = content.split('\n')
    brace_depth = 0
    paren_depth = 0
    target_lines = {77, 80, 81, 551, 563, 14228, 14229, 14230, 14535, 14541, 14542, 14543, 14544, 14545}

    print(f"=== Analyzing {path} ===")
    print(f"Total lines: {len(lines)}")

    # Track depth at each line
    depth_history = []
    for i, line in enumerate(lines, 1):
        # Remove strings
        cleaned = re.sub(r"'(?:[^'\\]|\\.)*'", '', line)
        cleaned = re.sub(r'"(?:[^"\\]|\\.)*"', '', cleaned)
        cleaned = re.sub(r'`(?:[^`\\]|\\.)*`', '', cleaned)
        # Remove // comments
        cleaned = re.sub(r'//.*$', '', cleaned)
        # Remove /* */ comments (simple, single-line)
        cleaned = re.sub(r'/\*.*?\*/', '', cleaned)

        old_brace = brace_depth
        old_paren = paren_depth
        brace_depth += cleaned.count('{') - cleaned.count('}')
        paren_depth += cleaned.count('(') - cleaned.count(')')

        if i in target_lines:
            print(f"Line {i}: brace {old_brace}->{brace_depth}, paren {old_paren}->{paren_depth} | {line[:80]}")

    print(f"\nFinal: brace_depth={brace_depth}, paren_depth={paren_depth}")

    # Find where .then() callback opens and track depth
    print("\n=== Tracking .then() callback ===")
    in_then = False
    then_start_line = None
    then_start_brace = None
    for i, line in enumerate(lines, 1):
        if 'return createWasm().then(function' in line:
            in_then = True
            then_start_line = i
            print(f".then() opens at line {i}: {line.strip()}")
            # Recount depth from beginning to this line
            depth = 0
            pdepth = 0
            for j in range(i):
                l = lines[j]
                cleaned = re.sub(r"'(?:[^'\\]|\\.)*'", '', l)
                cleaned = re.sub(r'"(?:[^"\\]|\\.)*"', '', cleaned)
                cleaned = re.sub(r'`(?:[^`\\]|\\.)*`', '', cleaned)
                cleaned = re.sub(r'//.*$', '', cleaned)
                cleaned = re.sub(r'/\*.*?\*/', '', cleaned)
                depth += cleaned.count('{') - cleaned.count('}')
                pdepth += cleaned.count('(') - cleaned.count(')')
            print(f"  Brace depth at .then() open: {depth}, paren depth: {pdepth}")
            then_start_brace = depth
            break

    # Find where .then() closes (should be line with '});' that brings brace back to then_start_brace)
    if then_start_line:
        depth = 0
        pdepth = 0
        for i, line in enumerate(lines, 1):
            cleaned = re.sub(r"'(?:[^'\\]|\\.)*'", '', line)
            cleaned = re.sub(r'"(?:[^"\\]|\\.)*"', '', cleaned)
            cleaned = re.sub(r'`(?:[^`\\]|\\.)*`', '', cleaned)
            cleaned = re.sub(r'//.*$', '', cleaned)
            cleaned = re.sub(r'/\*.*?\*/', '', cleaned)
            depth += cleaned.count('{') - cleaned.count('}')
            pdepth += cleaned.count('(') - cleaned.count(')')

            if i >= then_start_line and depth == then_start_brace and '})' in line:
                print(f".then() closes at line {i}: brace={depth}, paren={pdepth} | {line.strip()}")
                if i > then_start_line + 5:
                    break  # found the close

if __name__ == '__main__':
    path = sys.argv[1] if len(sys.argv) > 1 else r'C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\build\index.js'
    analyze(path)
