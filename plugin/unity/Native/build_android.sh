#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/../../.." && pwd)

NDK_PATH=${ANDROID_NDK_HOME:-/Users/joey/Library/Android/sdk/ndk/29.0.14033849}
TOOLCHAIN="$NDK_PATH/build/cmake/android.toolchain.cmake"
BUILD_DIR="$SCRIPT_DIR/build"
SHIPPED_ENABLE_LIB="$PROJECT_ROOT/lib/android/libmagic_sr_enable.a"
BUILD_ENABLE_LIB="$PROJECT_ROOT/build/android/build/libmagic_sr_enable.a"
RELEASE_ENABLE_LIB="$PROJECT_ROOT/../release/v1.1.0/lib/android/libmagic_sr_enable.a"

STATIC_LIB=""
for candidate in "$SHIPPED_ENABLE_LIB" "$BUILD_ENABLE_LIB" "$RELEASE_ENABLE_LIB"; do
  if [ -f "$candidate" ]; then
    STATIC_LIB="$candidate"
    break
  fi
done

if [ -z "$STATIC_LIB" ]; then
  echo "[MagicSR Unity] libmagic_sr_enable.a missing; building enable lib..."
  if [ -x "$PROJECT_ROOT/tools/build_enable_lib.sh" ]; then
    bash "$PROJECT_ROOT/tools/build_enable_lib.sh" android
  else
    echo "[MagicSR Unity] ERROR: tools/build_enable_lib.sh not found" >&2
    exit 1
  fi
  STATIC_LIB="$SHIPPED_ENABLE_LIB"
fi

if [ ! -f "$STATIC_LIB" ]; then
  echo "[MagicSR Unity] ERROR: still missing libmagic_sr_enable.a" >&2
  exit 1
fi

echo "[MagicSR Unity] linking $STATIC_LIB"

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DANDROID_STL=c++_shared

cmake --build "$BUILD_DIR" -j4

echo "[MagicSR Unity] Done. Output: $BUILD_DIR/libmagic_sr_unity.so"
