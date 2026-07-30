#!/bin/bash
# =============================================================================
# Mono 6.12 静态库交叉编译脚本 - Android arm64
# =============================================================================
# 在 Windows 上通过 Git Bash 或 WSL 运行。产物：
#   libmonosgen-2.0.a   (sgen GC + mini runtime)
#   libmono-ee-interp.a (interpreter 执行引擎)
#   libmonoutils.a      (工具库)
#   libeglib.a          (嵌入式 glib)
#   libz.a               (zlib)
#
# 产物部署到：godot4_7_mono/modules/mono_new/thirdparty/mono/lib/android/arm64/
#
# 前置要求：
#   1. NDK r21+ (项目用 21.3.6528147，路径 D:/software/Android/SDK/ndk/)
#   2. Mono 源码 (E:/workspace/godot/mono)
#   3. 系统已安装 autoconf/automake/libtool (Git Bash 自带或 MSYS2)
#   4. 已有桌面版 Mono (C:/Program Files/Mono) 用于交叉编译 BCL
#
# 用法：
#   bash build_mono_android.sh
#
# 参考：
#   - Mono 官方 Android 交叉编译：https://www.mono-project.com/docs/compiling-mono/android/
#   - eng/common/cross/android/arm64/toolchain.cmake (Mono 源码内)
#   - modules/mono/build_scripts/mono_configure.py:51-55
# =============================================================================

set -e  # 任意命令失败即退出

# -----------------------------------------------------------------------------
# 配置（按需修改）
# -----------------------------------------------------------------------------
MONO_SRC="${MONO_SRC:-E:/workspace/godot/mono}"
NDK_ROOT="${NDK_ROOT:-D:/software/Android/SDK/ndk/21.3.6528147}"
ANDROID_API="${ANDROID_API:-29}"
ARCH="${ARCH:-arm64}"   # arm64 | arm | x86 | x86_64
BUILD_DIR="${BUILD_DIR:-$MONO_SRC/build-android-$ARCH}"
DEPLOY_DIR="${DEPLOY_DIR:-C:/Users/Administrator/AppData/Roaming/TRAE SOLO CN/ModularData/ai-agent/work-mode-projects/6a47e7225801ac16b95705a6/godot4_7_mono/modules/mono_new/thirdparty/mono/lib/android/$ARCH}"
SYSTEM_MONO="${SYSTEM_MONO:-C:/Program Files/Mono}"

# 根据 ARCH 确定 host triple
case "$ARCH" in
    arm64)  HOST_TRIPLE="aarch64-linux-android"; ABI="arm64-v8a" ;;
    arm)    HOST_TRIPLE="armv7-linux-androideabi"; ABI="armeabi-v7a" ;;
    x86)    HOST_TRIPLE="i686-linux-android"; ABI="x86" ;;
    x86_64) HOST_TRIPLE="x86_64-linux-android"; ABI="x86_64" ;;
    *) echo "Unknown ARCH=$ARCH"; exit 1 ;;
esac

echo "=== Mono Android 交叉编译配置 ==="
echo "  MONO_SRC    = $MONO_SRC"
echo "  NDK_ROOT    = $NDK_ROOT"
echo "  ARCH        = $ARCH ($HOST_TRIPLE)"
echo "  ANDROID_API = $ANDROID_API"
echo "  BUILD_DIR   = $BUILD_DIR"
echo "  DEPLOY_DIR  = $DEPLOY_DIR"
echo ""

