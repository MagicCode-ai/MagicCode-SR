# Verified UE Plugin Snapshots

Verified UE plugin snapshots are stored by engine version and platform:

- `UE_5.6/Android/MagicSR`
- `UE_5.6/IOS/MagicSR`
- `UE_5.4/Android/MagicSR`
- `UE_5.7/Android/MagicSR`
- `UE_5.7/IOS/MagicSR`
- `UE_5.8/Android/MagicSR`
- `UE_5.8/IOS/MagicSR`
- `UE_5.3/Android/MagicSR`
- `UE_5.5/Android/MagicSR`
- `UE_5.2/Android/MagicSR`
- `UE_5.1/Android/MagicSR`
- `UE_5.0/Android/MagicSR`

## Verification Status

- `UE_5.6/Android/MagicSR`: re-verified on UE 5.6 (2026-07-19) with Android device `AYYKVB1A14001809` (ELZ-AN00).
  - APIs: **CreateSession/Process** PASS (Vulkan + OpenGLES, RGBA PNG 64→128); **Enable / Enable_3params / Enable_4params** PASS (RGBA8 64×64 @ scale 2).
  - Log: `UE_5.6/Android/Logs/ue-smoke-logcat.txt`
- `UE_5.6/IOS/MagicSR`: re-verified on UE 5.6 (2026-07-19) with Rui’s iPhone.
  - APIs: **CreateSession/Process** PASS (Metal R8 64→128); **Enable / Enable_3params / Enable_4params** PASS (RGBA8 64×64 @ scale 2).
  - Log: `UE_5.6/IOS/Logs/ue-smoke-ios-console.txt`
- `UE_5.3/Android/MagicSR`: re-verified on UE 5.3 (2026-07-19) with Android device `AYYKVB1A14001809`.
  - APIs: **CreateSession/Process** PASS (Vulkan + OpenGLES, RGBA PNG 64→128); **Enable / Enable_3params / Enable_4params** PASS (RGBA8 64×64 @ scale 2).
  - Log: `UE_5.3/Android/Logs/ue-smoke-logcat.txt`
- `UE_5.5/Android/MagicSR`: re-verified on UE 5.5 (2026-07-19) with Android device `AYYKVB1A14001809`.
  - APIs: **CreateSession/Process** PASS (Vulkan + OpenGLES, RGBA PNG 64→128); **Enable / Enable_3params / Enable_4params** PASS (RGBA8 64×64 @ scale 2).
  - Log: `UE_5.5/Android/Logs/ue-smoke-logcat.txt`
- `UE_5.5/IOS/MagicSR`: **blocked on iOS 26** (2026-07-17). Built/signed/installed on Rui’s iPhone (iPhone 14 Pro, iOS 26.5.2), but the process aborts at launch with `libc++abi: __cxa_guard_acquire detected recursive initialization` (signal 6) before MagicSR smoke runs. This matches a known UE 5.4/5.5 + iOS 26 engine issue; the same device runs UE 5.8 iOS smoke successfully. Need an iOS ≤18 device (or UE ≥5.6) to complete UE 5.5 iOS verification. Console: `plugin/ue/Samples/AndroidSmokeTest/ue-smoke-ios-console.txt`.
- `UE_5.2/Android/MagicSR`: verified on UE 5.2.1 with Android device smoke.
  - Backends: Vulkan PASS, OpenGLES PASS.
  - Log: `UE_5.2/Android/Logs/ue-smoke-logcat.txt`
- `UE_5.1/Android/MagicSR`: verified on UE 5.1 with Android device smoke.
  - Backends: Vulkan PASS, OpenGLES PASS.
  - Log: `UE_5.1/Android/Logs/ue-smoke-logcat.txt`
- `UE_5.0/Android/MagicSR`: verified on UE 5.0 with Android device smoke.
  - Backends: Vulkan PASS, OpenGLES PASS.
  - Log: `UE_5.0/Android/Logs/ue-smoke-logcat.txt`
- `UE_5.4/Android/MagicSR`: re-verified on UE 5.4.4 (2026-07-19) with Android device `AYYKVB1A14001809`.
  - APIs: **CreateSession/Process** PASS (Vulkan + OpenGLES, RGBA PNG 64→128); **Enable / Enable_3params / Enable_4params** PASS (RGBA8 64×64 @ scale 2).
  - Log: `UE_5.4/Android/Logs/ue-smoke-logcat.txt`
- `UE_5.7/Android/MagicSR`: re-verified on UE 5.7 (2026-07-19) with Android device `AYYKVB1A14001809`.
  - APIs: **CreateSession/Process** PASS (Vulkan + OpenGLES, RGBA PNG 64→128); **Enable / Enable_3params / Enable_4params** PASS (RGBA8 64×64 @ scale 2).
  - Log: `UE_5.7/Android/Logs/ue-smoke-logcat.txt`
- `UE_5.7/IOS/MagicSR`: re-verified on UE 5.7 (2026-07-19) with Rui’s iPhone.
  - APIs: **CreateSession/Process** PASS (Metal R8 64→128); **Enable / Enable_3params / Enable_4params** PASS (RGBA8 64×64 @ scale 2).
  - Log: `UE_5.7/IOS/Logs/ue-smoke-ios-console.txt`
- `UE_5.8/Android/MagicSR`: re-verified on UE 5.8 (2026-07-19) with Android device `AYYKVB1A14001809`.
  - APIs: **CreateSession/Process** PASS (Vulkan + OpenGLES, RGBA PNG 64→128); **Enable / Enable_3params / Enable_4params** PASS (RGBA8 64×64 @ scale 2).
  - Log: `UE_5.8/Android/Logs/ue-smoke-logcat.txt`
- `UE_5.8/IOS/MagicSR`: re-verified on UE 5.8 (2026-07-19) with Rui’s iPhone.
  - APIs: **CreateSession/Process** PASS (Metal R8 64→128); **Enable / Enable_3params / Enable_4params** PASS (RGBA8 64×64 @ scale 2).
  - Log: `UE_5.8/IOS/Logs/ue-smoke-ios-console.txt`

For new verification runs, use `plugin/ue/Samples/AndroidSmokeTest/build_and_run_android_smoke.sh` or `plugin/ue/Samples/AndroidSmokeTest/build_and_run_ios_smoke.sh`, then refresh the matching snapshot directory.
