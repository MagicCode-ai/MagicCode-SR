#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/../../../.." && pwd)

find_unity() {
  if [ -n "${UNITY_BIN:-}" ] && [ -x "$UNITY_BIN" ]; then
    echo "$UNITY_BIN"
    return 0
  fi

  local hub_dir="/Applications/Unity/Hub/Editor"
  if [ -d "$hub_dir" ]; then
    local candidate
    for candidate in "$hub_dir"/*/Unity.app/Contents/MacOS/Unity; do
      if [ -x "$candidate" ]; then
        echo "$candidate"
        return 0
      fi
    done
  fi

  if [ -x "/Applications/Unity/Unity.app/Contents/MacOS/Unity" ]; then
    echo "/Applications/Unity/Unity.app/Contents/MacOS/Unity"
    return 0
  fi

  return 1
}

find_adb() {
  if [ -n "${ADB_BIN:-}" ] && [ -x "$ADB_BIN" ]; then
    echo "$ADB_BIN"
    return 0
  fi

  if [ -n "${ANDROID_HOME:-}" ] && [ -x "$ANDROID_HOME/platform-tools/adb" ]; then
    echo "$ANDROID_HOME/platform-tools/adb"
    return 0
  fi

  if [ -n "${ANDROID_SDK_ROOT:-}" ] && [ -x "$ANDROID_SDK_ROOT/platform-tools/adb" ]; then
    echo "$ANDROID_SDK_ROOT/platform-tools/adb"
    return 0
  fi

  local local_props="$PROJECT_ROOT/demo/android_demo/camera/local.properties"
  if [ -f "$local_props" ]; then
    local sdk_dir
    sdk_dir=$(awk -F= '/^sdk.dir=/{print $2}' "$local_props" | tail -n 1 | tr -d '\r')
    if [ -n "$sdk_dir" ] && [ -x "$sdk_dir/platform-tools/adb" ]; then
      echo "$sdk_dir/platform-tools/adb"
      return 0
    fi
  fi

  if command -v adb >/dev/null 2>&1; then
    command -v adb
    return 0
  fi

  return 1
}

echo "[MagicSRUnitySmoke] project=$SCRIPT_DIR"

if unity_bin=$(find_unity); then
  echo "[MagicSRUnitySmoke] unity=$unity_bin"
else
  echo "[MagicSRUnitySmoke] unity=MISSING set UNITY_BIN=/path/to/Unity" >&2
fi

if adb_bin=$(find_adb); then
  echo "[MagicSRUnitySmoke] adb=$adb_bin"
  "$adb_bin" devices
else
  echo "[MagicSRUnitySmoke] adb=MISSING set ADB_BIN=/path/to/adb" >&2
fi

if [ -f "$SCRIPT_DIR/Assets/Plugins/Android/arm64-v8a/libmagic_sr_unity.so" ]; then
  echo "[MagicSRUnitySmoke] plugin_so=OK"
else
  echo "[MagicSRUnitySmoke] plugin_so=MISSING run ./sync_plugin_files.sh" >&2
fi

if ls "$SCRIPT_DIR/Assets/StreamingAssets/MagicSRModels/"*.bin >/dev/null 2>&1; then
  echo "[MagicSRUnitySmoke] streaming_models=OK"
else
  echo "[MagicSRUnitySmoke] streaming_models=MISSING set MAGIC_SR_MODEL_PATH and run ./sync_plugin_files.sh" >&2
fi