# -----------------------------------------------------------------------------
# 关键修复：MSYS2/Git Bash 路径转换
# -----------------------------------------------------------------------------
# Windows 驱动器路径（如 "E:/workspace/..."）会破坏 GNU make 的目标解析——
# make 把 "E:" 当作 target pattern 的分隔符，报 "multiple target patterns"。
# 必须把所有传入 configure 的路径转为 Unix 风格 (/e/workspace/...)。
# MSYS2 在调用 Windows 二进制（如 clang.exe）时会自动把 Unix 路径转回
# Windows 路径，所以交叉编译器仍能正确解析 -I/-L 参数。
if command -v cygpath >/dev/null 2>&1; then
    echo "=== 路径转换（Windows -> MSYS Unix 风格）==="
    MONO_SRC="$(cygpath -u "$MONO_SRC")"
    NDK_ROOT="$(cygpath -u "$NDK_ROOT")"
    BUILD_DIR="$(cygpath -u "$BUILD_DIR")"
    DEPLOY_DIR="$(cygpath -u "$DEPLOY_DIR")"
    SYSTEM_MONO="$(cygpath -u "$SYSTEM_MONO")"
    echo "  MONO_SRC    = $MONO_SRC"
    echo "  NDK_ROOT    = $NDK_ROOT"
    echo "  BUILD_DIR   = $BUILD_DIR"
    echo "  DEPLOY_DIR  = $DEPLOY_DIR"
    echo "  SYSTEM_MONO = $SYSTEM_MONO"
    echo ""
fi

# -----------------------------------------------------------------------------
# Step 1: 创建 NDK standalone toolchain（首次运行约 2 分钟）
# -----------------------------------------------------------------------------
TOOLCHAIN_DIR="$NDK_ROOT/../standalone-$ARCH"
# 关键修复：Git Bash/MSYS2 把 PATH 中的 ':' 当作分隔符，导致 Windows 驱动器路径
# (如 "D:/software/...") 被拆成 "D" + "/software/..." 两段，configure 找不到
# aarch64-linux-android-clang。必须用 cygpath 转 Unix 风格 (/d/software/...)。
if command -v cygpath >/dev/null 2>&1; then
    TOOLCHAIN_DIR="$(cygpath -u "$TOOLCHAIN_DIR")"
fi
if [ ! -d "$TOOLCHAIN_DIR" ]; then
    echo "=== Step 1: 创建 NDK standalone toolchain ==="
    python "$NDK_ROOT/build/tools/make_standalone_toolchain.py" \
        --arch "$ARCH" \
        --api "$ANDROID_API" \
        --install-dir "$TOOLCHAIN_DIR"
    echo "  done: $TOOLCHAIN_DIR"
else
    echo "=== Step 1: toolchain 已存在，跳过 ==="
fi
export PATH="$TOOLCHAIN_DIR/bin:$PATH"

# 验证 clang 可被 shell 找到（防止 PATH 配置再次出错）
if ! command -v aarch64-linux-android-clang >/dev/null 2>&1; then
    echo "ERROR: aarch64-linux-android-clang not found in PATH"
    echo "  PATH=$PATH"
    exit 1
fi
echo "  clang 路径: $(command -v aarch64-linux-android-clang)"
echo "  clang 版本: $(aarch64-linux-android-clang --version | head -1)"

# -----------------------------------------------------------------------------
# Step 1.5: 修补 mono-context.c 的 ARM64 FPSIMD 断言崩溃
# -----------------------------------------------------------------------------
# 问题：mono_sigctx_to_monoctx() 的 g_assert (fpctx->head.magic == FPSIMD_MAGIC)
#       在 VkThread 等无浮点上下文的线程上失败，触发 SIGABRT。
# 修复：遍历 ucontext reserved 区查找 FPSIMD section，找不到则跳过（GC 安全）。
echo "=== Step 1.5: 修补 mono-context.c (ARM64 FPSIMD assertion) ==="
PATCH_SCRIPT="$(dirname "$0")/patch_mono_context_arm64.py"
MONO_CONTEXT_C="$MONO_SRC/mono/utils/mono-context.c"
if [ -f "$PATCH_SCRIPT" ] && [ -f "$MONO_CONTEXT_C" ]; then
    PATCH_SCRIPT_WIN="$(cygpath -w "$PATCH_SCRIPT" 2>/dev/null || echo "$PATCH_SCRIPT")"
    MONO_CONTEXT_C_WIN="$(cygpath -w "$MONO_CONTEXT_C" 2>/dev/null || echo "$MONO_CONTEXT_C")"
    SYS_PY=""
    for cand in /c/Python313/python.exe /c/Python312/python.exe /c/Python311/python.exe; do
        if [ -x "$cand" ]; then SYS_PY="$cand"; break; fi
    done
    if [ -n "$SYS_PY" ]; then
        "$SYS_PY" "$PATCH_SCRIPT_WIN" "$MONO_CONTEXT_C_WIN"
    else
        python "$PATCH_SCRIPT_WIN" "$MONO_CONTEXT_C_WIN"
    fi
