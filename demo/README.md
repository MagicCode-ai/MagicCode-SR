# Magic Magnifier Demo

`magnifier_demo` is a camera magnifier demo based on `libmagic_sr.a`, with:
- Android (OpenGLES backend)
- iOS/iPadOS (Metal backend)
- Continuous zoom by slider in range `[1.0, 8.0]`
- Only `highspeed` / `speed` modes (no MetalFX/SGSR fallback paths)

If any required file is missing or SR init/process fails, the app reports error and stops.

## 1) Directory Layout

```text
magnifier_demo/
  android/                 # Android Studio project
  ios/                     # Xcode project
  ../lib/
    android/               # Android native headers/libs
      libmagic_sr.a
    ios/                   # iOS headers/libs
      libmagic_sr.a
  ../header/
    mc_interface.h
  ../model/                # SR model bin files
  build_demo.bat           # Windows one-click prepare/build script
  build_demo.sh            # macOS/Linux one-click prepare/build script
```

## 2) Required Files

Before build, ensure these files exist:

### Android
- `../lib/android/libmagic_sr.a`
- `../model/magic_gles_highspeed_gpu_params.bin`
- `../model/magic_gles_speed_gpu_params.bin`

### iOS
- `../lib/ios/libmagic_sr.a`
- `../header/mc_interface.h`
- `../model/magic_metal_highspeed_gpu_params.bin`
- `../model/magic_metal_speed_gpu_params.bin`

> iOS project has a build phase that copies all `../model/*.bin` into app bundle.

## 3) One-Click Build (Android)

Run from `demo` root:

Windows:
```bat
build_demo.bat
```

macOS/Linux:
```bash
bash ./build_demo.sh
```

What it does:
1. Verifies required libs/models exist
2. Creates `android/app/src/main/assets/model` if missing
3. Copies GLES model bins into Android assets
4. Runs `android/gradlew :app:assembleDebug`
5. Prints APK output path

APK output:
- `android\app\build\outputs\apk\debug\app-debug.apk`

## 4) Manual Build

### Android (macOS/Linux/Windows)

```bash
cd android
./gradlew :app:assembleDebug
```

Install:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

### iOS (macOS + Xcode)

Open:
- `ios/MagicCameraSR.xcodeproj`

Or CLI:

```bash
cd ios
xcodebuild -project "MagicCameraSR.xcodeproj" \
  -scheme "MagicCameraSR" \
  -configuration Debug \
  -sdk iphoneos build
```

## 5) Runtime Usage

1. Launch app and grant camera permission
2. Select mode: `highspeed` or `speed`
3. Drag slider to adjust scale `1.0x ~ 8.0x`
4. Output is SR frame from live camera input

## 6) Notes

- Android target ABI is `arm64-v8a`.
- iOS deployment target is configured in Xcode project.
- This demo is intentionally strict: missing model/library or SR errors will stop processing (no fallback).
