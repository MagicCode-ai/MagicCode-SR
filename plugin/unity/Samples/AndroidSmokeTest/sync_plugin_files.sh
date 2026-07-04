#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
UNITY_PLUGIN_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
PROJECT_ROOT=$(cd "$UNITY_PLUGIN_ROOT/../.." && pwd)

"$UNITY_PLUGIN_ROOT/Native/build_android.sh"

mkdir -p "$SCRIPT_DIR/Assets/Scripts/MagicSR"
mkdir -p "$SCRIPT_DIR/Assets/Plugins/Android/arm64-v8a"
mkdir -p "$SCRIPT_DIR/Assets/StreamingAssets/MagicSRModels"

cp "$UNITY_PLUGIN_ROOT/CSharp/MagicSRNative.cs" "$SCRIPT_DIR/Assets/Scripts/MagicSR/MagicSRNative.cs"
cp "$UNITY_PLUGIN_ROOT/Native/build/libmagic_sr_unity.so" "$SCRIPT_DIR/Assets/Plugins/Android/arm64-v8a/libmagic_sr_unity.so"

LIBCXX_DEST="$SCRIPT_DIR/Assets/Plugins/Android/arm64-v8a/libc++_shared.so"
PROJECT_UNITY_VERSION=""
if [ -f "$SCRIPT_DIR/ProjectSettings/ProjectVersion.txt" ]; then
  PROJECT_UNITY_VERSION=$(awk '/m_EditorVersion:/{print $2; exit}' "$SCRIPT_DIR/ProjectSettings/ProjectVersion.txt")
fi
if [[ "${MAGIC_SR_COPY_LIBCXX:-}" == "1" || "${UNITY_BIN:-}" == *"/2022."* || "$PROJECT_UNITY_VERSION" == 2022.* ]]; then
  LIBCXX_SHARED=""
  for candidate in \
    "${ANDROID_NDK_HOME:-}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" \
    "${ANDROID_NDK_ROOT:-}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" \
    "${ANDROID_HOME:-}/ndk/25.1.8937393/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" \
    "${ANDROID_SDK_ROOT:-}/ndk/25.1.8937393/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" \
    "${ANDROID_HOME:-}/ndk/23.1.7779620/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" \
    "${ANDROID_SDK_ROOT:-}/ndk/23.1.7779620/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" \
    "/Users/joey/Library/Android/sdk/ndk/25.1.8937393/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" \
    "/Users/joey/Library/Android/sdk/ndk/23.1.7779620/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" \
    "/Applications/Unity/Hub/Editor/2022.3.62f2/PlaybackEngines/AndroidPlayer/NDK/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"; do
    if [ -f "$candidate" ]; then
      LIBCXX_SHARED="$candidate"
      break
    fi
  done

  if [ -n "$LIBCXX_SHARED" ]; then
    cp "$LIBCXX_SHARED" "$LIBCXX_DEST"
  else
    echo "[MagicSRUnitySmoke] Warning: libc++_shared.so not found; Unity 2022 may fail to load libmagic_sr_unity.so." >&2
  fi
else
  rm -f "$LIBCXX_DEST"
fi

if [ -n "${MAGIC_SR_MODEL_PATH:-}" ]; then
  if [ ! -f "$MAGIC_SR_MODEL_PATH" ]; then
    echo "[MagicSRUnitySmoke] MAGIC_SR_MODEL_PATH does not exist: $MAGIC_SR_MODEL_PATH" >&2
    exit 1
  fi
  cp "$MAGIC_SR_MODEL_PATH" "$SCRIPT_DIR/Assets/StreamingAssets/MagicSRModels/$(basename "$MAGIC_SR_MODEL_PATH")"
elif [ -f "$PROJECT_ROOT/model/magic_veryfastx2_cpu_params.bin" ]; then
  cp "$PROJECT_ROOT/model/magic_veryfastx2_cpu_params.bin" "$SCRIPT_DIR/Assets/StreamingAssets/MagicSRModels/"
elif [ -f "$PROJECT_ROOT/models/magic_veryfastx2_cpu_params.bin" ]; then
  cp "$PROJECT_ROOT/models/magic_veryfastx2_cpu_params.bin" "$SCRIPT_DIR/Assets/StreamingAssets/MagicSRModels/"
else
  echo "[MagicSRUnitySmoke] No CPU model copied. Set MAGIC_SR_MODEL_PATH=/path/to/magic_veryfastx2_cpu_params.bin before building."
fi

echo "[MagicSRUnitySmoke] Synced plugin files into $SCRIPT_DIR"
