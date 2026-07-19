# MagicCode Super-Resolution (MagicSR) User Guide

Version: v1.1.2  
Applies to: Native SDK / Unity plugin / Unreal Engine plugin  
Platforms: Android (Vulkan / OpenGLES), iOS / macOS (Metal), Windows (OpenGL)

中文版：[`用户使用说明书.md`](用户使用说明书.md)

---

## 1. Product Overview

MagicSR is the MagicCode super-resolution SDK. It upscales an input GPU texture and enhances detail. Typical uses include game frames, camera preview, and UI textures.

**Recommended integration** (two steps):

1. `Enable`: pass one GPU texture; receive the upscaled output texture  
2. `Disable`: release the internal session and output texture when finished  

You do not need to create a session, pick a backend, or configure input type manually; `Enable` handles that internally.

| Layer | Public API |
|-------|------------|
| Native | `MC_Enable` / `MC_Disable` |
| Unity | `MagicSR.Enable` / `MagicSR.Disable` |
| Unreal | `UMagicSRBlueprintLibrary::Enable` / `Disable` |

Plugin public APIs are thin wrappers around Native `MC_Enable` / `MC_Disable`. Application code should call the plugin APIs, not `MC_*` directly.

---

## 2. Quick Start

### 2.1 Unity

```csharp
using MagicSR.UnityPlugin;
using UnityEngine;

public class MyUpscale : MonoBehaviour
{
    public RenderTexture inputRT;   // RGBA8 input
    public float scale = 2.0f;

    void Start()
    {
        // Recommended: set model dir to a writable path (contains magic_veryfast_*_params.bin)
        MagicSR.SetModelDir(Application.persistentDataPath + "/MagicSRModels");
    }

    void OnRenderImage(RenderTexture src, RenderTexture dst)
    {
        // Per frame: upscale one frame (session is reused when scale/size stay the same)
        System.IntPtr outNative = MagicSR.Enable(scale, src);
        if (outNative == System.IntPtr.Zero)
        {
            Graphics.Blit(src, dst);
            return;
        }

        // Create/update an external Texture from the output native handle, then blit
        // (see MagicSRCameraUpscaler)
        // ...
        Graphics.Blit(src, dst); // placeholder
    }

    void OnDestroy()
    {
        MagicSR.Disable(); // only valid release path
    }
}
```

You can also pass a native pointer:

```csharp
IntPtr outPtr = MagicSR.Enable(2.0f, inputRT.GetNativeTexturePtr());
MagicSR.Disable();
```

Built-in camera sample: attach `MagicSRCameraUpscaler` (Built-in pipeline `OnRenderImage` → `Enable`).

### 2.2 Unreal Engine (C++)

```cpp
#include "MagicSRBlueprintLibrary.h"

// Per frame (or when input is ready)
int64 OutNativeTex = UMagicSRBlueprintLibrary::Enable(/*Scale=*/2.0f, InputNativeTexture);
if (OutNativeTex == 0)
{
    // handle failure
}

// When finished
UMagicSRBlueprintLibrary::Disable();
```

Blueprint: `MagicSR | Easy` → **Enable** / **Disable**.

`InputNativeTexture` is a native GPU handle (`int64`):

| Backend | What to pass |
|---------|--------------|
| Metal | `MTLTexture*` cast to `int64` |
| OpenGLES | `GLuint` texture ID cast to `int64` |
| Vulkan | `VulkanTexture*` / pointer to `VkImage` cast to `int64` |

### 2.3 Native C

```c
#include "mc_enable.h"

void* out = MC_Enable(2.0f, input_texture);  // RGBA GPU texture
if (out == NULL) { /* failure */ }

/* Use out (borrowed pointer — do not free / CFRelease / glDelete) */

MC_Disable(NULL);  // or MC_Disable(out); argument may be ignored
```

---

## 3. Enable Behavior

| Item | Description |
|------|-------------|
| Role | Lazy-init session + process one frame + return output GPU texture |
| `scale` | `float`, range `[1.0, 8.0]`; defaults to `2.0` when `<= 0` |
| Session reuse | Same `scale` and input width/height reuse the internal session and output texture |
| Return value | Output texture pointer; failure is `NULL` / `IntPtr.Zero` / `0` |
| Ownership | Library owns the output; release **only** via `Disable` |

### 3.1 Input Texture Format (Important)

`Enable` requires **RGBA-family** GPU textures:

| Platform / Backend | Input format | `Enable` backend |
|--------------------|--------------|------------------|
| iOS / macOS Metal | `RGBA8Unorm` (`MTLTexture*`) | Metal |
| Windows OpenGL | `RGBA8` (`GLuint` as `void*`) | OpenGL |
| Android OpenGLES | `RGBA8` (`GLuint` as `void*`) | OpenGLES |
| Android Vulkan | `RGB8Unorm` / `RGBA8` | Vulkan |

Unity example: use a `RenderTexture` with `GraphicsFormat.R8G8B8A8_UNorm`.  
Do not pass single-channel R8 textures to `Enable`.

### 3.2 Size and Performance Tips

- Input width/height should generally be within the SDK-supported range (commonly about 64–4032).  
- On Android Vulkan, if `VkImage` size cannot be queried, the implementation may fall back to a fixed size (currently about 720×1280); keep input texture size consistent with the session.  
- Frequent changes to `scale` or size trigger rebuilds and hurt performance.

---

## 4. Model Files

`Enable` looks up models in this order (summary):

