#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
OUT_LOG="$SCRIPT_DIR/unity_runtime_log.txt"

echo "[runtime] collecting Unity/MagicSR logcat..."
adb logcat -d | rg "MagicSR|Unity" > "$OUT_LOG" || true
echo "[runtime] log saved: $OUT_LOG"
