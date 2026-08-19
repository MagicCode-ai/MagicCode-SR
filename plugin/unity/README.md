# MagicSR Unity Android Plugin (Stage 1)

## Documentation

- Full usage manual: `plugin/unity/USAGE_GUIDE.md`

## Layout

- `Native/`: Android native wrapper (`libmagic_sr_unity.so`)
- `CSharp/`: Unity C# wrapper and realtime sample behavior

## Build native library

```bash
cd plugin/unity/Native
bash build_android.sh
```

Output:
- `plugin/unity/Native/build/libmagic_sr_unity.so`

## Validation

For runtime validation workflow and troubleshooting, see:

- `plugin/unity/USAGE_GUIDE.md`

## Unity integration quick notes

1. Copy `build/libmagic_sr_unity.so` to `Assets/Plugins/Android/arm64-v8a/`.
2. Add `MagicSRNative.cs` and `MagicSRRealtimeDemo.cs` into Unity project.
3. Assign `sourceTexture`, device `modelPath`, and target `RawImage`.
4. Run on Android device for realtime validation.
