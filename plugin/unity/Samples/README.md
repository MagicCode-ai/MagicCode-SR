# MagicSR Unity Samples

## AndroidSmokeTest

`AndroidSmokeTest/` is a minimal Unity Android project that validates the native plugin in a real Unity Player.

```bash
cd plugin/unity/Samples/AndroidSmokeTest
UNITY_BIN="/Applications/Unity/Hub/Editor/<version>/Unity.app/Contents/MacOS/Unity" \
./build_and_run_android_smoke.sh
```

Pass condition:

```text
[MagicSRUnitySmoke] result=PASS
```

## Minimal runtime sample scene

1. Create a scene with one `RawImage`.
2. Attach `MagicSRRealtimeDemo` to an empty GameObject.
3. Assign:
   - `sourceTexture`: a low-resolution texture (`Texture2D`).
   - `modelPath`: absolute model path on device.
   - `outputView`: the `RawImage`.
4. Put `libmagic_sr_unity.so` into `Assets/Plugins/Android/arm64-v8a/`.
5. Build and run on Android (`arm64-v8a`).

The script logs per-frame return code, `gpu_time`, and `error_code`.
