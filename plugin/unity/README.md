# MagicSR Unity Android Plugin (Stage 1)

## Layout

- `Native/`: Android native wrapper (`libmagic_sr_unity.so`)
- `CSharp/`: Unity C# wrapper and realtime sample behavior
- `Samples/AndroidSmokeTest/`: minimal Unity Android project for real Unity Player validation
- `tests/`: build verification scripts and reports

## Build native library

```bash
cd plugin/unity/Native
bash build_android.sh
```

Output:
- `plugin/unity/Native/build/libmagic_sr_unity.so`

## Verify correctness (native stage)

```bash
cd plugin/unity/tests
bash verify_android_build.sh
```

This generates `plugin/unity/tests/verification_report.md`.

## Unity Android smoke test

Use the sample project to verify the plugin inside a real Unity Android Player:

```bash
cd plugin/unity/Samples/AndroidSmokeTest
UNITY_BIN="/Applications/Unity/Hub/Editor/<version>/Unity.app/Contents/MacOS/Unity" \
./build_and_run_android_smoke.sh
```

The script builds `libmagic_sr_unity.so`, copies C# bindings and the native plugin into the sample Unity project, builds an APK, installs it on a connected Android device, and waits for:

```text
[MagicSRUnitySmoke] result=PASS
```

## Unity integration quick notes

1. Copy `build/libmagic_sr_unity.so` to `Assets/Plugins/Android/arm64-v8a/`.
2. Add `MagicSRNative.cs` and `MagicSRRealtimeDemo.cs` into Unity project.
3. Assign `sourceTexture`, device `modelPath`, and target `RawImage`.
4. Run on Android device for realtime validation.
