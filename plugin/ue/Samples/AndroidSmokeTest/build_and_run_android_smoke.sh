#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_FILE="$SCRIPT_DIR/MagicSRUESmoke.uproject"
ARCHIVE_DIR="$SCRIPT_DIR/Saved/AndroidSmokeArchive"
LOG_FILE="$SCRIPT_DIR/ue-smoke-logcat.txt"
UAT_LOG="$SCRIPT_DIR/ue-smoke-uat.log"
IMAGE_OUTPUT_DIR="$SCRIPT_DIR/Saved/AndroidSmokeImages"
PACKAGE_NAME="com.magicsr.uesmoke"
LOG_TAG="MagicSRUESmoke"
DEVICE_SMOKE_DIR="/sdcard/Android/data/$PACKAGE_NAME/files/MagicSRSmoke"

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
    "$HOME/Epic/UE_5.8" \
    "$HOME/Epic/UE_5.7" \
    "$HOME/Epic/UE_5.6" \
    "$HOME/Epic/UE_5.4" \
    "$HOME/Epic/UE_5.3" \
    "$HOME/Epic/UE_5.2" \
    "/Users/Shared/Epic Games/UE_5.8" \
    "/Users/Shared/Epic Games/UE_5.7" \
    "/Users/Shared/Epic Games/UE_5.6" \
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

configure_android_env() {
  if [[ "${UE_ROOT:-}" == *"UE_5.0"* || "${UE_ROOT:-}" == *"UE_5.1"* || "${UE_ROOT:-}" == *"UE_5.2"* ]]; then
    if [ -d "/Library/Java/JavaVirtualMachines/jdk1.8.0_241.jdk/Contents/Home" ]; then
      export JAVA_HOME="/Library/Java/JavaVirtualMachines/jdk1.8.0_241.jdk/Contents/Home"
    elif [ -d "/Library/Java/JavaVirtualMachines/jdk-17.jdk/Contents/Home" ]; then
      export JAVA_HOME="/Library/Java/JavaVirtualMachines/jdk-17.jdk/Contents/Home"
    fi
  elif [ -d "/Library/Java/JavaVirtualMachines/jdk-17.jdk/Contents/Home" ]; then
    export JAVA_HOME="/Library/Java/JavaVirtualMachines/jdk-17.jdk/Contents/Home"
  fi
  if [ -n "${JAVA_HOME:-}" ]; then
    export PATH="$JAVA_HOME/bin:$PATH"
  fi
  if [ -z "${ANDROID_HOME:-}" ] && [ -d "$HOME/Library/Android/sdk" ]; then
    export ANDROID_HOME="$HOME/Library/Android/sdk"
  fi
  if [ -z "${ANDROID_SDK_ROOT:-}" ] && [ -n "${ANDROID_HOME:-}" ]; then
    export ANDROID_SDK_ROOT="$ANDROID_HOME"
  fi
  if [ -n "${ANDROID_HOME:-}" ]; then
    if [[ "${UE_ROOT:-}" == *"UE_5.6"* || "${UE_ROOT:-}" == *"UE_5.7"* || "${UE_ROOT:-}" == *"UE_5.8"* ]]; then
      for ndk in 27.2.12479018 27.0.12077973; do
        if [ -d "$ANDROID_HOME/ndk/$ndk" ]; then
          export NDKROOT="$ANDROID_HOME/ndk/$ndk"
          break
        fi
      done
    fi
    if [ -z "${NDKROOT:-}" ] && [ -d "$ANDROID_HOME/ndk/25.1.8937393" ]; then
      export NDKROOT="$ANDROID_HOME/ndk/25.1.8937393"
    fi
  fi
  if [ -n "${NDKROOT:-}" ]; then
    export ANDROID_NDK_ROOT="$NDKROOT"
  fi
}

