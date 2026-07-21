#!/usr/bin/env bash
# Build libmagic_sr_enable.a = libmagic_sr.a (core) + mc_enable(+metal).
# Users who want MC_Enable link this library; they do not compile mc_enable.c themselves.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# Prefer sibling private project tree when present (has core build outputs).
PROJECT_ROOT="$(cd "${REPO_ROOT}/../../project" 2>/dev/null && pwd || true)"
if [[ -z "${PROJECT_ROOT}" || ! -d "${PROJECT_ROOT}" ]]; then
  PROJECT_ROOT="${REPO_ROOT}"
fi

INTERFACE_DIR="${REPO_ROOT}/interface"
if [[ ! -f "${INTERFACE_DIR}/mc_enable.c" && -f "${PROJECT_ROOT}/interface/mc_enable.c" ]]; then
  INTERFACE_DIR="${PROJECT_ROOT}/interface"
fi

NDK="${ANDROID_NDK:-${HOME}/Library/Android/sdk/ndk/29.0.14033849}"
IOS_SDK="$(xcrun --sdk iphoneos --show-sdk-path 2>/dev/null || true)"
MIN_IOS="15.6"

log() { echo "[build_enable_lib] $*"; }
die() { echo "[build_enable_lib] ERROR: $*" >&2; exit 1; }

merge_ar() {
  local out="$1"
  local core="$2"
  shift 2
  local objs=("$@")
  rm -f "${out}"
  cp "${core}" "${out}"
  if command -v llvm-ar >/dev/null 2>&1; then
    llvm-ar r "${out}" "${objs[@]}"
  elif [[ -x "${NDK}/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-ar" ]]; then
    "${NDK}/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-ar" r "${out}" "${objs[@]}"
  else
    ar r "${out}" "${objs[@]}"
  fi
}

build_android() {
  local core="${1:-}"
  local out_dir="${2:-}"
  [[ -f "${core}" ]] || die "Android core lib missing: ${core}"
  [[ -d "${NDK}" ]] || die "Android NDK not found: ${NDK}"
  mkdir -p "${out_dir}"
  local tmp
  tmp="$(mktemp -d)"
  local cc="${NDK}/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android24-clang"
  [[ -x "${cc}" ]] || die "NDK clang not found: ${cc}"

  log "Android: compile mc_enable.c"
  "${cc}" -c "${INTERFACE_DIR}/mc_enable.c" -o "${tmp}/mc_enable.o" \
    -I"${INTERFACE_DIR}" \
    -I"${PROJECT_ROOT}/src" \
    -DSYS_ANDROID \
    -DARCH_AARCH64 \
    -DOpenGLES=1 \
    -DVULKAN \
    -DMAGIC_ANDROID_DUAL_BACKEND \
    -fPIC -O2 -std=c11

  local out="${out_dir}/libmagic_sr_enable.a"
  log "Android: merge -> ${out}"
  merge_ar "${out}" "${core}" "${tmp}/mc_enable.o"
  rm -rf "${tmp}"

  if nm "${out}" 2>/dev/null | grep -q 'T MC_Enable'; then
    log "Android: OK (MC_Enable present)"
  else
    die "Android: MC_Enable missing in ${out}"
  fi
}

