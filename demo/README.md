# MagicSR Demos

This folder contains two kinds of samples:

1. **Camera magnifier** (`android/`, `ios/`) — mobile apps using the **simple Enable path** (`MC_Enable*` / `libmagic_sr_enable.a`)
2. **macOS Apple Silicon offline sample** (`mac_arm/`) — desktop command-line / Xcode tool using the **professional session path** (`MC_Init` / `MC_Process` / `MC_Uninit` / `libmagic_sr.a`)

### Camera magnifier (Android / iOS)

- Android (OpenGLES)
- iOS / iPadOS (Metal)
- Continuous zoom `[1.0, 8.0]` via center-crop + SR
- Modes: `highspeed` / `speed`
- Scale slider with integer tick marks (1–8)

If any required file is missing or SR init/process fails, the app reports error and stops.

### macOS Apple Silicon (`mac_arm/`)

Offline image / Metal texture smoke test for **macOS arm64** (not the camera UI). It links core `lib/mac_arm/libmagic_sr.a`, uses `mc_interface.h`, and helpers in `metal.m` / `img_process.c` for RGBA Metal textures and YUV conversion. Model paths are compile-time macros in `example.c` (edit `MODEL_ROOT_PATH` / dataset paths before building).

## Layout

```text
<repo-root>/
  lib/
    android/
      libmagic_sr_enable.a   # Android camera demo
    ios/
      libmagic_sr_enable.a   # iOS camera demo
    mac_arm/
      libmagic_sr.a          # macOS arm64 session demo (core only)
  interface/
    mc_enable.h
    mc_interface.h
  model/                     # SR model bin files (sibling of demo/)
  demo/
    android/                 # Android Studio camera project
      packages/              # Optional prebuilt APK
    ios/                     # Xcode camera project
    mac_arm/                 # macOS arm64 offline / Metal sample
      example.c              # main: MC_Init / Process / Uninit
      metal.m / metal.h      # Metal texture helpers
      img_process.c / .h     # CPU image / YUV helpers
      stb_image.h
      example/example.xcodeproj
    build_demo.sh / .bat
    build_magnifier_demo.bat
    README.md
```

Native link paths resolve to the **repo-root** `lib/` (sibling of `demo/`), not `demo/lib/`:

| Platform | Library (from `demo/`) | Headers / search path |
|----------|------------------------|------------------------|
| Android | `../lib/android/libmagic_sr_enable.a` | `../interface/` (+ local copies in `android/.../cpp/`) |
| iOS | `../lib/ios/libmagic_sr_enable.a` | `$(SRCROOT)/../../interface` + `-lmagic_sr_enable` |
| macOS arm64 | `../lib/mac_arm/libmagic_sr.a` | `../../interface` (from `mac_arm/example.c`) + Xcode `LIBRARY_SEARCH_PATHS` → `lib/mac_arm` |

From `android/app/src/main/cpp/CMakeLists.txt`:

```cmake
set(MAGIC_LIB_PATH ${CMAKE_SOURCE_DIR}/../../../../../../lib/android/libmagic_sr_enable.a)
```

(`cpp` → `main` → `src` → `app` → `android` → `demo` → repo root, then `lib/android/...`)

iOS (`project.pbxproj`):

- `LIBRARY_SEARCH_PATHS = $(SRCROOT)/../../lib/ios`
- `HEADER_SEARCH_PATHS = $(SRCROOT)/../../interface`
- `OTHER_LDFLAGS = -lmagic_sr_enable`

## Required files

Paths below are relative to the **repo root** (the parent of `demo/`; the folder name may vary). Model bins live in repo-level `model/` (i.e. `../model/` from `demo/`).

### Android
- `lib/android/libmagic_sr_enable.a`
- `interface/mc_interface.h` (or `demo/android/app/src/main/cpp/mc_*.h`)
- `interface/mc_enable.h`
- `model/magic_gles_highspeed_gpu_params.bin`
- `model/magic_gles_speed_gpu_params.bin`

