# Unity Android Runtime Validation Guide

## Goal

Validate that runtime pipeline works in Unity Android player:
- `Create -> Process (multiple frames) -> Destroy`
- output size matches scale
- status/error fields are readable each frame

## Steps

1. Build native plugin:
   - `bash plugin/unity/tests/verify_android_build.sh`
2. Copy `.so`:
   - `plugin/unity/Native/build/libmagic_sr_unity.so`
   - to Unity project `Assets/Plugins/Android/arm64-v8a/`
3. Add scripts:
   - `plugin/unity/CSharp/MagicSRNative.cs`
   - `plugin/unity/CSharp/MagicSRRealtimeDemo.cs`
4. Configure demo scene:
   - assign source texture
   - assign model absolute path on device
   - assign output `RawImage`
5. Build Unity Android player and run on device.
6. Collect logs:
   - `bash plugin/unity/tests/collect_runtime_log.sh`

## Pass criteria

- no crash during continuous processing
- `Process` returns `0` for repeated frames
- `error_code == 0` in normal path
- output texture resolution equals `input * scale`
