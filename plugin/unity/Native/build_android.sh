#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/../../.." && pwd)

NDK_PATH=${ANDROID_NDK_HOME:-/Users/joey/Library/Android/sdk/ndk/29.0.14033849}
TOOLCHAIN="$NDK_PATH/build/cmake/android.toolchain.cmake"
BUILD_DIR="$SCRIPT_DIR/build"
STATIC_LIB="$PROJECT_ROOT/lib/android/libmagic_sr.a"

if [ ! -f "$STATIC_LIB" ]; then
  echo "[MagicSR Unity] Missing Android core library: $STATIC_LIB" >&2
  exit 1
fi

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DANDROID_STL=c++_shared

cmake --build "$BUILD_DIR" -j4

echo "[MagicSR Unity] Done. Output: $BUILD_DIR/libmagic_sr_unity.so"
