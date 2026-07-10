#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_FILE="$SCRIPT_DIR/MagicSRUESmoke.uproject"
ARCHIVE_DIR="$SCRIPT_DIR/Saved/iOSSmokeArchive"
UAT_LOG="$SCRIPT_DIR/ue-smoke-ios-uat.log"
LOG_FILE="$SCRIPT_DIR/ue-smoke-ios-console.txt"
IMAGE_OUTPUT_DIR="$SCRIPT_DIR/Saved/iOSSmokeImages"
PACKAGE_NAME="com.magicsr.uesmoke"
LOG_TAG="MagicSRUESmoke"

find_run_uat() {
  if [ -n "${RUN_UAT:-}" ] && [ -x "$RUN_UAT" ]; then
    echo "$RUN_UAT"
    return 0
  fi
  if [ -n "${UE_ROOT:-}" ] && [ -x "$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" ]; then
    echo "$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
    return 0
  fi
  for root in \
    "$HOME/UnrealEngine" \
    "/Users/Shared/Epic Games/UE_5.8" \
    "/Users/Shared/Epic Games/UE_5.7" \
    "/Users/Shared/Epic Games/UE_5.6" \
    "/Users/Shared/Epic Games/UE_5.4" \
    "/Users/Shared/Epic Games/UE_5.3"; do
    if [ -x "$root/Engine/Build/BatchFiles/RunUAT.sh" ]; then
      echo "$root/Engine/Build/BatchFiles/RunUAT.sh"
      return 0
    fi
  done
  return 1
}

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
    if hardware.get("platform") == "iOS" and hardware.get("reality") == "physical" and connection.get("tunnelState") == "connected":
        print(hardware.get("udid") or device.get("identifier"))
        raise SystemExit(0)
raise SystemExit("No connected physical iOS device found. Set IOS_DEVICE_ID=<udid>.")
PY
}

convert_smoke_images() {
  python3 - "$IMAGE_OUTPUT_DIR" <<'PY'
import struct
import sys
import zlib
from pathlib import Path

def read_pgm(path):
    with open(path, "rb") as f:
        if f.readline().strip() != b"P5":
            raise SystemExit(f"{path}: expected P5 PGM")
        line = f.readline()
        while line.startswith(b"#"):
            line = f.readline()
        width, height = map(int, line.split())
        if int(f.readline()) != 255:
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
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)

image_dir = Path(sys.argv[1])
for kind, expected_size in (("input", (64, 64)), ("output", (128, 128))):
    src = image_dir / f"{kind}_metal_{expected_size[0]}x{expected_size[1]}.pgm"
    dst = src.with_suffix(".png")
    width, height, data = read_pgm(src)
    write_gray_png(dst, width, height, data)
    nonzero = sum(1 for value in data if value)
    print(f"[MagicSRUESmoke] image {dst} {width}x{height} nonZero={nonzero}")
    if (width, height) != expected_size:
        raise SystemExit(f"{src}: unexpected dimensions {width}x{height}")
    if kind == "output" and nonzero == 0:
        raise SystemExit(f"{src}: output image is all zero")
PY
}

RUN_UAT_BIN=$(find_run_uat || true)
if [ -z "$RUN_UAT_BIN" ]; then
  echo "[MagicSRUESmoke] Unreal Automation Tool not found. Set UE_ROOT=/path/to/UE_x.y." >&2
  exit 1
fi

IOS_DEVICE_ID=${IOS_DEVICE_ID:-}
if [ -z "$IOS_DEVICE_ID" ]; then
  IOS_DEVICE_ID=$(find_ios_device)
fi

"$SCRIPT_DIR/sync_plugin_files.sh"
rm -rf "$ARCHIVE_DIR"

set +e
"$RUN_UAT_BIN" BuildCookRun \
  -project="$PROJECT_FILE" \
  -noP4 \
  -platform=IOS \
  -clientconfig=Development \
  -serverconfig=Development \
  -map=/Engine/Maps/Entry \
  -build \
  -cook \
  -stage \
  -pak \
  -package \
  -archive \
  -archivedirectory="$ARCHIVE_DIR" \
  -utf8output 2>&1 | tee "$UAT_LOG"
UAT_EXIT=${PIPESTATUS[0]}
set -e

if [ "$UAT_EXIT" -ne 0 ]; then
  if python3 - "$UAT_LOG" <<'PY'