else
    echo "  WARNING: patch script or mono-context.c not found, skip patch"
fi
echo ""

# -----------------------------------------------------------------------------
# Step 2: autogen.sh（若 configure 不存在）
# -----------------------------------------------------------------------------
cd "$MONO_SRC"
if [ ! -f configure ]; then
    echo "=== Step 2: 运行 autogen.sh ==="
    NOCONFIGURE=1 ./autogen.sh
fi

# -----------------------------------------------------------------------------
# Step 3: configure（交叉编译配置）
# -----------------------------------------------------------------------------
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ ! -f Makefile ]; then
    echo "=== Step 3: configure (host=$HOST_TRIPLE) ==="
    "$MONO_SRC/configure" \
        --host="$HOST_TRIPLE" \
        --target="$HOST_TRIPLE" \
        --cache-file="$BUILD_DIR/config.cache" \
        --prefix="$BUILD_DIR/install" \
        --disable-mcs-build \
        --disable-shared \
        --enable-static \
        --with-sgen=yes \
        --with-glib=embedded \
        --with-libgc-threads=posix \
        --with-tls=__thread \
        --enable-threads=posix \
        --disable-debug-helpers \
        --disable-boehm \
        --disable-btls \
        --without-ikvm-native \
        --without-x \
        CFLAGS="-fPIE -fPIC -O2 -g -D__ANDROID__ -D__BIONIC__" \
        CXXFLAGS="-fPIE -fPIC -O2 -g -D__ANDROID__ -D__BIONIC__" \
        LDFLAGS="-fPIE -pie" \
        CC="$HOST_TRIPLE-clang" \
        CXX="$HOST_TRIPLE-clang++" \
        LD="$HOST_TRIPLE-ld" \
        AR="$HOST_TRIPLE-ar" \
        RANLIB="$HOST_TRIPLE-ranlib" \
        STRIP="$HOST_TRIPLE-strip"
    echo "  configure done"

    # 关键修复：综合修复 Makefile（用 Python 脚本，比 sed 更可靠）。
    # 解决三个问题：
    # 1. autotools recipe 调用 $(SHELL)，路径含空格 → 替换为 @true
    # 2. config.status recipe 同样问题 → 替换为 @true
    # 3. NDK make 不理解 MSYS 路径 /e/... → 转为 Windows 路径 E:/...
    # 4. 精简 SUBDIRS（移除 po 等翻译目录）
    FIX_SCRIPT="$(dirname "$0")/fix_mono_makefiles.py"
    if [ -f "$FIX_SCRIPT" ]; then
        echo "  修复 Makefile（autotools/config.status/路径/SUBDIRS）..."
        # NDK python.exe 是 Windows 原生程序，不认 MSYS 路径 /c/...
        # 用 cygpath 转为 Windows 路径；优先用系统 Python（C:/Python313）
        FIX_SCRIPT_WIN="$(cygpath -w "$FIX_SCRIPT" 2>/dev/null || echo "$FIX_SCRIPT")"
        BUILD_DIR_WIN="$(cygpath -w "$BUILD_DIR" 2>/dev/null || echo "$BUILD_DIR")"
        SYS_PY=""
        for cand in /c/Python313/python.exe /c/Python312/python.exe /c/Python311/python.exe; do
            if [ -x "$cand" ]; then SYS_PY="$cand"; break; fi
        done
        if [ -n "$SYS_PY" ]; then
            "$SYS_PY" "$FIX_SCRIPT_WIN" "$BUILD_DIR_WIN"
        else
            python "$FIX_SCRIPT_WIN" "$BUILD_DIR_WIN"
        fi
    else
        echo "  WARNING: fix_mono_makefiles.py not found, skip Makefile fixes"
    fi
