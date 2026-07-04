# MagicSR Unity Android Smoke Test

This is a minimal Unity Android project that validates the plugin inside a real Unity Player.

## What It Tests

- Loads `libmagic_sr_unity.so` from `Assets/Plugins/Android/arm64-v8a`.
- Calls `MagicSR_GetVersion`.
- Creates a `NEON + INPUT_BUFFER` session.
- Queries status through `MagicSR_SetParam(..., QueryStatus, ...)`.
- Processes one deterministic 64x64 Y8 frame.
- Verifies output size, `ret == 0`, `error_code == 0`, and output is not all zero.
- Logs `[MagicSRUnitySmoke] result=PASS` or `result=FAIL`.

## Run

First check the local environment:

```bash
./check_environment.sh
```

It verifies Unity Editor, adb, connected Android device, the native plugin `.so`, and the StreamingAssets model.

If Unity batch mode reports no valid license, open Unity Hub, sign in, and activate a Personal or Pro license before running the smoke test.

```bash
cd plugin/unity/Samples/AndroidSmokeTest
MAGIC_SR_MODEL_PATH=/path/to/magic_veryfastx2_cpu_params.bin \
UNITY_BIN="/Applications/Unity/Hub/Editor/<version>/Unity.app/Contents/MacOS/Unity" \
./build_and_run_android_smoke.sh
```

`MAGIC_SR_MODEL_PATH` is copied into `Assets/StreamingAssets/MagicSRModels/` before building. At runtime, the app copies it into `Application.persistentDataPath` and passes that readable path to the native plugin.

## Pass Criteria

The script exits `0` only if logcat contains:

```text
[MagicSRUnitySmoke] result=PASS
```

This validates the actual Unity Android packaging and runtime path. It is stronger than testing the same `.so` from the Camera App because it verifies Unity's Android Player, IL2CPP C# P/Invoke, StreamingAssets model delivery, and native plugin loading together.