import sys
text = open(sys.argv[1], errors="ignore").read()
needles = ("Platform IOS is not a valid platform", "SDK is installed properly", "buildable: False")
raise SystemExit(0 if any(n in text for n in needles) else 1)
PY
  then
    echo "[MagicSRUESmoke] UE iOS build is blocked by the installed UE/Xcode SDK combination. UE 5.3 requires a compatible Xcode 14.1-15.x Apple SDK; install Xcode 15 and select it with xcode-select, then rerun this script." >&2
  fi
  exit "$UAT_EXIT"
fi

apps=("$ARCHIVE_DIR"/*.app "$SCRIPT_DIR"/Binaries/IOS/*.app)
APP_PATH=""
for candidate in "${apps[@]}"; do
  if [ -d "$candidate" ]; then
    APP_PATH="$candidate"
    break
  fi
done

if [ -z "$APP_PATH" ]; then
  echo "[MagicSRUESmoke] Built .app not found." >&2
  exit 1
fi

# Packaged iOS builds can inherit a relative .uproject path that is invalid
# inside the app sandbox. The smoke app only needs a flag to trigger the plugin.
printf '%s\n' '-MagicSRUESmoke' > "$APP_PATH/uecommandline.txt"
cp "$SCRIPT_DIR/Content/MagicSRModels/magic_veryfast_gpu_params.bin" "$APP_PATH/magic_veryfast_gpu_params.bin"
SIGN_IDENTITY=${MAGIC_SR_IOS_CODESIGN_IDENTITY:-}
if [ -z "$SIGN_IDENTITY" ]; then
  SIGN_IDENTITY=$(python3 - "$UAT_LOG" <<'PY'
import re
import sys
text = open(sys.argv[1], errors="ignore").read()
matches = re.findall(r'/usr/bin/codesign --force --sign ([0-9A-F]{40})\b', text)
if matches:
    print(matches[-1])
PY
)
fi
if [ -z "$SIGN_IDENTITY" ]; then
  SIGN_IDENTITY=$(python3 - <<'PY'
import re
import subprocess
text = subprocess.check_output(["security", "find-identity", "-v", "-p", "codesigning"], text=True)
for line in text.splitlines():
    if "Apple Development" in line:
        m = re.search(r'\b([0-9A-F]{40})\b', line)
        if m:
            print(m.group(1))
            raise SystemExit(0)
raise SystemExit(1)
PY
)
fi
/usr/bin/codesign --force --sign "$SIGN_IDENTITY" --preserve-metadata=identifier,entitlements,flags "$APP_PATH"

xcrun devicectl device install app --device "$IOS_DEVICE_ID" "$APP_PATH"
mkdir -p "$SCRIPT_DIR/.ios_model_push/MagicSRModels"
cp "$SCRIPT_DIR/Content/MagicSRModels/magic_veryfast_gpu_params.bin" "$SCRIPT_DIR/.ios_model_push/MagicSRModels/"
xcrun devicectl device copy to --device "$IOS_DEVICE_ID" --domain-type appDataContainer --domain-identifier "$PACKAGE_NAME" --source "$SCRIPT_DIR/.ios_model_push/MagicSRModels" --destination "Documents/MagicSRModels" >/dev/null

rm -f "$LOG_FILE"
set +e
xcrun devicectl device process launch --device "$IOS_DEVICE_ID" --terminate-existing --console "$PACKAGE_NAME" > "$LOG_FILE" 2>&1
set -e

if ! python3 - "$LOG_FILE" "$LOG_TAG" <<'PY'
import sys
text = open(sys.argv[1], errors="ignore").read()
raise SystemExit(0 if f"[{sys.argv[2]}] result=PASS" in text else 1)
PY
then
  echo "[MagicSRUESmoke] FAIL or timeout. See $LOG_FILE" >&2
  exit 1
fi

rm -rf "$IMAGE_OUTPUT_DIR"
mkdir -p "$IMAGE_OUTPUT_DIR"
xcrun devicectl device copy from --device "$IOS_DEVICE_ID" --domain-type appDataContainer --domain-identifier "$PACKAGE_NAME" --source "Documents/MagicSRSmoke/input_metal_64x64.pgm" --destination "$IMAGE_OUTPUT_DIR/input_metal_64x64.pgm" >/dev/null
xcrun devicectl device copy from --device "$IOS_DEVICE_ID" --domain-type appDataContainer --domain-identifier "$PACKAGE_NAME" --source "Documents/MagicSRSmoke/output_metal_128x128.pgm" --destination "$IMAGE_OUTPUT_DIR/output_metal_128x128.pgm" >/dev/null
convert_smoke_images
echo "[MagicSRUESmoke] PASS"
