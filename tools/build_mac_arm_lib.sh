#!/usr/bin/env bash
# Build Mac Apple Silicon (arm64) libmagic_sr.a and libmagic_sr_enable.a.
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
  -O2
  -g
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
  "${ROOT}/src/arm/magic_sr_neon.c"
  "${ROOT}/src/arm/magic_sr_balanced_upscaler_neon.c"
  "${ROOT}/src/arm/magic_sr_speed_upscaler_neon.c"
)
M_SRCS=(
  "${ROOT}/src/metal/magic_sr_metal.m"
  "${ROOT}/src/metal/metal_device.m"
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
  clang -std=c17 "${COMMON[@]}" -c "${src}" -o "${obj}"
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

log "compile Enable"
clang -std=c17 "${COMMON[@]}" -c "${ROOT}/interface/mc_enable.c" -o "${BUILD_DIR}/mc_enable.o"
clang -std=gnu17 -fobjc-arc "${COMMON[@]}" -c "${ROOT}/interface/mc_enable_metal.m" \
  -o "${BUILD_DIR}/mc_enable_metal.o"

enable="${OUT_DIR}/libmagic_sr_enable.a"
log "archive ${enable}"
rm -f "${enable}"
xcrun libtool -static -o "${enable}" "${core}" "${BUILD_DIR}/mc_enable.o" "${BUILD_DIR}/mc_enable_metal.o"
xcrun ranlib "${enable}"

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

need_sym "${core}" "_MC_Init"
need_sym "${core}" "_MC_Process"
need_sym "${core}" "_MC_Uninit"
if has_defined_sym "${core}" "_MC_Enable"; then
  die "core lib should not export MC_Enable"
fi
need_sym "${enable}" "_MC_Enable"
need_sym "${enable}" "_MC_Init"

lipo -info "${core}"
lipo -info "${enable}"
log "OK"
ls -lh "${core}" "${enable}"
