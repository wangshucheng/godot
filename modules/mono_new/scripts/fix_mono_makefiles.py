"""
Mono 交叉编译 Makefile 综合修复脚本。

解决 MSYS2/Git Bash 环境下的 Makefile 问题：
1. autotools recipe（autoconf/automake/aclocal/autoheader）调用 $(SHELL)，
   而 $(SHELL) 被 MSYS2 展开为含空格的 "C:/Program Files/Git/usr/bin/sh.exe"，
   导致 shell 拆词失败。→ 替换为 @true
2. config.status recipe 同样调用 $(SHELL)，触发同样的路径问题。
   → 替换为 @true
3. NDK make 是 Windows 原生程序，不理解 MSYS 路径 /e/workspace/...，
   导致 VPATH/srcdir 等变量无法定位源文件。→ 转为 Windows 路径 E:/workspace/...
4. 顶层 SUBDIRS 含 po 等翻译目录，编译不需要且会报错。→ 精简为 llvm mono support
5. $(SHELL) / ${SHELL} 变量引用被 MSYS2 转为含空格的 Windows 路径
   "C:/Program Files/Git/usr/bin/sh.exe"，导致 depcomp 等工具调用失败。
   → 替换 SHELL 变量定义为 sh，替换 ${SHELL}/$(SHELL) 引用为 sh

用法：
    python fix_mono_makefiles.py <build_dir>
"""
import os
import re
import sys


# 需要转换为 Windows 路径的变量名
PATH_VARS = {
    'VPATH', 'srcdir', 'top_srcdir', 'builddir', 'top_builddir',
    'prefix', 'exec_prefix', 'datadir', 'datarootdir', 'libdir',
    'libexecdir', 'includedir', 'pkgdatadir', 'pkgincludedir',
    'pkglibdir', 'pkglibexecdir', 'localedir', 'sysconfdir',
    'bindir', 'sbindir', 'localstatedir', 'sharedstatedir',
    'mandir', 'infodir', 'docdir', 'htmldir', 'dvidir', 'psdir',
    'ACLOCAL_M4', 'CONFIG_HEADER', 'CONFIG_CLEAN_FILES',
    'am__configure_deps', 'am__DIST_COMMON', 'am__aclocal_m4_deps',
}


def msys_path_to_win(m):
    """把 MSYS 路径 /e/path 转为 Windows 路径 E:/path"""
    drive = m.group(1)
    rest = m.group(2)
    return f"{drive.upper()}:/{rest}"


def fix_paths_in_value(value):
    """把 value 中的 MSYS 路径转为 Windows 路径"""
    pattern = re.compile(r'(?<!\w)/([a-zA-Z])/([^\s:;|"\']*)')
    return pattern.sub(msys_path_to_win, value)


