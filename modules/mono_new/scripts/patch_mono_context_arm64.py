#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
修补 Mono ARM64 mono-context.c 的 FPSIMD 断言崩溃。

问题：mono_sigctx_to_monoctx() 假定 ucontext reserved 区首项即为 FPSIMD context，
      g_assert (fpctx->head.magic == FPSIMD_MAGIC) 在 VkThread 等无浮点上下文的
      线程上失败，触发 SIGABRT 崩溃。

修复：按 Linux ARM64 ucontext 规范遍历 reserved 区查找 FPSIMD section，
      找不到则跳过浮点寄存器复制（GC 安全——浮点寄存器不持有对象引用）。

注意：Android NDK 不暴露 struct _acontext（仅 Linux 内核内部 UAPI），
      但 struct _aarch64_ctx 与 struct fpsimd_context 都在 <asm/sigcontext.h> 中。
      用 _aarch64_ctx 作为迭代头，magic 匹配时强转 fpsimd_context。

用法：python patch_mono_context_arm64.py <mono-context.c 路径>
"""

import sys
import os
import re
import shutil
from datetime import datetime

# 修补后的正确代码块标记
PATCH_MARKER = 'ARM64_LINUX_UCONTEXT_FPSIMD_SCAN_PATCH'
# 正确版本的特征字符串（用 _aarch64_ctx）
CORRECT_SIGNATURE = 'struct _aarch64_ctx *head = (struct _aarch64_ctx *'
# 旧版（损坏）修补的特征字符串（用 _acontext，Android NDK 无此类型）
BROKEN_SIGNATURE = 'struct _acontext *ctx = (struct _acontext *)'

def patch_file(filepath):
    if not os.path.isfile(filepath):
        print(f"ERROR: file not found: {filepath}")
        return 1

    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    # 已修补为正确版本 → 跳过
    if PATCH_MARKER in content and CORRECT_SIGNATURE in content:
        print(f"  already patched (correct _aarch64_ctx version), skip: {filepath}")
        return 0

    # 已修补但为损坏版本（struct _acontext） → 需修复
    if PATCH_MARKER in content and BROKEN_SIGNATURE in content:
        print(f"  detected broken patch (struct _acontext), fixing...")
        return fix_broken_patch(filepath, content)

    # 未修补 → 应用补丁
    return apply_fresh_patch(filepath, content)


def fix_broken_patch(filepath, content):
    """修复使用 struct _acontext 的损坏修补，改为 struct _aarch64_ctx。"""
    backup = f"{filepath}.bak.{datetime.now().strftime('%Y%m%d%H%M%S')}"
    try:
        shutil.copy2(filepath, backup)
        print(f"  backup created: {backup}")
    except Exception as e:
        print(f"  WARNING: backup failed: {e}")

    # 替换损坏的遍历块为正确的 _aarch64_ctx 版本
    # 匹配从 "struct _acontext *ctx =" 到 "/* else: VkThread" 注释结束
    broken_block_pattern = re.compile(
        r'\tstruct _acontext \*ctx = \(struct _acontext \*\)&\(\(ucontext_t\*\)sigctx\)->uc_mcontext\.__reserved;\n'
        r'.*?'
        r'/\* else: VkThread.*?\*/',
        re.DOTALL
    )

    correct_block = '''\t/* Android NDK 不暴露 struct _acontext（仅 Linux 内核内部 UAPI），
\t * 但 struct _aarch64_ctx 与 struct fpsimd_context 都在 <asm/sigcontext.h> 中。
\t * 用 _aarch64_ctx 作为迭代头，magic 匹配时强转 fpsimd_context。 */
\tstruct _aarch64_ctx *head = (struct _aarch64_ctx *)&((ucontext_t*)sigctx)->uc_mcontext.__reserved;
\tstruct fpsimd_context *fpctx = NULL;
\tchar *reserved_end = (char*)&((ucontext_t*)sigctx)->uc_mcontext.__reserved + sizeof(((ucontext_t*)sigctx)->uc_mcontext.__reserved);
\twhile ((char*)head < reserved_end) {
\t\tif (head->magic == 0 && head->size == 0)
\t\t\tbreak; /* 终止符 */
\t\tif (head->magic == FPSIMD_MAGIC) {
\t\t\tfpctx = (struct fpsimd_context *)head;
\t\t\tbreak;
\t\t}
\t\tif (head->size == 0)
\t\t\tbreak; /* 防御：避免无限循环 */
\t\thead = (struct _aarch64_ctx *)((char*)head + head->size);
\t}
\tif (fpctx) {
\t\tint i;
\t\tfor (i = 0; i < 32; ++i)
\t\t\tmctx->fregs [i] = fpctx->vregs [i];
\t}
\t/* else: VkThread 等无浮点上下文的线程，跳过 fregs 复制（GC 安全） */'''

    new_content, n = broken_block_pattern.subn(correct_block, content)
    if n == 0:
        print(f"  ERROR: could not locate broken _acontext block to fix")
        return 1

    print(f"  replaced {n} broken block(s) with correct _aarch64_ctx version")
    with open(filepath, 'w', encoding='utf-8', errors='replace') as f:
        f.write(new_content)
    print(f"  file written: {filepath} ({len(new_content)} bytes)")
    return 0


def apply_fresh_patch(filepath, content):
    """对未修补的原始文件应用补丁。"""
    backup = f"{filepath}.bak.{datetime.now().strftime('%Y%m%d%H%M%S')}"
    try:
        shutil.copy2(filepath, backup)
        print(f"  backup created: {backup}")
    except Exception as e:
        print(f"  WARNING: backup failed: {e}")

    # 原始代码块（精确匹配）
    old_block = '''	memcpy (mctx->regs, UCONTEXT_GREGS (sigctx), sizeof (host_mgreg_t) * 31);
	mctx->pc = UCONTEXT_REG_PC (sigctx);
	mctx->regs [ARMREG_SP] = UCONTEXT_REG_SP (sigctx);
#ifdef __linux__
	struct fpsimd_context *fpctx = (struct fpsimd_context*)&((ucontext_t*)sigctx)->uc_mcontext.__reserved;
	int i;

	g_assert (fpctx->head.magic == FPSIMD_MAGIC);
	for (i = 0; i < 32; ++i)
		mctx->fregs [i] = fpctx->vregs [i];
#endif
	/* FIXME: apple */'''

    # 修补后代码块（使用 _aarch64_ctx，Android NDK 兼容）
    new_block = '''	memcpy (mctx->regs, UCONTEXT_GREGS (sigctx), sizeof (host_mgreg_t) * 31);
	mctx->pc = UCONTEXT_REG_PC (sigctx);
	mctx->regs [ARMREG_SP] = UCONTEXT_REG_SP (sigctx);
#ifdef __linux__
	/* ARM64_LINUX_UCONTEXT_FPSIMD_SCAN_PATCH
	 *
	 * ARM64 Linux ucontext 的 uc_mcontext.__reserved 是一个 4096 字节区，
	 * 由一系列 (magic, size) 头部描述的 section 组成（见 Linux kernel 的
	 * arch/arm64/include/uapi/asm/ucontext.h 与 asm/sigcontext.h）。
	 *
	 * 原 g_assert (fpctx->head.magic == FPSIMD_MAGIC) 假定 reserved 区首项
	 * 即为 FPSIMD context。但 Godot 的 VkThread（Vulkan 渲染线程）在未被
	 * Mono 注册的情况下被信号挂起时，其 ucontext 的 reserved 区可能：
	 *   1. 首项不是 FPSIMD（可能是 ESR_CONTEXT 等）
	 *   2. FPSIMD section 缺失（线程从未使用浮点指令）
	 *   3. magic 为 0（终止符）
	 * 原断言失败调用 monoeg_assert_abort -> abort() -> SIGABRT 崩溃。
	 *
	 * 修复：按 Linux ucontext 规范遍历 reserved 区查找 FPSIMD section，
	 * 找不到则跳过浮点寄存器复制（保持 mctx->fregs 原值），不 abort。
	 * 这对 GC 安全：GC 只需要通用寄存器和栈指针即可扫描引用；
	 * 浮点寄存器不持有对象引用，跳过不影响 GC 正确性。
	 *
	 * 注意：Android NDK 不暴露 struct _acontext（仅 Linux 内核内部 UAPI），
	 * 但 struct _aarch64_ctx 与 struct fpsimd_context 都在 <asm/sigcontext.h>。
	 * 用 _aarch64_ctx 作为迭代头，magic 匹配时强转 fpsimd_context。
	 */
	struct _aarch64_ctx *head = (struct _aarch64_ctx *)&((ucontext_t*)sigctx)->uc_mcontext.__reserved;
	struct fpsimd_context *fpctx = NULL;
	char *reserved_end = (char*)&((ucontext_t*)sigctx)->uc_mcontext.__reserved + sizeof(((ucontext_t*)sigctx)->uc_mcontext.__reserved);
	while ((char*)head < reserved_end) {
		if (head->magic == 0 && head->size == 0)
			break; /* 终止符 */
		if (head->magic == FPSIMD_MAGIC) {
			fpctx = (struct fpsimd_context *)head;
			break;
		}
		if (head->size == 0)
			break; /* 防御：避免无限循环 */
		head = (struct _aarch64_ctx *)((char*)head + head->size);
	}
	if (fpctx) {
		int i;
		for (i = 0; i < 32; ++i)
			mctx->fregs [i] = fpctx->vregs [i];
	}
	/* else: VkThread 等无浮点上下文的线程，跳过 fregs 复制（GC 安全） */
#endif
	/* FIXME: apple */'''

    if old_block not in content:
        print(f"  WARNING: old block not found exactly, trying regex match...")
        # 用正则匹配更宽松地查找
        pattern = re.compile(
            r'memcpy \(mctx->regs, UCONTEXT_GREGS \(sigctx\).*?'
            r'/\* FIXME: apple \*/',
            re.DOTALL
        )
        if pattern.search(content):
            content = pattern.sub(new_block, content)
            print(f"  patched via regex: {filepath}")
        else:
            print(f"  ERROR: could not find target block in {filepath}")
            return 1
    else:
        content = content.replace(old_block, new_block)
        print(f"  patched via exact match: {filepath}")

    with open(filepath, 'w', encoding='utf-8', errors='replace') as f:
        f.write(content)

    print(f"  file written: {filepath} ({len(content)} bytes)")
    return 0

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python patch_mono_context_arm64.py <mono-context.c path>")
        sys.exit(1)
    sys.exit(patch_file(sys.argv[1]))
