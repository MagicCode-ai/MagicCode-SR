# MagicSR Demos (v2.0.0)

Public samples for **MagicCode Super-Resolution v2.0.0**. Every supported demo uses the **core Session API** (`MC_Init` / `MC_Process` / `MC_Uninit`) and links **core** `libmagic_sr.a`. They do **not** use `MC_Enable*` or `libmagic_sr_enable.a`.

Supported samples:

1. **Camera magnifier** (`android/`, `ios/`) — spatial OpenGLES (Android arm64-v8a) and Metal (iOS device arm64). Center-crop by `1/scale`, then spatial SR back to display size.
2. **macOS Apple Silicon offline sample** (`mac_arm/`) — command-line / Xcode tool for **macOS arm64** Metal / NEON. There is **no** macOS x86_64 demo target in this tree (a `lib/mac_x86/libmagic_sr.a` may exist at repo root, but it is not wired here).

These samples are **spatial** (`SPATIAL_SPEED_MODE` / `SPATIAL_BALANCED_MODE`). They do not implement temporal TAAU. Temporal integration must supply color, output, depth, motion, jitter, sizes, scale, camera, reset, layout, and a recording command buffer where the backend requires it (see `interface/mc_interface.h`). Do not route temporal through `MC_Enable`.

## Layout

```text
<repo-root>/
  lib/
    android/libmagic_sr.a    # Android arm64-v8a core
    ios/libmagic_sr.a        # iOS device arm64 core
    mac_arm/libmagic_sr.a    # macOS arm64 core (mac_arm demo only)
  interface/
    mc_interface.h            # v2.0.0 Session API (authoritative)
  model/                     # SR model .bin files (sibling of demo/)
  demo/
    android/                 # Android Studio camera project
    ios/                     # Xcode camera project
    mac_arm/                 # macOS arm64 offline / Metal sample
    build_demo.sh / .bat
    build_magnifier_demo.bat
    README.md
```

Native link paths resolve to repo-root `lib/` (sibling of `demo/`). Demo-local copies of `mc_interface.h` match the v2.0.0 public header.

| Platform | Library (from `demo/`) | Headers |
|----------|------------------------|--------|
| Android | `../lib/android/libmagic_sr.a` | `android/app/src/main/cpp/mc_interface.h` (+ `../interface/`) |
| iOS | `../lib/ios/libmagic_sr.a` (`-lmagic_sr`) | `ios/MagicCameraSR/mc_interface.h` |
| macOS arm64 | `../lib/mac_arm/libmagic_sr.a` | `mac_arm/mc_interface.h` |

Android CMake:

```cmake
set(MAGIC_LIB_PATH ${CMAKE_SOURCE_DIR}/../../../../../../lib/android/libmagic_sr.a)
```

iOS (`project.pbxproj`):

- `LIBRARY_SEARCH_PATHS = $(SRCROOT)/../../lib/ios`
- `HEADER_SEARCH_PATHS = $(SRCROOT)/MagicCameraSR` (and repo `interface/` as fallback)
- `OTHER_LDFLAGS = -lmagic_sr -lc++`

## Session lifecycle

1. Zero-initialize `input_param_t`, set `struct_size = sizeof(input_param_t)`.
2. Fill `width` / `height` (input, `[64, 4032]`), `scaler_factor` (spatial `[1, 8]`), `alg_mode`, `backend`, `input_type`, `model_path`, `gpu_context`.
   - GLES: current EGL/GLES context before `MC_Init` and every `MC_Process`. `gpu_context` may be all-zero; this demo stores the EGL context in `native_context`.
   - Metal: pass the same `MTLDevice*` used to create textures in `gpu_context.device`.
3. Call `MC_Init` **once** per size / scale / mode. Do not Init/Uninit every frame.
4. Each frame: bind caller-owned `magic_frame_t.image_in` / `image_out` (`handle.gl_texture` or `handle.pointer`, plus backend `format`). Spatial `frame` and `command_buffer` are NULL (Metal may submit internally).
5. After GPU work is done and textures are no longer in use, call `MC_Uninit`.
6. Check `MC_Process` / `MC_Uninit` return codes (`0` = success).

UI **speed** → `SPATIAL_SPEED_MODE`; **balanced** → `SPATIAL_BALANCED_MODE`.

## Required files

Paths are relative to the **repo root**.

Core spatial GPU models are the **combined** `magic_sr_*_params.bin` files. Product `load_params` (`common.c`) maps **SPATIAL_BALANCED_MODE → segment 0** and **SPATIAL_SPEED_MODE → segment (1 + spatial_sharpen_level)**. One file is enough for both UI modes. This tree does **not** ship separate `magic_*_balanced_gpu_params.bin` / `magic_metal_*` / `magic_gles_*` mode files; do not treat those names as required.

### Android (arm64-v8a, OpenGLES, minSdk 28)

- `lib/android/libmagic_sr.a`
- `demo/android/app/src/main/cpp/mc_interface.h` (v2.0.0)
- `model/magic_sr_gpu_params.bin` — Gradle `copyRepoGpuModel` copies it into `android/app/src/main/assets/model/` on every build so a clean checkout can `MC_Init`