else
    echo "=== Step 3: Makefile 已存在，跳过 configure ==="
fi

# -----------------------------------------------------------------------------
# Step 4: make（约 20-40 分钟）
# -----------------------------------------------------------------------------
echo "=== Step 4: make -j$(nproc) ==="
# Makefile 中的 autotools/config.status recipe 已被禁用（@true），
# 不需要 -o 标志或 AUTO*=true 覆盖。
# PYTHON 覆盖：configure 可能检测到含空格的 TRAE IDE Python 路径，
# 导致 genmdesc.py 等脚本调用失败。用系统 Python 替代。
SYS_PYTHON="${SYS_PYTHON:-C:/Python313/python.exe}"
make -j"$(nproc)" PYTHON="$SYS_PYTHON" \
    2>&1 | tee "$BUILD_DIR/build.log"
MAKE_EXIT=${PIPESTATUS[0]}

# -----------------------------------------------------------------------------
# Step 5: 收集静态库产物
# -----------------------------------------------------------------------------
echo "=== Step 5: 收集静态库到 $DEPLOY_DIR ==="
mkdir -p "$DEPLOY_DIR"

# 关键库（必须）
# 注意：libmonosgen-2.0.a 在 mono/mini/.libs/ 下（不是 mono/sgen/）
#       libmonoutils.a 在 mono/utils/.libs/ 下（不是 mono/mini/）
LIBS_TO_COLLECT=(
    "mono/mini/.libs/libmonosgen-2.0.a"
    "mono/mini/.libs/libmono-ee-interp.a"
    "mono/utils/.libs/libmonoutils.a"
    "mono/eglib/.libs/libeglib.a"
    "mono/metadata/.libs/libmonoruntimesgen.a"
    "mono/sgen/.libs/libmonosgen.a"
    "mono/mini/.libs/libmono-dbg.a"
    "mono/utils/.libs/libmonomath.a"
)

for lib in "${LIBS_TO_COLLECT[@]}"; do
    src="$BUILD_DIR/$lib"
    dst="$DEPLOY_DIR/$(basename $lib)"
    if [ -f "$src" ]; then
        cp -f "$src" "$dst"
        echo "  copied: $(basename $lib) ($(stat -c%s "$src" 2>/dev/null || stat -f%z "$src") bytes)"
    else
        echo "  WARNING: $lib not found"
    fi
done

# -----------------------------------------------------------------------------
# Step 6: 交叉编译 BCL（用桌面 Mono 的 mcs）
# -----------------------------------------------------------------------------
echo "=== Step 6: 部署 Android BCL（复用桌面 Mono 4.5 profile）==="
BCL_SRC="$SYSTEM_MONO/lib/mono/4.5"
BCL_DST="$DEPLOY_DIR/../../../../../preload/mono/lib/mono/4.5/android-$ARCH"
mkdir -p "$BCL_DST"
for dll in mscorlib.dll System.dll System.Core.dll System.Numerics.dll System.Numerics.Vectors.dll System.Runtime.CompilerServices.Unsafe.dll I18N.dll I18N.West.dll; do
    if [ -f "$BCL_SRC/$dll" ]; then
        cp -f "$BCL_SRC/$dll" "$BCL_DST/$dll"
        echo "  copied: $dll"
    fi
done
echo "  Note: Android BCL 复用桌面版 4.5 profile，首次运行时若报 invalid CIL image"
echo "  需要单独构建 Android 专用 BCL（见 MONO_BUILD_STATIC.md 第 7 节）"

echo ""
echo "=== 全部完成 ==="
echo "  静态库：$DEPLOY_DIR"
echo "  BCL：   $BCL_DST"
echo ""
echo "下一步："
echo "  1. 在 godot4_7_mono 下运行："
echo "     scons -j4 platform=android target=template_release arch=arm64 \\"
echo "         mono_new_static=yes mono_new_android_interp=yes"
echo "  2. 验证 libgodot_android.so 生成且包含 Mono 符号"