1. Environment variable `MAGIC_SR_MODEL` (full file path)  
2. Environment variable `MAGIC_SR_MODEL_DIR` (directory + default filename)  
3. Relative paths / platform default directories (e.g. Android `Documents/MagicSRModels`)  
4. iOS: models embedded in the App Bundle  

Common model filenames:

| Scenario | File |
|----------|------|
| Metal / Vulkan (GPU) | `magic_veryfast_gpu_params.bin` |
| Windows OpenGL | `magic_veryfast_gpu_params.bin` / `magic_gl_highspeed_gpu_params.bin` |
| OpenGLES | `magic_veryfast_gles_params.bin` |

### Unity

- Place models under `StreamingAssets/MagicSRModels/`  
- At runtime, copy to `persistentDataPath/MagicSRModels/` and call:

```csharp
MagicSR.SetModelDir(Application.persistentDataPath + "/MagicSRModels");
```

### Unreal

- Place models under `Content/MagicSRModels/`  
- After packaging, ensure device access (e.g. copy to app sandbox `Documents/MagicSRModels`, or set `MAGIC_SR_MODEL` / `MAGIC_SR_MODEL_DIR`)

---

## 5. Integration Steps

### 5.1 Unity

1. Import C# scripts: `plugin/unity/CSharp/` (at least `MagicSR.cs`, `MagicSRNative.cs`)  
2. Build and place the Native plugin:  
   - Android: `libmagic_sr_unity.so` → `Assets/Plugins/Android/`  
   - iOS: `libmagic_sr_unity_ios.a` → `Assets/Plugins/iOS/`  
3. Place models under `StreamingAssets/MagicSRModels/`  
4. Call `MagicSR.SetModelDir` (recommended) → `Enable` → `Disable`

Build scripts:

- `plugin/unity/Native/build_android.sh`  
- `plugin/unity/Native/build_ios.sh`  
- Sample project: `plugin/unity/Samples/AndroidSmokeTest/`

### 5.2 Unreal Engine

1. Copy the plugin into the project: `<YourProject>/Plugins/MagicSR`  
2. Link the release libraries:  
   - `lib/android/libmagic_sr.a`  
   - `lib/ios/libmagic_sr.a`  
3. Enable MagicSR in `.uproject`, regenerate project files / open the editor  
4. Call `Enable` / `Disable` from Blueprint or C++  
5. Place models under `Content/MagicSRModels/`

Sample project: `plugin/ue/Samples/AndroidSmokeTest/`

### 5.3 Native

1. Include headers: `interface/mc_enable.h` (and `mc_interface.h`)  
2. Link `libmagic_sr.a` (android / ios as appropriate)  
3. On iOS, also link Metal / MetalKit and related system frameworks  
4. Call `MC_Enable` / `MC_Disable`

---

## 6. Memory and Lifetime (Required Reading)

1. **The output texture from `Enable` is a borrowed pointer**  
   - Do not `free` / `delete` / `CFRelease` / `glDeleteTextures` / `DestroyImmediate`  
   - iOS Metal: an incorrect `CFRelease` can `abort` at the release point  
2. **The only valid release path is `Disable()`**  
3. After `Disable`, all previously returned output pointers are invalid  
4. You may call `Enable` repeatedly; internal resources are reused when `scale` and size stay the same  
5. Calling `Enable` again after `Disable` creates a new session  

---

## 7. Advanced API (Optional)

For explicit control of input/output textures, backend, or CPU buffer paths, use the session API:

| Platform | API |
|----------|-----|
| Native | `MC_Init` / `MC_Process` / `MC_Uninit` |
| Unity | `MagicSRSession.CreateEx` + `ProcessTexture` |
| Unreal | `CreateSessionEx` + `ProcessUTexture` / `ProcessNativeTexture` |

See `mc_interface.h` for `InputType` / `Backend` values.  
**For typical game / camera integration, prefer `Enable` / `Disable`; the session API is not required.**

More detailed plugin guides (English):

- `plugin/unity/USAGE_GUIDE.md`  
- `plugin/ue/USAGE_GUIDE.md`

---

## 8. FAQ

**Q: Enable returns a null pointer?**  
A: Check that the model is readable, `SetModelDir` / model paths are correct, input texture format is RGBA, size is valid, and the GPU backend is available.

**Q: Why can't iOS use R8 anymore?**  
A: Current `MC_Enable` defaults to RGBA input. R8 applies only to older session-based Metal/GLES advanced paths, not to `Enable`.

**Q: Output is corrupted / all black?**  
A: Confirm RGBA8 input, no illegal release of the output, Vulkan input size matches the session; check MagicSR / Unity / UE error codes in logs.

**Q: How do I integrate camera / URP / HDRP?**  
A: Resolve / blit the frame to an RGBA `RenderTexture` (or equivalent native texture), then call the same `Enable(scale, texture)`. There is no dedicated Camera Enable API.

**Q: Can multiple instances Enable at once?**  
A: The `Enable` path is a process-wide singleton. For concurrent streams, use the session API, or share one `Enable` session serially.

---

## 9. Version and Deliverables

| Item | Path |
|------|------|
| Native Android library | `lib/android/libmagic_sr.a` |
| Native iOS library | `lib/ios/libmagic_sr.a` |
| Enable header | `interface/mc_enable.h` |
| Unity plugin | `plugin/unity/` |
| UE plugin | `plugin/ue/` |
| License | `doc/MagicCode Super-Resolution Software End User License Agreement (EULA) v1.1.0.pdf` |

---

## 10. Support

For technical issues, contact MagicCode support and include: platform, engine version, graphics backend, model filename, full error logs, and a minimal reproducible project when possible.