Local extra copies under `assets/model/` (if present) may share the same SHA as `magic_sr_gpu_params.bin`; they are not distinct mode weights.

### iOS (device arm64, Metal)

- `lib/ios/libmagic_sr.a`
- `demo/ios/MagicCameraSR/mc_interface.h`
- Repo `model/*.bin` (Xcode copies them into the bundle). A clean tree has `magic_sr_gpu_params.bin` (and optional CPU `magic_sr_cpu_params.bin`). That GPU file is what the camera demo loads.

Simulator / x86_64 iOS is **not** supported (the core archive is device arm64).

### macOS arm64 (`mac_arm/`)

- `lib/mac_arm/libmagic_sr.a` (core only — **not** `libmagic_sr_enable.a`)
- `demo/mac_arm/mc_interface.h`
- `model/magic_sr_gpu_params.bin` and/or `model/magic_sr_cpu_params.bin` via `MODEL_ROOT_PATH` in `mac_arm/example.c`
- `demo/mac_arm/metal_temporal_link_stubs.c` — spatial-only stubs so the published mac archive can link (see Limits)
- Build **arm64 only**. This host sample does not link `lib/mac_x86`.

## Build

### Android

From `demo/`:

```bash
./build_demo.sh          # macOS / Linux
# or
build_demo.bat           # Windows
# or
build_magnifier_demo.bat
```

Manual:

```bash
cd android
./gradlew :app:assembleRelease
# instrumented sources (optional): ./gradlew :app:compileDebugAndroidTestSources
```

ABI is `arm64-v8a` only. Install (optional; this tree is for compile verification):

```bash
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
```

### iOS

macOS + Xcode. Generic device build (no signing / no device install):

```bash
cd ios
xcodebuild -project MagicCameraSR.xcodeproj \
  -scheme MagicMagnifierSR \
  -configuration Release \
  -destination 'generic/platform=iOS' \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGN_IDENTITY="" \
  build
```

### macOS arm64 (`mac_arm/`)

Requires the arm64 core library. On Apple Silicon, build normally. On Intel Macs this is a **cross-compile** to arm64 (the binary will not run on x86_64).

```bash
cd mac_arm/example
xcodebuild -project example.xcodeproj \
  -scheme example \
  -configuration Release \
  -destination 'generic/platform=macOS' \
  ARCHS=arm64 \
  ONLY_ACTIVE_ARCH=NO \
  CODE_SIGNING_ALLOWED=NO \
  build
```

Edit `MODEL_ROOT_PATH` / dataset paths in `mac_arm/example.c` before running.

## Runtime (camera)

1. Grant camera permission
2. Select `speed` or `balanced`
3. Drag the scale slider (1–8) — center crop by `1/scale`, then spatial `MC_Process`

`mac_arm` is a developer smoke / PSNR tool, not the magnifier UI.

## Limits

- Android: **arm64-v8a**, OpenGLES 3.x, **minSdk 28** (v2 core links Vulkan 1.1 `vkGetPhysicalDeviceFeatures2`; NDK API 26 stubs do not export it)
- iOS: **device arm64** Metal; no simulator
- Output width/height must match core `scaled_dimension` in `src/magic_backend.h`: `floor((double)value * (double)scaler + 0.5)`, not a float truncate. Demos size output textures with that rule and confirm via `MC_Control(QUERY_STATUS)`.
- iOS spatial `MC_Process(..., command_buffer=NULL)` is **synchronous**: `MC_Process` → `gpu_process` → Metal `speed_sr_process` / `balanced_sr_process`, which `commit` + `waitUntilCompleted` on an internal command buffer before return. `getBytes` after a 0 return is safe; `command_buffer` is only used on the temporal Metal path.
- macOS demo: **arm64** only. The published `lib/mac_arm/libmagic_sr.a` does not contain Metal temporal objects (`metal_temporal_*` are **U** in that archive). Those refs live in the same `magic_process.o` as `MC_Init`/`MC_Process`, so `-dead_strip` cannot drop them and the archive is not repacked. `metal_temporal_link_stubs.c` matches `src/metal/magic_sr_metal_temporal.h` and the official generator in product `tools/mac_api_test_common.sh`, but every temporal entry **fails** (`MC_ERROR_INIT_BACKEND_UNAVAILABLE` / `MC_ERROR_TEMPORAL_COMMAND_BUFFER`). `nm` of the linked demo must not show a real temporal implementation.
- These samples do **not** call `MC_Enable*`, v1 `SPEED_MODE`/`BALANCED_MODE`/`HIGH_SPEED_MODE`, or the three-argument `MC_Process(handle, in, out)`.
- Spatial Session API only; no Enable API examples
- No temporal demo in this folder
- Do not mix `libmagic_sr.a` with `libmagic_sr_enable.a` in the same process

## What not to commit under `demo/`

- `android/.gradle/`, `android/app/build/`, `android/app/.cxx/`, `android/local.properties`
- `ios/**/xcuserdata/`, `ios/Build/`
- `mac_arm/example/**/xcuserdata/`, `mac_arm/**/Build/`
- `.DS_Store`

Keep source, Gradle wrappers, Xcode projects, and scripts. Link repo-root `../lib/<platform>/libmagic_sr.a` and load models from `../model/` (or paths in `example.c`).
