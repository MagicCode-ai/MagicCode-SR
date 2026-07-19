#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/../../.." && pwd)
BUILD_DIR="$SCRIPT_DIR/build_ios"
OBJ_DIR="$BUILD_DIR/obj"
OUT_LIB="$BUILD_DIR/libmagic_sr_unity_ios.a"

IOS_MIN_VERSION=${IOS_MIN_VERSION:-15.6}
IOS_SDK=${IOS_SDK:-iphoneos}

CORE_PROJECT="$PROJECT_ROOT/build/ipad/magic_sr/magic_sr.xcodeproj"
RELEASE_CORE_LIB="$PROJECT_ROOT/../release/v1.1.0/lib/ios/libmagic_sr.a"
LEGACY_CORE_LIB="$PROJECT_ROOT/build/ipad/magic_sr/Release-iphoneos/libmagic_sr.a"
CORE_LIB="$RELEASE_CORE_LIB"

if [ ! -f "$CORE_LIB" ]; then
  CORE_LIB="$LEGACY_CORE_LIB"
fi

COMMON_HEADER_SEARCH_PATHS="$PROJECT_ROOT/interface $PROJECT_ROOT/src $PROJECT_ROOT/src/metal"
COMMON_DEFINES="SYS_IOS=1 HAVE_NEON=1"

mkdir -p "$OBJ_DIR"

if [ ! -f "$CORE_LIB" ]; then
  echo "[MagicSR Unity iOS] Core library not found in release/legacy paths, building MagicSR core library..."
  xcodebuild \
    -project "$CORE_PROJECT" \
    -scheme magic_sr \
    -configuration Release \
    -sdk "$IOS_SDK" \
    build \
    HEADER_SEARCH_PATHS="$COMMON_HEADER_SEARCH_PATHS" \
    GCC_PREPROCESSOR_DEFINITIONS="$COMMON_DEFINES" \
    IPHONEOS_DEPLOYMENT_TARGET="$IOS_MIN_VERSION" \
    CODE_SIGNING_ALLOWED=NO
  CORE_LIB="$LEGACY_CORE_LIB"
fi

if [ ! -f "$CORE_LIB" ]; then
  echo "[MagicSR Unity iOS] Missing core library: $CORE_LIB" >&2
  exit 1
fi

SDK_PATH=$(xcrun --sdk "$IOS_SDK" --show-sdk-path)
CXX=$(xcrun --sdk "$IOS_SDK" --find clang++)
LIBTOOL=$(xcrun --sdk "$IOS_SDK" --find libtool)

echo "[MagicSR Unity iOS] Compiling Unity wrapper..."
"$CXX" \
  -arch arm64 \
  -isysroot "$SDK_PATH" \
  -miphoneos-version-min="$IOS_MIN_VERSION" \
  -std=c++17 \
  -fvisibility=hidden \
  -DSYS_IOS=1 \
  -DHAVE_NEON=1 \
  -I"$SCRIPT_DIR" \
  -I"$PROJECT_ROOT/src" \
  -I"$PROJECT_ROOT/src/metal" \
  -c "$SCRIPT_DIR/magic_sr_plugin.cpp" \
  -o "$OBJ_DIR/magic_sr_plugin.o"

echo "[MagicSR Unity iOS] Creating static library..."
"$LIBTOOL" -static -o "$OUT_LIB" "$OBJ_DIR/magic_sr_plugin.o" "$CORE_LIB"

echo "[MagicSR Unity iOS] Done. Output: $OUT_LIB"
