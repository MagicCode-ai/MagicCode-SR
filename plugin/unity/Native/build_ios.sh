#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/../../.." && pwd)
BUILD_DIR="$SCRIPT_DIR/build_ios"
OBJ_DIR="$BUILD_DIR/obj"
OUT_LIB="$BUILD_DIR/libmagic_sr_unity_ios.a"

IOS_MIN_VERSION=${IOS_MIN_VERSION:-15.6}
IOS_SDK=${IOS_SDK:-iphoneos}

# Prefer combined Enable+core library so apps do not compile mc_enable themselves.
resolve_enable_lib() {
  local candidates=(
    "$PROJECT_ROOT/lib/ios/libmagic_sr_enable.a"
    "$PROJECT_ROOT/build/ipad/magic_sr/Release-iphoneos/libmagic_sr_enable.a"
    "$PROJECT_ROOT/../release/v1.1.0/lib/ios/libmagic_sr_enable.a"
    "$PROJECT_ROOT/../github/MagicCode-SR/lib/ios/libmagic_sr_enable.a"
  )
  local c
  for c in "${candidates[@]}"; do
    if [ -f "$c" ]; then
      echo "$c"
      return 0
    fi
  done
  return 1
}

ENABLE_LIB=$(resolve_enable_lib || true)
if [ -z "${ENABLE_LIB:-}" ]; then
  echo "[MagicSR Unity iOS] Missing libmagic_sr_enable.a. Run tools/build_enable_lib.sh ios first." >&2
  exit 1
fi

mkdir -p "$OBJ_DIR"

SDK_PATH=$(xcrun --sdk "$IOS_SDK" --show-sdk-path)
CXX=$(xcrun --sdk "$IOS_SDK" --find clang++)
LIBTOOL=$(xcrun --sdk "$IOS_SDK" --find libtool)

echo "[MagicSR Unity iOS] Using enable lib: $ENABLE_LIB"
echo "[MagicSR Unity iOS] Compiling Unity wrapper only (MC_Enable is inside libmagic_sr_enable.a)..."
"$CXX" \
  -arch arm64 \
  -isysroot "$SDK_PATH" \
  -miphoneos-version-min="$IOS_MIN_VERSION" \
  -std=c++17 \
  -fvisibility=hidden \
  -DSYS_IOS=1 \
  -DHAVE_NEON=1 \
  -I"$SCRIPT_DIR" \
  -I"$PROJECT_ROOT/interface" \
  -I"$PROJECT_ROOT/src" \
  -I"$PROJECT_ROOT/src/metal" \
  -c "$SCRIPT_DIR/magic_sr_plugin.cpp" \
  -o "$OBJ_DIR/magic_sr_plugin.o"

echo "[MagicSR Unity iOS] Creating static library..."
"$LIBTOOL" -static -o "$OUT_LIB" \
  "$OBJ_DIR/magic_sr_plugin.o" \
  "$ENABLE_LIB"

# Sanity: plugin .a must export MC_Enable (from enable lib) and MagicSR plugin symbols.
if ! nm -gU "$OUT_LIB" 2>/dev/null | grep -q 'T _MC_Enable'; then
  echo "[MagicSR Unity iOS] ERROR: MC_Enable missing in $OUT_LIB" >&2
  exit 1
fi

echo "[MagicSR Unity iOS] Done. Output: $OUT_LIB"
