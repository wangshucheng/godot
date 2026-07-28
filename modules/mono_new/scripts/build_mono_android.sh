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
# Step 1: 创建 NDK standalone toolchain（首次运行约 2 分钟）
# -----------------------------------------------------------------------------
TOOLCHAIN_DIR="$NDK_ROOT/../standalone-$ARCH"
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
        --without-ikvm-native \
        --without-x \
        CFLAGS="-fPIE -fPIC -O2 -g -D__ANDROID__ -D__BIONIC__" \
        CXXFLAGS="-fPIE -fPIC -O2 -g -D__ANDROID__ -D__BIONIC__" \
        LDFLAGS="-fPIE -pie"
    echo "  configure done"
else
    echo "=== Step 3: Makefile 已存在，跳过 configure ==="
fi

# -----------------------------------------------------------------------------
# Step 4: make（约 20-40 分钟）
# -----------------------------------------------------------------------------
echo "=== Step 4: make -j$(nproc) ==="
make -j"$(nproc)" 2>&1 | tee "$BUILD_DIR/build.log"
echo "  make done"

# -----------------------------------------------------------------------------
# Step 5: 收集静态库产物
# -----------------------------------------------------------------------------
echo "=== Step 5: 收集静态库到 $DEPLOY_DIR ==="
mkdir -p "$DEPLOY_DIR"

# 关键库（必须）
LIBS_TO_COLLECT=(
    "mono/sgen/.libs/libmonosgen-2.0.a"
    "mono/mini/.libs/libmono-ee-interp.a"
    "mono/mini/.libs/libmonoutils.a"
    "mono/eglib/.libs/libeglib.a"
    "external/zlib/.libs/libz.a"
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
