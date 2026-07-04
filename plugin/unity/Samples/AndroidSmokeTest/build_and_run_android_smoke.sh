#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/../../../.." && pwd)
APK_PATH="$SCRIPT_DIR/Builds/Android/MagicSRUnitySmoke.apk"
PACKAGE_NAME="com.magicsr.unitysmoke"
LOG_TAG="MagicSRUnitySmoke"
LOG_FILE="$SCRIPT_DIR/unity-smoke-logcat.txt"
IMAGE_OUTPUT_DIR="$SCRIPT_DIR/Saved/AndroidSmokeImages"
DEVICE_SMOKE_DIR="/sdcard/Android/data/$PACKAGE_NAME/files/MagicSRSmoke"

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

ADB_BIN=${ADB_BIN:-}
if [ -z "$ADB_BIN" ]; then
  if [ -n "${ANDROID_HOME:-}" ] && [ -x "$ANDROID_HOME/platform-tools/adb" ]; then
    ADB_BIN="$ANDROID_HOME/platform-tools/adb"
  elif [ -n "${ANDROID_SDK_ROOT:-}" ] && [ -x "$ANDROID_SDK_ROOT/platform-tools/adb" ]; then
    ADB_BIN="$ANDROID_SDK_ROOT/platform-tools/adb"
  elif [ -f "$PROJECT_ROOT/demo/android_demo/camera/local.properties" ]; then
    SDK_DIR=$(awk -F= '/^sdk.dir=/{print $2}' "$PROJECT_ROOT/demo/android_demo/camera/local.properties" | tail -n 1 | tr -d '\r')
    if [ -n "$SDK_DIR" ] && [ -x "$SDK_DIR/platform-tools/adb" ]; then
      ADB_BIN="$SDK_DIR/platform-tools/adb"
    fi
  fi
  if [ -z "$ADB_BIN" ] && command -v adb >/dev/null 2>&1; then
    ADB_BIN=$(command -v adb)
  fi
fi

if [ -z "$ADB_BIN" ] || [ ! -x "$ADB_BIN" ]; then
  echo "[MagicSRUnitySmoke] adb not found. Set ADB_BIN=/path/to/adb." >&2
  exit 1
fi

wait_for_adb() {
  "$ADB_BIN" start-server >/dev/null 2>&1 || true
  for _ in 1 2 3 4 5; do
    if "$ADB_BIN" get-state >/dev/null 2>&1; then
      return 0
    fi
    sleep 1
  done
  return 1
}

adb_retry() {
  local attempt
  for attempt in 1 2 3; do
    if "$ADB_BIN" "$@"; then
      return 0
    fi
    "$ADB_BIN" kill-server >/dev/null 2>&1 || true
    "$ADB_BIN" start-server >/dev/null 2>&1 || true
    sleep 1
  done
  return 1
}

collect_smoke_images() {
  rm -rf "$IMAGE_OUTPUT_DIR"
  mkdir -p "$IMAGE_OUTPUT_DIR"
  adb_retry pull "$DEVICE_SMOKE_DIR/input_64x64.pgm" "$IMAGE_OUTPUT_DIR/input_64x64.pgm" >/dev/null
  adb_retry pull "$DEVICE_SMOKE_DIR/output_128x128.pgm" "$IMAGE_OUTPUT_DIR/output_128x128.pgm" >/dev/null
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

if [ -z "$UNITY_BIN" ] || [ ! -x "$UNITY_BIN" ]; then
  echo "[MagicSRUnitySmoke] Unity executable not found. Set UNITY_BIN=/path/to/Unity." >&2
  "$SCRIPT_DIR/check_environment.sh" || true
  exit 1
fi
export UNITY_BIN

if ! wait_for_adb; then
  echo "[MagicSRUnitySmoke] No connected Android device." >&2
  "$ADB_BIN" devices || true
  exit 1
fi

export SKIP_JDK_VERSION_CHECK=1

"$SCRIPT_DIR/sync_plugin_files.sh"

if ! "$UNITY_BIN" \
  -batchmode \
  -quit \
  -projectPath "$SCRIPT_DIR" \
  -executeMethod MagicSR.UnityPlugin.Samples.Editor.MagicSRSmokeBuild.BuildAndroid \
  -logFile "$SCRIPT_DIR/unity-build.log"; then
  if rg -q "No valid Unity Editor license found|License is not active|No ULF license found|Token not found" "$SCRIPT_DIR/unity-build.log"; then
    echo "[MagicSRUnitySmoke] Unity license is not active. Open Unity Hub, sign in, and activate a Personal/Pro license, then rerun this script." >&2
  else
    echo "[MagicSRUnitySmoke] Unity build failed. See $SCRIPT_DIR/unity-build.log" >&2
  fi
  exit 1
fi

if [ ! -f "$APK_PATH" ]; then
  echo "[MagicSRUnitySmoke] APK not found: $APK_PATH" >&2
  exit 1
fi

if ! wait_for_adb; then
  echo "[MagicSRUnitySmoke] Android device disappeared after Unity build." >&2
  "$ADB_BIN" devices || true
  exit 1
fi

adb_retry install -r "$APK_PATH"
adb_retry shell rm -rf "$DEVICE_SMOKE_DIR"
adb_retry shell mkdir -p "$DEVICE_SMOKE_DIR"
adb_retry logcat -c
adb_retry shell monkey -p "$PACKAGE_NAME" -c android.intent.category.LAUNCHER 1

deadline=$((SECONDS + 45))
rm -f "$LOG_FILE"
touch "$LOG_FILE"

echo "[MagicSRUnitySmoke] Waiting for PASS in logcat..."
while [ "$SECONDS" -lt "$deadline" ]; do
  "$ADB_BIN" logcat -d -s Unity ActivityManager > "$LOG_FILE" || true
  if rg -q "\\[$LOG_TAG\\] result=PASS" "$LOG_FILE"; then
    collect_smoke_images
    echo "[MagicSRUnitySmoke] PASS"
    exit 0
  fi
  if rg -q "\\[$LOG_TAG\\] result=FAIL" "$LOG_FILE"; then
    echo "[MagicSRUnitySmoke] FAIL. See $LOG_FILE" >&2
    exit 1
  fi
  sleep 2
done

echo "[MagicSRUnitySmoke] Timed out waiting for result. See $LOG_FILE" >&2
exit 1
