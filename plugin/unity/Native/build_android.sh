#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/../../.." && pwd)

NDK_PATH=${ANDROID_NDK_HOME:-/Users/joey/Library/Android/sdk/ndk/29.0.14033849}
TOOLCHAIN="$NDK_PATH/build/cmake/android.toolchain.cmake"
BUILD_DIR="$SCRIPT_DIR/build"
RELEASE_STATIC_LIB="$PROJECT_ROOT/../release/v1.1.0/lib/android/libmagic_sr.a"
LEGACY_STATIC_LIB="$PROJECT_ROOT/build/android/build/libmagic_sr.a"
STATIC_LIB="$RELEASE_STATIC_LIB"

if [ ! -f "$STATIC_LIB" ]; then
  STATIC_LIB="$LEGACY_STATIC_LIB"
fi

if [ ! -f "$STATIC_LIB" ]; then
  echo "[MagicSR Unity] libmagic_sr.a missing at release and legacy paths, building core library first..."
  "$PROJECT_ROOT/build/android/android_build.sh"
fi

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DANDROID_STL=c++_shared

cmake --build "$BUILD_DIR" -j4

echo "[MagicSR Unity] Done. Output: $BUILD_DIR/libmagic_sr_unity.so"
