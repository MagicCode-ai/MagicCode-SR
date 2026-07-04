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
- `UE_5.2/Android/MagicSR`
- `UE_5.1/Android/MagicSR`

## Verification Status

- `UE_5.6/Android/MagicSR`: verified on UE 5.6.1 with Android NDK `27.0.12077973` on device. Smoke output:
  - `plugin/ue/Samples/AndroidSmokeTest/Saved/AndroidSmokeImages/input_64x64.png`
  - `plugin/ue/Samples/AndroidSmokeTest/Saved/AndroidSmokeImages/output_128x128.png`
- `UE_5.6/IOS/MagicSR`: verified on UE 5.6.1 with iOS device smoke. Smoke output:
  - `plugin/ue/Samples/AndroidSmokeTest/Saved/iOSSmokeImages/input_64x64.png`
  - `plugin/ue/Samples/AndroidSmokeTest/Saved/iOSSmokeImages/output_128x128.png`
- `UE_5.3/Android/MagicSR`: Android smoke was verified previously on UE 5.3.x.
- `UE_5.2/Android/MagicSR`: verified on UE 5.2.1 with Android device smoke.
  - Backends: Vulkan PASS, OpenGLES PASS.
  - Visual outputs: `UE_5.2/Android/SmokeImages/input_vulkan_64x64.png`, `UE_5.2/Android/SmokeImages/output_vulkan_128x128.png`, `UE_5.2/Android/SmokeImages/input_gles_64x64.png`, `UE_5.2/Android/SmokeImages/output_gles_128x128.png`
  - Log: `UE_5.2/Android/Logs/ue-smoke-logcat.txt`
- `UE_5.1/Android/MagicSR`: verified on UE 5.1 with Android device smoke.
  - Backends: Vulkan PASS, OpenGLES PASS.
  - Visual outputs: `UE_5.1/Android/SmokeImages/input_vulkan_64x64.png`, `UE_5.1/Android/SmokeImages/output_vulkan_128x128.png`, `UE_5.1/Android/SmokeImages/input_gles_64x64.png`, `UE_5.1/Android/SmokeImages/output_gles_128x128.png`
  - Log: `UE_5.1/Android/Logs/ue-smoke-logcat.txt`
- `UE_5.4/Android/MagicSR`: verified on UE 5.4.4 with Android device smoke.
  - Backends: Vulkan PASS, OpenGLES PASS.
  - Visual outputs: `UE_5.4/Android/SmokeImages/input_vulkan_64x64.png`, `UE_5.4/Android/SmokeImages/output_vulkan_128x128.png`, `UE_5.4/Android/SmokeImages/input_gles_64x64.png`, `UE_5.4/Android/SmokeImages/output_gles_128x128.png`
  - Log: `UE_5.4/Android/Logs/ue-smoke-logcat.txt`
- `UE_5.7/Android/MagicSR`: verified on UE 5.7 with Android device smoke.
  - Backends: Vulkan PASS, OpenGLES PASS.
  - Visual outputs: `UE_5.7/Android/SmokeImages/input_vulkan_64x64.png`, `UE_5.7/Android/SmokeImages/output_vulkan_128x128.png`, `UE_5.7/Android/SmokeImages/input_gles_64x64.png`, `UE_5.7/Android/SmokeImages/output_gles_128x128.png`
  - Log: `UE_5.7/Android/Logs/ue-smoke-logcat.txt`
- `UE_5.7/IOS/MagicSR`: verified on UE 5.7 with iOS device smoke.
  - Backend: Metal PASS.
  - Visual outputs: `UE_5.7/IOS/SmokeImages/input_metal_64x64.png`, `UE_5.7/IOS/SmokeImages/output_metal_128x128.png`
  - Log: `UE_5.7/IOS/Logs/ue-smoke-ios-console.txt`
- `UE_5.8/Android/MagicSR`: verified on UE 5.8 with Android device smoke.
  - Backends: Vulkan PASS, OpenGLES PASS.
  - Visual outputs: `UE_5.8/Android/SmokeImages/input_vulkan_64x64.png`, `UE_5.8/Android/SmokeImages/output_vulkan_128x128.png`, `UE_5.8/Android/SmokeImages/input_gles_64x64.png`, `UE_5.8/Android/SmokeImages/output_gles_128x128.png`
  - Log: `UE_5.8/Android/Logs/ue-smoke-logcat.txt`
- `UE_5.8/IOS/MagicSR`: verified on UE 5.8 with iOS device smoke.
  - Backend: Metal PASS.
  - Visual outputs: `UE_5.8/IOS/SmokeImages/input_metal_64x64.png`, `UE_5.8/IOS/SmokeImages/output_metal_128x128.png`
  - Log: `UE_5.8/IOS/Logs/ue-smoke-ios-console.txt`

For new verification runs, use `plugin/ue/Samples/AndroidSmokeTest/build_and_run_android_smoke.sh` or `plugin/ue/Samples/AndroidSmokeTest/build_and_run_ios_smoke.sh`, then refresh the matching snapshot directory.
