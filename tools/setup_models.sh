#!/usr/bin/env bash
# Copy MagicSR model .bin files into the paths used by demos / Unity / UE / device.
# Usage:
#   ./tools/setup_models.sh                  # demos + local MagicSRModels/
#   ./tools/setup_models.sh demo
#   ./tools/setup_models.sh unity --project /path/to/UnityProject
#   ./tools/setup_models.sh ue --project /path/to/UEProject
#   ./tools/setup_models.sh dir --dest /absolute/path/MagicSRModels
#   ./tools/setup_models.sh adb              # adb push GPU models to Android Documents
#   ./tools/setup_models.sh all --project-unity ... --project-ue ...
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
MODEL_SRC="${REPO_ROOT}/model"

die() { echo "error: $*" >&2; exit 1; }
log() { echo "[setup_models] $*"; }

need_src() {
  [[ -d "${MODEL_SRC}" ]] || die "model directory not found: ${MODEL_SRC}"
}

# GPU models used by Enable (skip CPU-only bins by default)
GPU_MODELS=(
  magic_metal_highspeed_gpu_params.bin
  magic_metal_speed_gpu_params.bin
  magic_gl_highspeed_gpu_params.bin
  magic_gl_speed_gpu_params.bin
  magic_gles_highspeed_gpu_params.bin
  magic_gles_speed_gpu_params.bin
  magic_vulkan_highspeed_gpu_params.bin
  magic_vulkan_speed_gpu_params.bin
)

copy_gpu_models() {
  local dest="$1"
  mkdir -p "${dest}"
  local name
  for name in "${GPU_MODELS[@]}"; do
    local src="${MODEL_SRC}/${name}"
    if [[ -f "${src}" ]]; then
      cp -f "${src}" "${dest}/${name}"
      log "copied ${name} -> ${dest}/"
    else
      log "skip missing ${name}"
    fi
  done
}

setup_demo_android() {
  local dest="${REPO_ROOT}/demo/android/app/src/main/assets/model"
  copy_gpu_models "${dest}"
  log "Android demo assets ready. App should load from assets then MC_Enable_SetModelPath."
}

setup_demo_ios() {
  # iOS demo typically copies from a known folder into sandbox at runtime.
  local dest="${REPO_ROOT}/demo/ios/MagicCameraSR/MagicSRModels"
  copy_gpu_models "${dest}"
  log "iOS demo models at: ${dest}"
  log "Call MC_Enable_SetModelPath / SetModelDir for these files (see demo ViewController)."
}

setup_local_dir() {
  local dest="${REPO_ROOT}/MagicSRModels"
  copy_gpu_models "${dest}"
  log "Local folder: ${dest}"
  log "For Native testing: MC_Enable_SetModelDir(\"${dest}\");"
}

setup_unity_project() {
  local project="${1:-}"
  [[ -n "${project}" ]] || die "unity requires --project <UnityProjectRoot>"
  [[ -d "${project}" ]] || die "Unity project not found: ${project}"
  local dest="${project}/Assets/StreamingAssets/MagicSRModels"
  copy_gpu_models "${dest}"
  log "Unity StreamingAssets ready: ${dest}"
  log "In code: MagicSR.SetModelDir(Application.persistentDataPath + \"/MagicSRModels\");"
  log "Also copy StreamingAssets -> persistentDataPath at runtime (see User Guide §4)."
}

setup_ue_project() {
  local project="${1:-}"
  [[ -n "${project}" ]] || die "ue requires --project <UEProjectRoot>"
  [[ -d "${project}" ]] || die "UE project not found: ${project}"
  local dest="${project}/Content/MagicSRModels"
  copy_gpu_models "${dest}"
  log "UE Content models ready: ${dest}"
  log "After packaging, copy to device and call SetModelDir (see User Guide §4)."
}

setup_custom_dir() {
  local dest="${1:-}"
  [[ -n "${dest}" ]] || die "dir requires --dest <absolute_or_relative_path>"
  copy_gpu_models "${dest}"
  # Prefer absolute path in the hint
  local abs
  abs="$(cd "${dest}" && pwd)"
  log "Models ready: ${abs}"
  log "Then: MC_Enable_SetModelDir(\"${abs}\");"
}

setup_adb_push() {
  command -v adb >/dev/null 2>&1 || die "adb not found in PATH"
  adb get-state >/dev/null 2>&1 || die "no Android device/emulator (adb get-state failed)"
  local remote="/storage/emulated/0/Documents/MagicSRModels"
  adb shell "mkdir -p '${remote}'"
  local name
  for name in "${GPU_MODELS[@]}"; do
    local src="${MODEL_SRC}/${name}"
    if [[ -f "${src}" ]]; then
      adb push "${src}" "${remote}/${name}" >/dev/null
      log "adb pushed ${name} -> ${remote}/"
    fi
  done
  log "On device: MC_Enable_SetModelDir(\"${remote}\"); before Enable"
}

usage() {
  cat <<'EOF'
MagicSR model setup helper

Commands:
  demo                 Copy GPU models into Android + iOS demos
  demo-android         Android demo assets only
  demo-ios             iOS demo MagicSRModels only
  local                Copy to <repo>/MagicSRModels (then SetModelDir)
  unity --project DIR  Copy to Unity Assets/StreamingAssets/MagicSRModels
  ue --project DIR     Copy to UE Content/MagicSRModels
  dir --dest DIR       Copy to a custom directory
  adb                  adb push GPU models to Android Documents/MagicSRModels
  all                  demo + local (and unity/ue if --project-unity / --project-ue given)

Options:
  --project DIR        Project root for unity / ue
  --project-unity DIR  Unity project for "all"
  --project-ue DIR     UE project for "all"
  --dest DIR           Destination for "dir"
  -h, --help           Show help
EOF
}

main() {
  need_src
  local cmd="${1:-demo}"
  shift || true

  local project=""
  local project_unity=""
  local project_ue=""
  local dest=""

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --project) project="${2:-}"; shift 2 ;;
      --project-unity) project_unity="${2:-}"; shift 2 ;;
      --project-ue) project_ue="${2:-}"; shift 2 ;;
      --dest) dest="${2:-}"; shift 2 ;;
      -h|--help) usage; exit 0 ;;
      *) die "unknown argument: $1" ;;
    esac
  done

  case "${cmd}" in
    -h|--help) usage ;;
    demo)
      setup_demo_android
      setup_demo_ios
      ;;
    demo-android) setup_demo_android ;;
    demo-ios) setup_demo_ios ;;
    local) setup_local_dir ;;
    unity) setup_unity_project "${project}" ;;
    ue) setup_ue_project "${project}" ;;
    dir) setup_custom_dir "${dest}" ;;
    adb) setup_adb_push ;;
    all)
      setup_demo_android
      setup_demo_ios
      setup_local_dir
      if [[ -n "${project_unity}" ]]; then setup_unity_project "${project_unity}"; fi
      if [[ -n "${project_ue}" ]]; then setup_ue_project "${project_ue}"; fi
      ;;
    *)
      usage
      die "unknown command: ${cmd}"
      ;;
  esac

  log "done."
}

main "$@"
