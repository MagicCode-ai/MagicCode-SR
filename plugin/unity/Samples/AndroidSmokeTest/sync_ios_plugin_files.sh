#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
UNITY_PLUGIN_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
PROJECT_ROOT=$(cd "$UNITY_PLUGIN_ROOT/../.." && pwd)

"$UNITY_PLUGIN_ROOT/Native/build_ios.sh"

mkdir -p "$SCRIPT_DIR/Assets/Scripts/MagicSR"
mkdir -p "$SCRIPT_DIR/Assets/Plugins/iOS"
mkdir -p "$SCRIPT_DIR/Assets/StreamingAssets/MagicSRModels"

cp "$UNITY_PLUGIN_ROOT/CSharp/MagicSRNative.cs" "$SCRIPT_DIR/Assets/Scripts/MagicSR/MagicSRNative.cs"
cp "$UNITY_PLUGIN_ROOT/Native/build_ios/libmagic_sr_unity_ios.a" "$SCRIPT_DIR/Assets/Plugins/iOS/libmagic_sr_unity_ios.a"

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

if [ -f "$PROJECT_ROOT/model/magic_veryfast_gpu_params.bin" ]; then
  cp "$PROJECT_ROOT/model/magic_veryfast_gpu_params.bin" "$SCRIPT_DIR/Assets/StreamingAssets/MagicSRModels/"
elif [ -f "$PROJECT_ROOT/models/magic_veryfast_gpu_params.bin" ]; then
  cp "$PROJECT_ROOT/models/magic_veryfast_gpu_params.bin" "$SCRIPT_DIR/Assets/StreamingAssets/MagicSRModels/"
else
  echo "[MagicSRUnitySmoke] No Metal GPU model copied. Set MAGIC_SR_MODEL_PATH=/path/to/magic_veryfast_gpu_params.bin for Metal-only builds."
fi

echo "[MagicSRUnitySmoke] Synced iOS plugin files into $SCRIPT_DIR"
