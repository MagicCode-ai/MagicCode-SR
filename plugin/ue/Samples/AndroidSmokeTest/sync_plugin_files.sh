#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
UE_PLUGIN_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
PROJECT_ROOT=$(cd "$UE_PLUGIN_ROOT/../.." && pwd)

mkdir -p "$SCRIPT_DIR/Plugins"
rm -rf "$SCRIPT_DIR/Plugins/MagicSR"
cp -R "$UE_PLUGIN_ROOT/MagicSR" "$SCRIPT_DIR/Plugins/MagicSR"

mkdir -p "$SCRIPT_DIR/Content/MagicSRModels"
if [ -n "${MAGIC_SR_MODEL_PATH:-}" ]; then
  if [ ! -f "$MAGIC_SR_MODEL_PATH" ]; then
    echo "[MagicSRUESmoke] MAGIC_SR_MODEL_PATH does not exist: $MAGIC_SR_MODEL_PATH" >&2
    exit 1
  fi
  cp "$MAGIC_SR_MODEL_PATH" "$SCRIPT_DIR/Content/MagicSRModels/$(basename "$MAGIC_SR_MODEL_PATH")"
elif [ -f "$PROJECT_ROOT/model/magic_veryfastx2_cpu_params.bin" ]; then
  cp "$PROJECT_ROOT/model/magic_veryfastx2_cpu_params.bin" "$SCRIPT_DIR/Content/MagicSRModels/"
elif [ -f "$PROJECT_ROOT/models/magic_veryfastx2_cpu_params.bin" ]; then
  cp "$PROJECT_ROOT/models/magic_veryfastx2_cpu_params.bin" "$SCRIPT_DIR/Content/MagicSRModels/"
else
  echo "[MagicSRUESmoke] CPU model not found; continuing with GPU/GLES backend smoke only."
fi

for model_name in magic_veryfast_gpu_params.bin magic_veryfast_gles_params.bin; do
  if [ -f "$PROJECT_ROOT/model/$model_name" ]; then
    cp "$PROJECT_ROOT/model/$model_name" "$SCRIPT_DIR/Content/MagicSRModels/"
  elif [ -f "$PROJECT_ROOT/models/$model_name" ]; then
    cp "$PROJECT_ROOT/models/$model_name" "$SCRIPT_DIR/Content/MagicSRModels/"
  else
    echo "[MagicSRUESmoke] Missing GPU model $model_name under $PROJECT_ROOT/model or $PROJECT_ROOT/models." >&2
    exit 1
  fi
done

echo "[MagicSRUESmoke] Synced UE plugin and model into $SCRIPT_DIR"
