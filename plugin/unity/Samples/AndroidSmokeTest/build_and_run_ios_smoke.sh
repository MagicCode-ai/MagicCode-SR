#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PACKAGE_NAME="com.magicsr.unitysmoke"
LOG_TAG="MagicSRUnitySmoke"
XCODE_PROJECT_DIR="$SCRIPT_DIR/Builds/iOS/MagicSRUnitySmoke"
XCODE_PROJECT="$XCODE_PROJECT_DIR/Unity-iPhone.xcodeproj"
XCODE_BUILD_ROOT="$SCRIPT_DIR/Builds/iOS/XcodeBuild"
APP_NAME="MagicSRUnitySmoke.app"
APP_PATH="$XCODE_BUILD_ROOT/Debug-iphoneos/$APP_NAME"
LOG_FILE="$SCRIPT_DIR/unity-ios-smoke-console.txt"
IMAGE_OUTPUT_DIR="$SCRIPT_DIR/Saved/iOSSmokeImages"

UNITY_BIN=${UNITY_BIN:-}
if [ -z "$UNITY_BIN" ]; then
  HUB_DIR="/Applications/Unity/Hub/Editor"
  if [ -d "$HUB_DIR" ]; then
    for candidate in "$HUB_DIR"/*/Unity.app/Contents/MacOS/Unity; do
      if [ -x "$candidate" ]; then
        UNITY_BIN="$candidate"
        break
      fi
    done
  fi
  if [ -z "$UNITY_BIN" ] && [ -x "/Applications/Unity/Unity.app/Contents/MacOS/Unity" ]; then
    UNITY_BIN="/Applications/Unity/Unity.app/Contents/MacOS/Unity"
  fi
fi

if [ -z "$UNITY_BIN" ] || [ ! -x "$UNITY_BIN" ]; then
  echo "[MagicSRUnitySmoke] Unity executable not found. Set UNITY_BIN=/path/to/Unity." >&2
  exit 1
fi
export UNITY_BIN

find_ios_device() {
  local json_path="$SCRIPT_DIR/devicectl-devices.json"
  xcrun devicectl list devices --json-output "$json_path" >/dev/null
  python3 - "$json_path" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as f:
    data = json.load(f)

for device in data.get("result", {}).get("devices", []):
    hardware = device.get("hardwareProperties", {})
    connection = device.get("connectionProperties", {})
    if hardware.get("platform") != "iOS":
        continue
    if hardware.get("reality") != "physical":
        continue
    if connection.get("pairingState") not in (None, "paired"):
        continue
    if connection.get("tunnelState") not in (None, "connected"):
        continue
    print(hardware.get("udid") or device.get("identifier"))
    raise SystemExit(0)

raise SystemExit("No paired physical iOS device found. Set IOS_DEVICE_ID=<udid>.")
PY
}

IOS_DEVICE_ID=${IOS_DEVICE_ID:-}
if [ -z "$IOS_DEVICE_ID" ]; then
  IOS_DEVICE_ID=$(find_ios_device)
fi

if [ -z "$IOS_DEVICE_ID" ]; then
  echo "[MagicSRUnitySmoke] No iOS device found. Set IOS_DEVICE_ID=<udid>." >&2
  exit 1
fi

collect_smoke_images() {
  rm -rf "$IMAGE_OUTPUT_DIR"
  mkdir -p "$IMAGE_OUTPUT_DIR"
  xcrun devicectl device copy from \
    --device "$IOS_DEVICE_ID" \
    --domain-type appDataContainer \
    --domain-identifier "$PACKAGE_NAME" \
    --source "Documents/MagicSRSmoke/input_64x64.pgm" \
    --destination "$IMAGE_OUTPUT_DIR/input_64x64.pgm" >/dev/null
  xcrun devicectl device copy from \
    --device "$IOS_DEVICE_ID" \
    --domain-type appDataContainer \
    --domain-identifier "$PACKAGE_NAME" \
    --source "Documents/MagicSRSmoke/output_128x128.pgm" \
    --destination "$IMAGE_OUTPUT_DIR/output_128x128.pgm" >/dev/null
  python3 - "$IMAGE_OUTPUT_DIR/input_64x64.pgm" "$IMAGE_OUTPUT_DIR/input_64x64.png" "$IMAGE_OUTPUT_DIR/output_128x128.pgm" "$IMAGE_OUTPUT_DIR/output_128x128.png" <<'PY'
import struct
import sys
import zlib


def read_pgm(path):
    with open(path, "rb") as f:
        magic = f.readline().strip()
        if magic != b"P5":
            raise SystemExit(f"{path}: expected P5 PGM")
        line = f.readline()
        while line.startswith(b"#"):
            line = f.readline()
        width, height = map(int, line.split())
        max_value = int(f.readline())
        if max_value != 255:
            raise SystemExit(f"{path}: expected max value 255")
        data = f.read()
    if len(data) != width * height:
        raise SystemExit(f"{path}: expected {width * height} bytes, got {len(data)}")
    return width, height, data


def write_gray_png(path, width, height, data):
    def chunk(kind, payload):
        body = kind + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + data[row * width : (row + 1) * width] for row in range(height))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


input_pgm, input_png, output_pgm, output_png = sys.argv[1:]
for src, dst in ((input_pgm, input_png), (output_pgm, output_png)):
    width, height, data = read_pgm(src)
    write_gray_png(dst, width, height, data)
    nonzero = sum(1 for value in data if value)
    print(f"[MagicSRUnitySmoke] image {dst} {width}x{height} nonZero={nonzero}")
    if src == output_pgm and (width, height) != (128, 128):
        raise SystemExit(f"{src}: unexpected dimensions {width}x{height}")
    if src == output_pgm and nonzero == 0:
        raise SystemExit(f"{src}: output image is all zero")
PY
}

"$SCRIPT_DIR/sync_ios_plugin_files.sh"

if ! "$UNITY_BIN" \
  -batchmode \
  -quit \
  -projectPath "$SCRIPT_DIR" \
  -executeMethod MagicSR.UnityPlugin.Samples.Editor.MagicSRSmokeBuild.BuildIOS \
  -logFile "$SCRIPT_DIR/unity-ios-build.log"; then
  if python3 - "$SCRIPT_DIR/unity-ios-build.log" <<'PY'
import sys
text = open(sys.argv[1], errors="ignore").read()
needles = ("No valid Unity Editor license found", "License is not active", "No ULF license found", "Token not found")
raise SystemExit(0 if any(n in text for n in needles) else 1)
PY
  then
    echo "[MagicSRUnitySmoke] Unity license is not active. Open Unity Hub, sign in, and activate a license, then rerun this script." >&2
  else
    echo "[MagicSRUnitySmoke] Unity iOS build failed. See $SCRIPT_DIR/unity-ios-build.log" >&2
  fi
  exit 1
fi

if [ ! -d "$XCODE_PROJECT" ]; then
  echo "[MagicSRUnitySmoke] Xcode project not found: $XCODE_PROJECT" >&2
  exit 1
fi

rm -rf "$XCODE_BUILD_ROOT"
mkdir -p "$XCODE_BUILD_ROOT"

XCODEBUILD_ARGS=(
  -project "$XCODE_PROJECT"
  -scheme "Unity-iPhone"
  -configuration Debug
  -sdk iphoneos
  -destination "id=$IOS_DEVICE_ID"
  SYMROOT="$XCODE_BUILD_ROOT"
  -allowProvisioningUpdates
)

if [ -n "${MAGIC_SR_IOS_TEAM_ID:-}" ]; then
  XCODEBUILD_ARGS+=(DEVELOPMENT_TEAM="$MAGIC_SR_IOS_TEAM_ID")
fi

xcodebuild "${XCODEBUILD_ARGS[@]}" build

if [ ! -d "$APP_PATH" ]; then
  apps=("$XCODE_BUILD_ROOT/Debug-iphoneos/"*.app)
  if [ "${#apps[@]}" -gt 0 ] && [ -d "${apps[0]}" ]; then
    APP_PATH="${apps[0]}"
  fi
fi

if [ ! -d "$APP_PATH" ]; then
  echo "[MagicSRUnitySmoke] Built .app not found under $XCODE_BUILD_ROOT/Debug-iphoneos" >&2
  exit 1
fi

xcrun devicectl device install app --device "$IOS_DEVICE_ID" "$APP_PATH"

rm -f "$LOG_FILE"
set +e
xcrun devicectl device process launch \
  --device "$IOS_DEVICE_ID" \
  --terminate-existing \
  --console \
  "$PACKAGE_NAME" \
  -- \
  --magic-sr-backend metal \
  --magic-sr-model magic_veryfast_gpu_params.bin > "$LOG_FILE" 2>&1
LAUNCH_EXIT=$?
set -e

if python3 - "$LOG_FILE" "$LOG_TAG" <<'PY'
import sys
text = open(sys.argv[1], errors="ignore").read()
tag = sys.argv[2]
raise SystemExit(0 if f"[{tag}] result=PASS" in text else 1)
PY
then
  collect_smoke_images
  echo "[MagicSRUnitySmoke] PASS"
  exit 0
fi

if python3 - "$LOG_FILE" "$LOG_TAG" <<'PY'
import sys
text = open(sys.argv[1], errors="ignore").read()
tag = sys.argv[2]
raise SystemExit(0 if f"[{tag}] result=FAIL" in text else 1)
PY
then
  echo "[MagicSRUnitySmoke] FAIL. See $LOG_FILE" >&2
  exit 1
fi

echo "[MagicSRUnitySmoke] Timed out or exited without PASS/FAIL (exit=$LAUNCH_EXIT). See $LOG_FILE" >&2
exit 1
