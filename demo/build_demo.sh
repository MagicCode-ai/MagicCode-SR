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

copy_if_exists() {
  local src="$1"
  local dst_dir="$2"
  if [[ -f "${src}" ]]; then
    cp -f "${src}" "${dst_dir}/"
    echo "[INFO] Copied $(basename "${src}")"
  fi
}

echo "[INFO] Demo root: ${ROOT}"

if [[ ! -f "${GRADLEW}" ]]; then
  echo "[ERROR] gradlew not found: ${GRADLEW}" >&2
  exit 1
fi

require_file "${INTERFACE_DIR}/mc_interface.h"
require_file "${ROOT}/android/app/src/main/cpp/mc_interface.h"
require_file "${ROOT}/ios/MagicCameraSR/mc_interface.h"
require_file "${ANDROID_LIB_DIR}/libmagic_sr.a"
require_file "${IOS_LIB_DIR}/libmagic_sr.a"
require_file "${REPO_ROOT}/lib/mac_arm/libmagic_sr.a"

mkdir -p "${ASSETS_MODEL_DIR}"
mkdir -p "${PACKAGES_DIR}"

echo "[INFO] Copying Android GLES model files (if present in ${MODEL_DIR})..."
copy_if_exists "${MODEL_DIR}/magic_gles_speed_gpu_params.bin" "${ASSETS_MODEL_DIR}"
copy_if_exists "${MODEL_DIR}/magic_gles_balanced_gpu_params.bin" "${ASSETS_MODEL_DIR}"
copy_if_exists "${MODEL_DIR}/magic_gles_highspeed_gpu_params.bin" "${ASSETS_MODEL_DIR}"
copy_if_exists "${MODEL_DIR}/magic_sr_gpu_params.bin" "${ASSETS_MODEL_DIR}"

if ! ls "${ASSETS_MODEL_DIR}"/*.bin >/dev/null 2>&1; then
  echo "[ERROR] No GLES/GPU model .bin in ${ASSETS_MODEL_DIR}. Place MagicSR v2 spatial models under ${MODEL_DIR}." >&2
  exit 1
fi

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
echo "[INFO] iOS: xcodebuild generic/platform=iOS (see README). Core lib is device arm64."
echo "[INFO] mac_arm: arm64-only Session sample; no mac_x86 target."
