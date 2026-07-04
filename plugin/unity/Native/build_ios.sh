#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/../../.." && pwd)
BUILD_DIR="$SCRIPT_DIR/build_ios"
OBJ_DIR="$BUILD_DIR/obj"
OUT_LIB="$BUILD_DIR/libmagic_sr_unity_ios.a"

IOS_MIN_VERSION=${IOS_MIN_VERSION:-15.6}
IOS_SDK=${IOS_SDK:-iphoneos}

CORE_LIB="$PROJECT_ROOT/lib/ios/libmagic_sr.a"

COMMON_HEADER_SEARCH_PATHS="$PROJECT_ROOT/header"
COMMON_DEFINES="SYS_IOS=1 HAVE_NEON=1"

mkdir -p "$OBJ_DIR"

if [ ! -f "$CORE_LIB" ]; then
  echo "[MagicSR Unity iOS] Missing iOS core library: $CORE_LIB" >&2
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
  -I"$PROJECT_ROOT/header" \
  -c "$SCRIPT_DIR/magic_sr_plugin.cpp" \
  -o "$OBJ_DIR/magic_sr_plugin.o"

echo "[MagicSR Unity iOS] Creating static library..."
"$LIBTOOL" -static -o "$OUT_LIB" "$OBJ_DIR/magic_sr_plugin.o" "$CORE_LIB"

echo "[MagicSR Unity iOS] Done. Output: $OUT_LIB"