build_ios() {
  local core="${1:-}"
  local out_dir="${2:-}"
  [[ -f "${core}" ]] || die "iOS core lib missing: ${core}"
  [[ -n "${IOS_SDK}" && -d "${IOS_SDK}" ]] || die "iPhoneOS SDK not found"
  [[ -f "${INTERFACE_DIR}/mc_enable_metal.m" ]] || die "mc_enable_metal.m missing"
  mkdir -p "${out_dir}"
  local tmp
  tmp="$(mktemp -d)"

  log "iOS: compile mc_enable.c + mc_enable_metal.m"
  xcrun --sdk iphoneos clang -c "${INTERFACE_DIR}/mc_enable.c" -o "${tmp}/mc_enable.o" \
    -arch arm64 -isysroot "${IOS_SDK}" -miphoneos-version-min="${MIN_IOS}" \
    -I"${INTERFACE_DIR}" -I"${PROJECT_ROOT}/src" -O2 -std=c11

  xcrun --sdk iphoneos clang -c "${INTERFACE_DIR}/mc_enable_metal.m" -o "${tmp}/mc_enable_metal.o" \
    -arch arm64 -isysroot "${IOS_SDK}" -miphoneos-version-min="${MIN_IOS}" \
    -I"${INTERFACE_DIR}" -I"${PROJECT_ROOT}/src" -O2 -fobjc-arc

  local out="${out_dir}/libmagic_sr_enable.a"
  log "iOS: merge -> ${out}"
  xcrun libtool -static -o "${out}" "${core}" "${tmp}/mc_enable.o" "${tmp}/mc_enable_metal.o"
  rm -rf "${tmp}"

  if nm -arch arm64 "${out}" 2>/dev/null | grep -q 'T _MC_Enable'; then
    log "iOS: OK (MC_Enable present)"
  else
    die "iOS: MC_Enable missing in ${out}"
  fi
}

usage() {
  cat <<'EOF'
Usage: ./tools/build_enable_lib.sh [android|ios|all]

Produces libmagic_sr_enable.a (core libmagic_sr.a + mc_enable).
Looks for core libs under:
  <repo>/lib/<platform>/libmagic_sr.a
  <project>/build/... (sibling Magic project tree when present)
EOF
}

cmd="${1:-all}"
case "${cmd}" in
  -h|--help) usage; exit 0 ;;
esac

ANDROID_CORE=""
for c in \
  "${REPO_ROOT}/lib/android/libmagic_sr.a" \
  "${PROJECT_ROOT}/build/android/build/libmagic_sr.a" \
  "${PROJECT_ROOT}/lib/android/libmagic_sr.a"
do
  [[ -f "${c}" ]] && ANDROID_CORE="${c}" && break
done

IOS_CORE=""
for c in \
  "${REPO_ROOT}/lib/ios/libmagic_sr.a" \
  "${PROJECT_ROOT}/build/ipad/magic_sr/Release-iphoneos/libmagic_sr.a" \
  "${PROJECT_ROOT}/build/ipad/magic_sr/Debug-iphoneos/libmagic_sr.a"
do
  [[ -f "${c}" ]] && IOS_CORE="${c}" && break
done

do_android=0
do_ios=0
case "${cmd}" in
  android) do_android=1 ;;
  ios) do_ios=1 ;;
  all) do_android=1; do_ios=1 ;;
  *) usage; die "unknown command: ${cmd}" ;;
esac

if [[ "${do_android}" -eq 1 ]]; then
  [[ -n "${ANDROID_CORE}" ]] || die "no Android libmagic_sr.a found"
  build_android "${ANDROID_CORE}" "${REPO_ROOT}/lib/android"
  if [[ -d "${PROJECT_ROOT}/build/android/build" ]]; then
    cp "${REPO_ROOT}/lib/android/libmagic_sr_enable.a" \
       "${PROJECT_ROOT}/build/android/build/libmagic_sr_enable.a"
  fi
fi

if [[ "${do_ios}" -eq 1 ]]; then
  [[ -n "${IOS_CORE}" ]] || die "no iOS libmagic_sr.a found"
  build_ios "${IOS_CORE}" "${REPO_ROOT}/lib/ios"
  # Also place next to core outputs used by demos
  for d in \
    "${PROJECT_ROOT}/build/ipad/magic_sr/Release-iphoneos" \
    "${PROJECT_ROOT}/build/ipad/magic_sr/Debug-iphoneos"
  do
    if [[ -d "${d}" ]]; then
      cp "${REPO_ROOT}/lib/ios/libmagic_sr_enable.a" "${d}/libmagic_sr_enable.a"
    fi
  done
fi

log "Done."