def fix_makefile(path):
    """修复单个 Makefile"""
    try:
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            lines = f.readlines()
    except Exception as e:
        return 0, f"read error: {e}"

    # autotools 变量名正则
    auto_tools_vars = (r'\$\(AUTOMAKE\)', r'\$\(AUTOCONF\)',
                       r'\$\(ACLOCAL\)', r'\$\(AUTOHEADER\)')
    # config.status 正则
    config_status_pattern = re.compile(r'\$\(SHELL\)\s+(?:\./)?config\.status')

    changed = 0
    new_lines = []
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]

        # 1. 处理以 tab 开头的 recipe 行
        if line.startswith('\t') and line.strip():
            # 收集整个 recipe 块（处理续行）
            block_lines = [line]
            while i < n and lines[i].rstrip().endswith('\\'):
                i += 1
                if i < n:
                    block_lines.append(lines[i])
                else:
                    break

            block_text = ''.join(block_lines)

            # 检查是否含 autotools 或 config.status 调用
            is_auto = any(re.search(v, block_text) for v in auto_tools_vars)
            is_cs = bool(config_status_pattern.search(block_text))

            if is_auto or is_cs:
                tag = "autotools" if is_auto else "config.status"
                new_lines.append(f'\t@true # {tag} disabled\n')
                changed += 1
                i += 1
                continue
            else:
                new_lines.extend(block_lines)
                i += 1
                continue

        # 2. 处理变量赋值行（路径转换 + SHELL 替换）
        # 匹配 VAR = value 或 VAR := value 格式
        vm = re.match(r'^(\s*)([A-Za-z_][A-Za-z0-9_]*)\s*[:?]?=\s*(.*?)(\r?\n)?$',
                       line)
        if vm:
            indent, varname, value, newline = vm.groups()
            if varname in PATH_VARS and value:
                new_value = fix_paths_in_value(value)
                if new_value != value:
                    new_line = f"{indent}{varname} = {new_value}"
                    if newline:
                        new_line += newline
                    new_lines.append(new_line)
                    changed += 1
                    i += 1
                    continue
            # 5. 替换 SHELL 变量定义：SHELL = /bin/sh → SHELL = sh
            #    MSYS2 会把 /bin/sh 转为含空格的 C:/Program Files/.../sh.exe
            if varname == 'SHELL' and value:
                new_value = 'sh'
                if new_value != value:
                    new_line = f"{indent}{varname} = {new_value}"
                    if newline:
                        new_line += newline
                    new_lines.append(new_line)
                    changed += 1
                    i += 1
                    continue
            # 5b. 替换变量值中的 ${SHELL} / $(SHELL) 引用为 sh
            #     depcomp = $(SHELL) $(top_srcdir)/depcomp → depcomp = sh $(top_srcdir)/depcomp
            #     install_sh = ${SHELL} /e/.../install-sh → install_sh = sh /e/.../install-sh
            if value and ('${SHELL}' in value or '$(SHELL)' in value):
                new_value = value.replace('${SHELL}', 'sh').replace('$(SHELL)', 'sh')
                if new_value != value:
                    new_line = f"{indent}{varname} = {new_value}"
                    if newline:
                        new_line += newline
                    new_lines.append(new_line)
                    changed += 1
                    i += 1
                    continue

        # 3. 处理顶层 SUBDIRS（只处理顶层 Makefile）
        # SUBDIRS = po llvm mono support data runtime scripts man samples msvc acceptance-tests
        sm = re.match(r'^SUBDIRS\s*=\s*(.*?)(\r?\n)?$', line)
        if sm and 'po' in sm.group(1) and 'mono' in sm.group(1):
            newline = sm.group(2)
            new_line = 'SUBDIRS = llvm mono support'
            if newline:
                new_line += newline
            new_lines.append(new_line)
            changed += 1
            i += 1
            continue

        new_lines.append(line)
        i += 1

    if changed > 0:
        try:
            with open(path, 'w', encoding='utf-8', errors='replace',
                      newline='') as f:
                f.writelines(new_lines)
        except Exception as e:
            return 0, f"write error: {e}"

    return changed, "ok" if changed else "skip"


def main():
    if len(sys.argv) < 2:
        print("Usage: python fix_mono_makefiles.py <build_dir>")
        sys.exit(1)

    build_dir = sys.argv[1]
    if not os.path.isdir(build_dir):
        print(f"ERROR: build_dir not found: {build_dir}")
        sys.exit(1)

    total_files = 0
    total_changed = 0
    total_lines = 0

    for root, dirs, files in os.walk(build_dir):
        dirs[:] = [d for d in dirs if d != '.git']
        for name in files:
            if name == 'Makefile':
                path = os.path.join(root, name)
                total_files += 1
                changed, msg = fix_makefile(path)
                if changed > 0:
                    total_changed += 1
                    total_lines += changed
                    rel = os.path.relpath(path, build_dir)
                    print(f"  fixed: {rel} ({changed} edits)")

    print(f"\n=== Summary ===")
    print(f"  Makefiles scanned: {total_files}")
    print(f"  Makefiles modified: {total_changed}")
    print(f"  Total edits: {total_lines}")


if __name__ == '__main__':
    main()
