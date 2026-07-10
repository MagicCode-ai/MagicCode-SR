#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

find_run_uat() {
  if [ -n "${UE_ROOT:-}" ] && [ -x "$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" ]; then
    echo "$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
    return 0
  fi

  for root in \
    "$HOME/UnrealEngine" \
    "$HOME/Epic/UE_5.4" \
    "$HOME/Epic/UE_5.3" \
    "$HOME/Epic/UE_5.2" \
    "/Users/Shared/Epic Games/UE_5.4" \
    "/Users/Shared/Epic Games/UE_5.3" \
    "/Users/Shared/Epic Games/UE_5.2"; do
    if [ -x "$root/Engine/Build/BatchFiles/RunUAT.sh" ]; then
      echo "$root/Engine/Build/BatchFiles/RunUAT.sh"
      return 0
    fi
  done

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
  if [ -x "/Users/joey/Library/Android/sdk/platform-tools/adb" ]; then
    echo "/Users/joey/Library/Android/sdk/platform-tools/adb"
    return 0
  fi
  if command -v adb >/dev/null 2>&1; then
    command -v adb
    return 0
  fi
  return 1
}

echo "[MagicSRUESmoke] project=$SCRIPT_DIR"

if RUN_UAT=$(find_run_uat); then
  echo "[MagicSRUESmoke] RunUAT=$RUN_UAT"
else
  echo "[MagicSRUESmoke] RunUAT not found. Install Unreal Engine and set UE_ROOT=/path/to/UE_x.y." >&2
fi

if ADB=$(find_adb); then
  echo "[MagicSRUESmoke] adb=$ADB"
  "$ADB" devices || true
else
  echo "[MagicSRUESmoke] adb not found. Set ADB_BIN=/path/to/adb." >&2
fi

if [ -f "$SCRIPT_DIR/Plugins/MagicSR/MagicSR.uplugin" ]; then
  echo "[MagicSRUESmoke] plugin=synced"
else
  echo "[MagicSRUESmoke] plugin=missing; run ./sync_plugin_files.sh"
fi

if [ -f "$SCRIPT_DIR/Content/MagicSRModels/magic_veryfastx2_cpu_params.bin" ]; then
  echo "[MagicSRUESmoke] model=present"
else
  echo "[MagicSRUESmoke] model=missing; run ./sync_plugin_files.sh"
fi
