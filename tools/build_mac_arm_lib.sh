#!/usr/bin/env bash
# Build Mac Apple Silicon (arm64) libmagic_sr.a.
# Flags match iOS Xcode Release (magic_sr.xcodeproj): -Os -g, NS assertions off.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="${1:-${ROOT}/lib/mac_arm}"
BUILD_DIR="${ROOT}/build/mac/magic_sr/obj"
SDK="$(xcrun --sdk macosx --show-sdk-path)"
MIN_MACOS="${MACOSX_DEPLOYMENT_TARGET:-14.0}"
DEFS=(
  -DHAVE_NEON
  -DHAVE_AARCH64
  -DARCH_ARM
  -DSYS_MACOSX
  -DHAVE_POSIXTHREAD
  -DNS_BLOCK_ASSERTIONS=1
)
INCS=(
  -I"${ROOT}/interface"
  -I"${ROOT}/src"
  -I"${ROOT}/src/arm"
  -I"${ROOT}/src/metal"
)
COMMON=(
  -arch arm64
  -isysroot "${SDK}"
  -mmacosx-version-min="${MIN_MACOS}"
  -Os
  -g
  -fno-common
  -fPIC
  "${DEFS[@]}"
  "${INCS[@]}"
  -Wno-pointer-sign
  -Wno-incompatible-pointer-types
  -Wno-unused-variable
  -Wno-unused-function
)

C_SRCS=(
  "${ROOT}/src/base.c"
  "${ROOT}/src/common.c"
  "${ROOT}/src/magic_backend.c"
  "${ROOT}/src/magic_process.c"
  "${ROOT}/src/magic_report.c"
  "${ROOT}/src/magic_sr_c.c"
  "${ROOT}/src/magic_temporal_common.c"
  "${ROOT}/src/arm/magic_sr_neon.c"
  "${ROOT}/src/arm/magic_sr_balanced_upscaler_neon.c"
  "${ROOT}/src/arm/magic_sr_speed_upscaler_neon.c"
)
M_SRCS=(
  "${ROOT}/src/metal/magic_sr_metal.m"
  "${ROOT}/src/metal/metal_device.m"
  "${ROOT}/src/metal/temporal_validate_metal.m"
)

log() { echo "[build_mac_arm_lib] $*"; }
die() { echo "[build_mac_arm_lib] ERROR: $*" >&2; exit 1; }

mkdir -p "${BUILD_DIR}" "${OUT_DIR}"
rm -f "${BUILD_DIR}"/*.o

objs=()
log "compile core C (${#C_SRCS[@]} files)"
for src in "${C_SRCS[@]}"; do
  [[ -f "${src}" ]] || die "missing ${src}"
  obj="${BUILD_DIR}/$(basename "${src%.*}").o"
  clang -std=gnu17 "${COMMON[@]}" -c "${src}" -o "${obj}"
  objs+=("${obj}")
done

log "compile Metal ObjC (${#M_SRCS[@]} files)"
for src in "${M_SRCS[@]}"; do
  [[ -f "${src}" ]] || die "missing ${src}"
  obj="${BUILD_DIR}/$(basename "${src%.*}").o"
  clang -std=gnu17 -fobjc-arc "${COMMON[@]}" -c "${src}" -o "${obj}"
  objs+=("${obj}")
done

core="${OUT_DIR}/libmagic_sr.a"
log "archive ${core}"
rm -f "${core}"
xcrun libtool -static -o "${core}" "${objs[@]}"
xcrun ranlib "${core}"

has_defined_sym() {
  local lib="$1"
  local sym="$2"
  nm -gU "${lib}" 2>/dev/null | grep -E " T ${sym}$" >/dev/null
}

need_sym() {
  local lib="$1"
  local sym="$2"
  if ! has_defined_sym "${lib}" "${sym}"; then
    die "${lib} missing ${sym}"
  fi
}

need_sym "${core}" "_MC_Enable"
need_sym "${core}" "_MC_Disable"
need_sym "${core}" "_MC_GetVersion"

lipo -info "${core}"
log "OK"
ls -lh "${core}"