### iOS
- `lib/ios/libmagic_sr_enable.a`
- `interface/mc_interface.h`
- `interface/mc_enable.h`
- `model/magic_metal_highspeed_gpu_params.bin`
- `model/magic_metal_speed_gpu_params.bin`

### macOS arm64 (`mac_arm/`)
- `lib/mac_arm/libmagic_sr.a` (session / professional path — **not** `libmagic_sr_enable.a`)
- `interface/mc_interface.h`
- Metal / MetalKit / MetalPerformanceShaders / Foundation (system frameworks)
- Model `.bin` files reachable via the `MODEL_ROOT_PATH` macro in `mac_arm/example.c` (point this at your `model/` tree before run)

> iOS has a build phase that copies `$(SRCROOT)/../../model/*.bin` into the app bundle.
> Android build scripts copy GLES bins from `../model/` into `android/app/src/main/assets/model/`.
> `mac_arm` does **not** auto-copy models; edit paths in `example.c`.

## Build

### Camera demos (Android / iOS)

From `demo/`:

```bash
./build_demo.sh          # macOS / Linux → Android APK
# or
build_demo.bat           # Windows → Android APK
# or
build_magnifier_demo.bat # Windows (same checks, writes debug APK path)
```

Manual Android:

```bash
cd android
./gradlew :app:assembleDebug
```

Install:

```bash
adb install -r android/packages/MagicMagnifierSR-android-arm64.apk
# or
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
```

iOS (macOS + Xcode — run on device from Xcode):

```bash
open ios/MagicCameraSR.xcodeproj
# or
cd ios
xcodebuild -project MagicCameraSR.xcodeproj \
  -scheme MagicMagnifierSR \
  -configuration Debug \
  -sdk iphoneos build
```

### macOS arm64 (`mac_arm/`)

Requires an Apple Silicon Mac and Xcode. Before building, update absolute paths in `mac_arm/example.c` (`MODEL_ROOT_PATH`, and optional `HR_PATH` / `LR_PATH` for PSNR tests).

```bash
open mac_arm/example/example.xcodeproj
# or
cd mac_arm/example
xcodebuild -project example.xcodeproj \
  -scheme example \
  -configuration Debug \
  -sdk macosx \
  build
```

Xcode project notes:

- `LIBRARY_SEARCH_PATHS` → `$(SRCROOT)/../../../lib/mac_arm`
- Links `libmagic_sr.a` plus Metal / MetalKit / MetalPerformanceShaders / Foundation
- Sources: `example.c`, `metal.m`, `img_process.c` (sibling of `example/`)

## Runtime

### Camera demos

1. Grant camera permission
2. Select `highspeed` or `speed`
3. Drag the scale slider (or tap tick marks 1–8) — the view crops the center by `1/scale` then runs SR

### mac_arm

Runs as a desktop binary: loads LR/HR (or video) according to macros in `example.c`, calls `MC_Init` → `MC_Process` (Metal RGBA texture or buffer path) → `MC_Uninit`, optionally prints timing / PSNR. This is a developer smoke / quality check, not the magnifier UI.

## Notes

- Android ABI: `arm64-v8a`
- AndroidManifest keeps `INTERNET` (needed for report)
- Camera demos: prefer `libmagic_sr_enable.a` only (do not mix with core-only `libmagic_sr.a`)
- `mac_arm`: uses core-only `libmagic_sr.a` + session API on purpose; do not expect `MC_Enable` symbols in that library

## What not to commit under `demo/`

Build caches and intermediates (safe to delete; regenerated by build):

- `android/.gradle/`
- `android/app/build/`
- `android/app/.cxx/`
- `android/local.properties`
- `ios/**/xcuserdata/`
- `ios/Build/` (if present)
- `mac_arm/example/**/xcuserdata/`
- `mac_arm/**/Build/` (if present)
- `.DS_Store`

Keep source: `android/app/src/`, Gradle wrappers, `ios/MagicCameraSR/`, `mac_arm/` sources + `.xcodeproj`, scripts, optional `android/packages/*.apk`. Link against repo-root `../lib/<platform>/` and load models from repo-root `../model/` (or paths you set in `mac_arm/example.c`). Do not add a `demo/lib/` or `demo/model/` copy.