configure_uproject_plugins() {
  local uproject="$SCRIPT_DIR/MagicSRUESmoke.uproject"
  python3 - "$uproject" "${UE_ROOT:-}" <<'PY'
import json
import sys

uproject_path = sys.argv[1]
ue_root = sys.argv[2]
legacy = ("UE_5.1" in ue_root) or ("UE_5.2" in ue_root)
legacy = legacy or ("UE_5.0" in ue_root)
default_modules = [{
    "Name": "MagicSRUESmoke",
    "Type": "Runtime",
    "LoadingPhase": "Default",
}]

with open(uproject_path, "r", encoding="utf-8") as f:
    data = json.load(f)

plugins = data.get("Plugins", [])
filtered = [p for p in plugins if p.get("Name") != "ACLPlugin"]
if not legacy:
    filtered.append({"Name": "ACLPlugin", "Enabled": True})

modules = data.get("Modules", [])
if legacy:
    modules = []
else:
    modules = default_modules

changed = (filtered != data.get("Plugins")) or (modules != data.get("Modules"))
if changed:
    data["Plugins"] = filtered
    data["Modules"] = modules
    with open(uproject_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.write("\n")
PY
}

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

collect_smoke_images() {
  rm -rf "$IMAGE_OUTPUT_DIR"
  mkdir -p "$IMAGE_OUTPUT_DIR"
  for backend in vulkan gles; do
    "$ADB_BIN" pull "$DEVICE_SMOKE_DIR/input_${backend}_64x64.pgm" "$IMAGE_OUTPUT_DIR/input_${backend}_64x64.pgm" >/dev/null
    "$ADB_BIN" pull "$DEVICE_SMOKE_DIR/output_${backend}_128x128.pgm" "$IMAGE_OUTPUT_DIR/output_${backend}_128x128.pgm" >/dev/null
  done
  python3 - "$IMAGE_OUTPUT_DIR" <<'PY'
import struct
import sys
import zlib
from pathlib import Path


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


image_dir = Path(sys.argv[1])
for backend in ("vulkan", "gles"):
    for kind, expected_size in (("input", (64, 64)), ("output", (128, 128))):
        src = image_dir / f"{kind}_{backend}_{expected_size[0]}x{expected_size[1]}.pgm"
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

run_uat_android_build() {
  local skip_build="${1:-0}"
  local args=(
    BuildCookRun
    -project="$PROJECT_FILE"
    -noP4
    -buildmachine
    -platform=Android
    -clientconfig=Development
    -serverconfig=Development
    -map=/Engine/Maps/Entry
    -cook
    -stage
    -pak
    -package
    -archive
    -archivedirectory="$ARCHIVE_DIR"
    -prereqs
    -utf8output
  )
  if [[ "${UE_ROOT:-}" == *"UE_5.0"* || "${UE_ROOT:-}" == *"UE_5.1"* || "${UE_ROOT:-}" == *"UE_5.2"* ]]; then
    args+=(-nocompileeditor -skipbuildeditor)
  fi
  if [ "$skip_build" = "1" ]; then
    args+=(-skipbuild)
  else
    args+=(-build)
  fi
  "$RUN_UAT_BIN" "${args[@]}" 2>&1 | tee "$UAT_LOG"
  return "${PIPESTATUS[0]}"
}

RUN_UAT_BIN=$(find_run_uat || true)
if [ -z "$RUN_UAT_BIN" ]; then
  echo "[MagicSRUESmoke] Unreal Automation Tool not found. Install Unreal Engine and set UE_ROOT=/path/to/UE_x.y." >&2
  "$SCRIPT_DIR/check_environment.sh" || true
  exit 1
fi

configure_android_env

echo "[MagicSRUESmoke] Android env ANDROID_HOME=${ANDROID_HOME:-} NDKROOT=${NDKROOT:-} JAVA_HOME=${JAVA_HOME:-}"

ADB_BIN=$(find_adb || true)
if [ -z "$ADB_BIN" ]; then
  echo "[MagicSRUESmoke] adb not found. Set ADB_BIN=/path/to/adb." >&2
  exit 1
fi

if ! wait_for_adb; then
  echo "[MagicSRUESmoke] No connected Android device." >&2
  "$ADB_BIN" devices || true
  exit 1
fi

"$SCRIPT_DIR/sync_plugin_files.sh"
configure_uproject_plugins
rm -rf "$ARCHIVE_DIR"

set +e
run_uat_android_build 0
UAT_EXIT=$?
set -e

if [ "$UAT_EXIT" -ne 0 ]; then
  GRADLE_DIR="$SCRIPT_DIR/Intermediate/Android/arm64/gradle"
  if rg -q "dexBuilderDebug|downloader_library" "$UAT_LOG" && [ -f "$GRADLE_DIR/settings.gradle" ]; then
    echo "[MagicSRUESmoke] UAT Gradle hit downloader dex issue; retrying without unused downloader module..."
    python3 - "$GRADLE_DIR/settings.gradle" <<'PY'
import sys
path = sys.argv[1]
with open(path, "r", encoding="utf-8") as f:
    lines = f.readlines()
with open(path, "w", encoding="utf-8") as f:
    for line in lines:
        if "downloader_library" not in line:
            f.write(line)
PY
    rm -rf "$GRADLE_DIR/app/build" "$GRADLE_DIR/downloader_library/build"
    (cd "$GRADLE_DIR" && ./gradlew :app:assembleDebug)
    mkdir -p "$ARCHIVE_DIR"
    cp "$SCRIPT_DIR/Binaries/Android/MagicSRUESmoke-arm64.apk" "$ARCHIVE_DIR/"
  else
    exit "$UAT_EXIT"
  fi
fi

APK_PATH=""
for candidate in "$ARCHIVE_DIR"/*.apk "$ARCHIVE_DIR"/Android/*.apk "$ARCHIVE_DIR"/Android/*/*.apk "$SCRIPT_DIR"/Binaries/Android/*.apk; do
  if [ -f "$candidate" ]; then
    APK_PATH="$candidate"
    break
  fi
done

if [ -z "$APK_PATH" ]; then
  echo "[MagicSRUESmoke] APK not found under $ARCHIVE_DIR" >&2
  exit 1
fi

set +e
INSTALL_OUTPUT=$("$ADB_BIN" install -r "$APK_PATH" 2>&1)
INSTALL_EXIT=$?
set -e
if [ "$INSTALL_EXIT" -ne 0 ]; then
  echo "$INSTALL_OUTPUT"
  if [[ "$INSTALL_OUTPUT" == *"INSTALL_FAILED_UPDATE_INCOMPATIBLE"* ]]; then
    echo "[MagicSRUESmoke] Existing app signature differs; uninstalling old package and retrying install..."
    "$ADB_BIN" uninstall "$PACKAGE_NAME" >/dev/null 2>&1 || true
    "$ADB_BIN" install "$APK_PATH"
  else
    exit "$INSTALL_EXIT"
  fi
fi
for permission in android.permission.WRITE_EXTERNAL_STORAGE android.permission.READ_EXTERNAL_STORAGE; do
  "$ADB_BIN" shell pm grant "$PACKAGE_NAME" "$permission" >/dev/null 2>&1 || true
done

for obb in "$ARCHIVE_DIR"/main.*."$PACKAGE_NAME".obb; do
  if [ -f "$obb" ]; then
    "$ADB_BIN" shell mkdir -p "/sdcard/Android/obb/$PACKAGE_NAME"
    "$ADB_BIN" push "$obb" "/sdcard/Android/obb/$PACKAGE_NAME/$(basename "$obb")"
  fi
done

if [ -d "$SCRIPT_DIR/Content/MagicSRModels" ]; then
  "$ADB_BIN" shell mkdir -p "/sdcard/Android/data/$PACKAGE_NAME/files/MagicSRModels"
  for model in "$SCRIPT_DIR/Content/MagicSRModels"/*.bin; do
    [ -f "$model" ] || continue
    "$ADB_BIN" push "$model" "/sdcard/Android/data/$PACKAGE_NAME/files/MagicSRModels/$(basename "$model")"
  done
fi
"$ADB_BIN" shell rm -rf "$DEVICE_SMOKE_DIR"
"$ADB_BIN" shell mkdir -p "$DEVICE_SMOKE_DIR"

"$ADB_BIN" logcat -c
"$ADB_BIN" shell am force-stop "$PACKAGE_NAME"
"$ADB_BIN" shell monkey -p "$PACKAGE_NAME" -c android.intent.category.LAUNCHER 1 >/dev/null

deadline=$((SECONDS + 60))
rm -f "$LOG_FILE"
touch "$LOG_FILE"

echo "[MagicSRUESmoke] Waiting for PASS in logcat..."
while [ "$SECONDS" -lt "$deadline" ]; do
  "$ADB_BIN" logcat -d -s UE "$LOG_TAG" UnrealEngine > "$LOG_FILE" || true
  if rg -q "\\[$LOG_TAG\\] result=PASS" "$LOG_FILE"; then
    collect_smoke_images
    echo "[MagicSRUESmoke] PASS"
    exit 0
  fi
  if rg -q "\\[$LOG_TAG\\] result=FAIL" "$LOG_FILE"; then
    echo "[MagicSRUESmoke] FAIL. See $LOG_FILE" >&2
    exit 1
  fi
  sleep 2
done

echo "[MagicSRUESmoke] Timed out waiting for result. See $LOG_FILE" >&2
exit 1
