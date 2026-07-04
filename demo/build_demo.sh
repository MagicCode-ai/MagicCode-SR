#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ANDROID_DIR="${ROOT}/android"
GRADLEW="${ANDROID_DIR}/gradlew"
ASSETS_MODEL_DIR="${ANDROID_DIR}/app/src/main/assets/model"
MODEL_DIR="${ROOT}/../model"
ANDROID_LIB_DIR="${ROOT}/../lib/android"
IOS_LIB_DIR="${ROOT}/../lib/ios"
HEADER_DIR="${ROOT}/../header"

require_file() {
  local file="$1"
  if [[ ! -f "${file}" ]]; then
    echo "[ERROR] Required file missing: ${file}" >&2
    exit 1
  fi
}

echo "[INFO] Root: ${ROOT}"

if [[ ! -f "${GRADLEW}" ]]; then
  echo "[ERROR] gradlew not found: ${GRADLEW}" >&2
  exit 1
fi

require_file "${ANDROID_LIB_DIR}/libmagic_sr.a"
require_file "${IOS_LIB_DIR}/libmagic_sr.a"
require_file "${HEADER_DIR}/mc_interface.h"

require_file "${MODEL_DIR}/magic_gles_highspeed_gpu_params.bin"
require_file "${MODEL_DIR}/magic_gles_speed_gpu_params.bin"
require_file "${MODEL_DIR}/magic_metal_highspeed_gpu_params.bin"
require_file "${MODEL_DIR}/magic_metal_speed_gpu_params.bin"

mkdir -p "${ASSETS_MODEL_DIR}"

echo "[INFO] Copying Android model files..."
cp -f "${MODEL_DIR}/magic_gles_highspeed_gpu_params.bin" "${ASSETS_MODEL_DIR}/"
cp -f "${MODEL_DIR}/magic_gles_speed_gpu_params.bin" "${ASSETS_MODEL_DIR}/"

echo "[INFO] Building Android Debug APK..."
(
  cd "${ANDROID_DIR}"
  chmod +x "${GRADLEW}"
  ./gradlew :app:assembleDebug
)

echo
echo "[SUCCESS] Android build complete."
echo "[SUCCESS] APK: ${ANDROID_DIR}/app/build/outputs/apk/debug/app-debug.apk"
echo "[INFO] iOS build should be done on macOS via Xcode (see README.md)."
