# MagicSR Unity Plugin User Guide

## 1) Overview

The MagicSR Unity plugin provides C# + native bindings so Unity apps can run MagicSR super-resolution on mobile platforms.

Supported runtime platforms:

- Android (primary runtime backends: Vulkan and OpenGLES)
- iOS (Metal path via iOS static wrapper)

Core native libraries used by this plugin:

- Android: `lib/android/libmagic_sr_enable.a` (core + `MC_Enable`)
- iOS: `lib/ios/libmagic_sr_enable.a` (core + `MC_Enable`)

## 2) Deliverables

Source deliverables:

- Native Android wrapper sources: `plugin/unity/Native/`
- C# bindings: `plugin/unity/CSharp/MagicSRNative.cs`
- Public entry: `plugin/unity/CSharp/MagicSR.cs` (`Enable` / `Disable`)
- Built-in camera sample: `plugin/unity/CSharp/MagicSRCameraUpscaler.cs` (calls the same `Enable`)
- Demo: `plugin/unity/CSharp/MagicSRRealtimeDemo.cs`
- Sample Unity project: `plugin/unity/Samples/AndroidSmokeTest/`

Build output deliverables:

- Android native plugin: `plugin/unity/Native/build/libmagic_sr_unity.so`
- iOS static plugin: `plugin/unity/Native/build_ios/libmagic_sr_unity_ios.a`

Verification deliverables:

- Validation scripts/reports: `plugin/unity/tests/`

## 3) Public API (C#)

### One-call integration (recommended)

Only two public calls — any GPU texture source (camera RT, UI, custom pass, etc.):

```csharp
using MagicSR.UnityPlugin;

// Each frame (or when input is ready):
IntPtr output = MagicSR.Enable(inputRT, 1.5f);           // Texture / RenderTexture
// or: MagicSR.Enable(inputRT.GetNativeTexturePtr(), 1.5f);

// When finished:
MagicSR.Disable();  // only valid release for output pointer
```

`scale` is float in `[1, 8]`; `<=0` defaults to `2.0`. Session is reused when scale/size are unchanged.

**Input format for Enable:** RGBA8Unorm (Metal / GLES) or RGB8Unorm (Vulkan).

**Camera / URP / HDRP:** blit (or resolve) color into a `RenderTexture`, then call the same `MagicSR.Enable(rt, scale)`.  
Built-in sample: attach `MagicSRCameraUpscaler` (uses `OnRenderImage` → `Enable`).

Models (resolved by native `MC_Enable`, typically under StreamingAssets / bundle):

- `StreamingAssets/MagicSRModels/magic_veryfast_gpu_params.bin`
- `StreamingAssets/MagicSRModels/magic_veryfast_gles_params.bin` (OpenGLES)

User manual: [`doc/User_Guide.md`](../../doc/User_Guide.md) (English) · [`doc/用户使用说明书.md`](../../doc/用户使用说明书.md) (中文)

### Advanced API

Primary session API is `MagicSRSession` in `MagicSRNative.cs`.

**Preferred path:**

1. `MagicSR.Enable(inputTexture, scale)` — wraps `MC_Enable` (returns output texture)
   - also: `Enable_3params` / `Enable_4params` for mode / backend
2. `MagicSR.Disable()` — wraps `MC_Disable`
   - or `CreateEx` + `ProcessTexture` for explicit in/out control

Recommended pairing for `CreateEx` (advanced session API, not `Enable`):

| Platform | inputType | backend |
|----------|-----------|---------|
| Android (Vulkan) | `TextureRgb8Unorm` | `Vulkan` |
| Android (GLES) | `TextureRgb8Unorm` or `TextureR8Unorm` | `OpenGLES` |
| iOS | `TextureRgb8Unorm` or `TextureR8Unorm` | `Metal` |

**`MagicSR.Enable` always expects RGBA8 / RGB8Unorm textures** (see user manual).

**Fallback path (CPU / Buffer):**

1. `Create(...)`  (Neon/CPU buffer session)
2. `Process(inputY, outputY)`

Key methods:

- `GetVersion()`
- `Create(...)` — buffer/Y8 helper
- `CreateEx(..., inputType, backend, ...)` — explicit session
- `Process(inputY, outputY)` — buffer/Y8 only
- `ProcessTexture(inputTexture, outputTexture)` — explicit GPU in/out
- `SetParam(...)`
- `Destroy()`
