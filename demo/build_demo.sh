#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${ROOT}/.." && pwd)"
ANDROID_DIR="${ROOT}/android"
GRADLEW="${ANDROID_DIR}/gradlew"
ASSETS_MODEL_DIR="${ANDROID_DIR}/app/src/main/assets/model"
MODEL_DIR="${REPO_ROOT}/model"
ANDROID_LIB_DIR="${REPO_ROOT}/lib/android"
IOS_LIB_DIR="${REPO_ROOT}/lib/ios"
INTERFACE_DIR="${REPO_ROOT}/interface"
PACKAGES_DIR="${ANDROID_DIR}/packages"

require_file() {
  local file="$1"
  if [[ ! -f "${file}" ]]; then
    echo "[ERROR] Required file missing: ${file}" >&2
    exit 1
  fi
}

echo "[INFO] Demo root: ${ROOT}"

if [[ ! -f "${GRADLEW}" ]]; then
  echo "[ERROR] gradlew not found: ${GRADLEW}" >&2
  exit 1
fi

require_file "${INTERFACE_DIR}/mc_interface.h"
require_file "${INTERFACE_DIR}/mc_enable.h"
require_file "${ANDROID_LIB_DIR}/libmagic_sr_enable.a"
require_file "${IOS_LIB_DIR}/libmagic_sr_enable.a"

require_file "${MODEL_DIR}/magic_gles_highspeed_gpu_params.bin"
require_file "${MODEL_DIR}/magic_gles_speed_gpu_params.bin"
require_file "${MODEL_DIR}/magic_metal_highspeed_gpu_params.bin"
require_file "${MODEL_DIR}/magic_metal_speed_gpu_params.bin"

mkdir -p "${ASSETS_MODEL_DIR}"
mkdir -p "${PACKAGES_DIR}"

echo "[INFO] Copying Android model files..."
cp -f "${MODEL_DIR}/magic_gles_highspeed_gpu_params.bin" "${ASSETS_MODEL_DIR}/"
cp -f "${MODEL_DIR}/magic_gles_speed_gpu_params.bin" "${ASSETS_MODEL_DIR}/"

echo "[INFO] Building Android Debug APK..."
(
  cd "${ANDROID_DIR}"
  chmod +x "${GRADLEW}"
  ./gradlew :app:assembleDebug
)

APK_SRC="${ANDROID_DIR}/app/build/outputs/apk/debug/app-debug.apk"
APK_DST="${PACKAGES_DIR}/MagicMagnifierSR-android-arm64.apk"
cp -f "${APK_SRC}" "${APK_DST}"

echo
echo "[SUCCESS] Android build complete."
echo "[SUCCESS] APK: ${APK_DST}"
echo "[INFO] Install: adb install -r \"${APK_DST}\""
echo "[INFO] iOS: open demo/ios/MagicCameraSR.xcodeproj, or see README for packaging notes."
