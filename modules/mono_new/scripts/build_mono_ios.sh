#!/bin/bash
# =============================================================================
# Mono 6.12 静态库交叉编译脚本 - iOS arm64 (需 macOS)
# =============================================================================
# 必须在 macOS 上运行（需 Xcode + iOS SDK）。产物：
#   libmonosgen-2.0.a   (sgen GC + mini runtime, arm64)
#   libmono-ee-interp.a (interpreter 执行引擎)
#   libmonoutils.a      (工具库)
#   libeglib.a          (嵌入式 glib)
#   libz.a               (zlib)
#
# 产物部署到：godot4_7_mono/modules/mono_new/thirdparty/mono/lib/ios/arm64/
#
# 前置要求：
#   1. macOS 11+ with Xcode 13+
#   2. iOS SDK (xcodebuild -showsdks 应列出 iphoneos)
#   3. Mono 源码 (E:/workspace/godot/mono，需同步到 macOS)
#   4. 已安装 autoconf/automake/libtool (brew install autoconf automake libtool)
#   5. 桌面版 Mono (brew install mono) 用于交叉编译 BCL
#
# 关键约束：
#   - iOS App Store 禁止 JIT（W^X 内存保护）
#   - 必须用 interpreter 模式（MONO_EE_MODE_INTERP）或 Full AOT
#   - 本脚本只编译 runtime 静态库；interpreter 模式由 gd_mono.cpp 在运行时设置
#   - dlopen 禁用，所有代码必须静态链接
#
# 用法：
#   bash build_mono_ios.sh
#
# 参考：
#   - Mono 官方 iOS 交叉编译：https://www.mono-project.com/docs/compiling-mono/ios/
#   - modules/mono/build_scripts/mono_configure.py:37-43
# =============================================================================

set -e

# -----------------------------------------------------------------------------
# 配置
# -----------------------------------------------------------------------------
MONO_SRC="${MONO_SRC:-/path/to/mono}"   # macOS 上 Mono 源码路径
IOS_SDK_VERSION="${IOS_SDK_VERSION:-17.0}"
DEPLOY_DIR="${DEPLOY_DIR:-/path/to/godot4_7_mono/modules/mono_new/thirdparty/mono/lib/ios/arm64}"
SYSTEM_MONO="${SYSTEM_MONO:-/usr/local}"  # brew install mono 的 prefix
ARCH="arm64"
HOST_TRIPLE="aarch64-apple-darwin"
BUILD_DIR="$MONO_SRC/build-ios-$ARCH"

# iOS SDK 路径
IOS_SDK=$(xcrun --sdk iphoneos --show-sdk-path)
echo "=== iOS SDK: $IOS_SDK ==="

echo "=== Mono iOS 交叉编译配置 ==="
echo "  MONO_SRC    = $MONO_SRC"
echo "  ARCH        = $ARCH ($HOST_TRIPLE)"
echo "  BUILD_DIR   = $BUILD_DIR"
echo "  DEPLOY_DIR  = $DEPLOY_DIR"
echo ""

# -----------------------------------------------------------------------------
# Step 1: autogen.sh
# -----------------------------------------------------------------------------
cd "$MONO_SRC"
if [ ! -f configure ]; then
    echo "=== Step 1: 运行 autogen.sh ==="
    NOCONFIGURE=1 ./autogen.sh
fi

# -----------------------------------------------------------------------------
# Step 2: configure（iOS arm64 交叉编译）
# -----------------------------------------------------------------------------
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ ! -f Makefile ]; then
    echo "=== Step 2: configure (host=$HOST_TRIPLE) ==="
    # iOS 交叉编译关键参数：
    #   --host=aarch64-apple-darwin  让 configure 按 ARM64 macOS/iOS 处理
    #   --disable-mcs-build           不编译 BCL（用桌面 Mono 的 mcs 交叉编译）
    #   --enable-static --disable-shared  iOS 不支持动态库
    #   --with-tls=__thread            iOS 不支持 pthread_key
    #   --with-glib=embedded           内嵌 eglib，不依赖系统 glib
    #   --disable-boehm                禁用 boehm GC，只用 sgen
    #   -D__IOS__ -DMONO_IOS=1         iOS 平台宏
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
        CC="$(xcrun --sdk iphoneos -f clang) -arch arm64 -isysroot $IOS_SDK" \
        CXX="$(xcrun --sdk iphoneos -f clang++) -arch arm64 -isysroot $IOS_SDK" \
        CFLAGS="-O2 -g -D__IOS__ -DMONO_IOS=1 -fPIC" \
        CXXFLAGS="-O2 -g -D__IOS__ -DMONO_IOS=1 -fPIC" \
        LDFLAGS="-arch arm64 -isysroot $IOS_SDK"
    echo "  configure done"
fi

# -----------------------------------------------------------------------------
# Step 3: make（约 20-40 分钟）
# -----------------------------------------------------------------------------
echo "=== Step 3: make -j$(sysctl -n hw.ncpu) ==="
make -j"$(sysctl -n hw.ncpu)" 2>&1 | tee "$BUILD_DIR/build.log"
echo "  make done"

# -----------------------------------------------------------------------------
# Step 4: 收集静态库
# -----------------------------------------------------------------------------
echo "=== Step 4: 收集静态库到 $DEPLOY_DIR ==="
mkdir -p "$DEPLOY_DIR"

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
        sz=$(stat -f%z "$src" 2>/dev/null || stat -c%s "$src")
        echo "  copied: $(basename $lib) ($sz bytes)"
    else
        echo "  WARNING: $lib not found"
    fi
done

# -----------------------------------------------------------------------------
# Step 5: 部署 iOS BCL
# -----------------------------------------------------------------------------
echo "=== Step 5: 部署 iOS BCL（复用桌面 Mono 4.5 profile，需后续验证）==="
BCL_SRC="$SYSTEM_MONO/lib/mono/4.5"
BCL_DST="$DEPLOY_DIR/../../../../../preload/mono/lib/mono/4.5/ios-$ARCH"
mkdir -p "$BCL_DST"
for dll in mscorlib.dll System.dll System.Core.dll System.Numerics.dll System.Numerics.Vectors.dll System.Runtime.CompilerServices.Unsafe.dll I18N.dll I18N.West.dll; do
    if [ -f "$BCL_SRC/$dll" ]; then
        cp -f "$BCL_SRC/$dll" "$BCL_DST/$dll"
        echo "  copied: $dll"
    fi
done
echo "  Note: iOS BCL 复用桌面版，若报 invalid CIL image 需单独构建 iOS 专用 BCL"

# -----------------------------------------------------------------------------
# Step 6: 验证架构（确保是 arm64）
# -----------------------------------------------------------------------------
echo "=== Step 6: 验证产物架构 ==="
for lib in "$DEPLOY_DIR"/*.a; do
    if [ -f "$lib" ]; then
        archs=$(lipo -info "$lib" 2>/dev/null || file "$lib")
        echo "  $(basename $lib): $archs"
    fi
done

echo ""
echo "=== 全部完成 ==="
echo "  静态库：$DEPLOY_DIR"
echo "  BCL：   $BCL_DST"
echo ""
echo "下一步："
echo "  1. 把 $DEPLOY_DIR 同步到 Windows 工作区（或直接在 macOS 上构建 Godot iOS）"
echo "  2. 在 godot4_7_mono 下运行："
echo "     scons -j4 platform=ios target=template_release arch=arm64 \\"
echo "         mono_new_static=yes mono_new_ios_interp=yes"
echo "  3. 验证 Godot iOS 导出包生成 .ipa 且包含 Mono 符号"
