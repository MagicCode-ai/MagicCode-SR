# MagicCode Super-Resolution (MagicSR) User Guide

Version: v1.1.3  
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

### 1.1 Native deliverables: two integration paths

Native SDK ships **two** static libraries. Pick **one** path — do not mix them:

| Path | Who it is for | Link | Include | Call |
|------|---------------|------|---------|------|
| **Simple (recommended)** | Most apps / games / camera | `libmagic_sr_enable.a` (core + Enable) | `mc_enable.h` | `MC_Enable*` / `MC_Disable` |
| **Professional / advanced** | Explicit session / backend / CPU buffer control | `libmagic_sr.a` (core only) | `mc_interface.h` | `MC_Init` / `MC_Process` / `MC_Uninit` |

| Item | Simple path | Professional path |
|------|-------------|-------------------|
| Header | `interface/mc_enable.h` | `interface/mc_interface.h` |
| Android library | `lib/android/libmagic_sr_enable.a` | `lib/android/libmagic_sr.a` |
| iOS library | `lib/ios/libmagic_sr_enable.a` | `lib/ios/libmagic_sr.a` |

Notes:

- Simple path: you do **not** need to add `mc_enable.c` to your app; Enable is already inside `libmagic_sr_enable.a`.  
- Core-only `libmagic_sr.a` does **not** export `MC_Enable*` — linking it alone causes undefined-symbol errors for Enable.  
- **Unity / Unreal plugins:** already ship Enable inside the plugin binary; game C# / Blueprint code does not link these `.a` files directly.  
- **Native apps:** choose a path above (see §5.3). On Apple, still link Metal / MetalKit system frameworks.  
- Release packages usually already include `libmagic_sr_enable.a`. To **rebuild** the combined library from core + Enable sources locally, see `tools/build_enable_lib.sh` in §5.3 A.

---

## 2. Quick Start

**Argument order (all layers):** `(input_texture, scale)` — texture first, scale second.

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
        // Recommended: set model dir to a writable path (see §4 model table)
        MagicSR.SetModelDir(Application.persistentDataPath + "/MagicSRModels");
    }

    void OnRenderImage(RenderTexture src, RenderTexture dst)
    {
        // Per frame: upscale one frame (session is reused when scale/size stay the same)
        // Signature: Enable(inputTexture, scale)
        System.IntPtr outNative = MagicSR.Enable(src, scale);
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
IntPtr outPtr = MagicSR.Enable(inputRT.GetNativeTexturePtr(), 2.0f);
MagicSR.Disable();
```

Built-in camera sample: attach `MagicSRCameraUpscaler` (Built-in pipeline `OnRenderImage` → `Enable`).

### 2.2 Unreal Engine (C++)

```cpp
#include "MagicSRBlueprintLibrary.h"

// Per frame (or when input is ready)
// Signature: Enable(InputNativeTexture, Scale)
int64 OutNativeTex = UMagicSRBlueprintLibrary::Enable(InputNativeTexture, /*Scale=*/2.0f);
if (OutNativeTex == 0)
{
    // handle failure — check log for [MagicSR][Enable] err=...
}

// When finished
UMagicSRBlueprintLibrary::Disable();
```

Blueprint: `MagicSR | Easy` → **Enable** / **Disable**.

> **Deprecated:** `EnableNative(Scale, InputNativeTexture)` keeps the old **scale-first** parameter order for compatibility only. Prefer `Enable(InputNativeTexture, Scale)`. Do not mix the two orders.

`InputNativeTexture` is a native GPU handle (`int64`):

| Backend | What to pass |
|---------|--------------|
| Metal | `MTLTexture*` cast to `int64` |
| OpenGLES | `GLuint` texture ID cast to `int64` |
| Vulkan | `VulkanTexture*` / pointer to `VkImage` cast to `int64` |

### 2.3 Native C

Before calling `MC_Enable`, link `libmagic_sr_enable.a` and `#include "mc_enable.h"`. See §1.1 and §5.3.

```c
#include "mc_enable.h"

/* Signature: MC_Enable(input_texture, scale) */
void* out = MC_Enable(input_texture, 2.0f);  // RGBA GPU texture
if (out == NULL) { /* failure — see [MagicSR][Enable] err=<code> in logs */ }

/* Use out (borrowed pointer — do not free / CFRelease / glDelete) */

MC_Disable(NULL);  // or MC_Disable(out); argument may be ignored
```

Optional variants (same argument order): `MC_Enable_3params(input, scale, mode)`, `MC_Enable_4params(input, scale, mode, backend)`.

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

### 3.2 Size Hint (Android GLES / Vulkan — Required)

On **Android OpenGLES** and **Android Vulkan**, the SDK cannot reliably query input texture size. You **must** call `SetInputSizeHint` / `MC_Enable_SetInputSizeHint(width, height)` before `Enable`. Without a valid hint (`width`/`height` in `[64, 4032]`), `Enable` fails with `MC_ENABLE_ERROR_SIZE_HINT_REQUIRED` (`-300006`).

Unity `Enable(Texture, scale)` sets the hint automatically from the `Texture` size. Raw `IntPtr` / native / UE paths must set it explicitly.

Metal and Windows OpenGL can usually query size; the hint remains optional there.

### 3.3 Size and Performance Tips

- Input width/height must be in `[64, 4032]`.  
- Frequent changes to `scale` or size trigger rebuilds and hurt performance.  
- On failure, check logs for `[MagicSR][Enable] err=<code>` (see `mc_enable.h` for `MC_ENABLE_ERROR_*`).

---

## 4. Model Files

You do **not** need to hand-copy model files for most workflows. Use the setup script first, then the short steps below.

### 4.0 One-command model setup (recommended)

`tools/setup_models.sh` (Windows: `tools\setup_models.bat`) **only copies files**. It does **not** call `SetModelDir` / `Enable` for you.  
Source is always `<repo>/model/`. By default it copies the Enable **GPU** `.bin` set (Metal / OpenGL / GLES / Vulkan × highspeed + speed — 8 files; missing names are skipped).

Run from the repo root. See `./tools/setup_models.sh --help` for the full list.

#### What each subcommand does

| Command | Copies models to | Typical use | What you still must do |
|---------|------------------|-------------|------------------------|
| `demo` (default if you omit the command) | Android: `demo/android/app/src/main/assets/model/`<br>iOS: `demo/ios/MagicCameraSR/MagicSRModels/` | Stock camera demos only | Build/install the demo as usual; the demo sets its model path |
| `local` | `<repo>/MagicSRModels/` | Native / local debugging | **Must** call `MC_Enable_SetModelDir("<absolute path to that folder>")` (or `SetModelPath` to one `.bin`). Dropping files there does **not** make every app find them automatically |
| `unity --project <UnityRoot>` | `<project>/Assets/StreamingAssets/MagicSRModels/` | Unity integration | At runtime, copy to a writable folder then `MagicSR.SetModelDir(...)` (see §4.2) |
| `ue --project <UERoot>` | `<project>/Content/MagicSRModels/` | UE packaging | Runtime does **not** auto-read `Content/`; copy/adb on device and `SetModelDir` (see §4.2) |
| `dir --dest <path>` | Your chosen directory | Custom layout | `MC_Enable_SetModelDir("<absolute path>")` |
| `adb` | Device `/storage/emulated/0/Documents/MagicSRModels/` | Quick Android device check | `MC_Enable_SetModelDir("/storage/emulated/0/Documents/MagicSRModels")` |
| `all` | `demo` + `local` (plus Unity/UE if `--project-unity` / `--project-ue` given) | Prep several targets at once | Still set paths in code per target above |

```bash
# macOS / Linux examples
./tools/setup_models.sh demo
./tools/setup_models.sh local
./tools/setup_models.sh unity --project /path/to/YourUnityProject
./tools/setup_models.sh ue --project /path/to/YourUEProject
./tools/setup_models.sh adb
```

```bat
REM Windows
tools\setup_models.bat demo
tools\setup_models.bat local
tools\setup_models.bat unity --project C:\path\to\YourUnityProject
tools\setup_models.bat ue --project C:\path\to\YourUEProject
```

**Common misunderstandings:**

- `local` is **not** a global auto-search. It only creates `<repo>/MagicSRModels/`. The relative fallback `MagicSRModels/<filename>` may hit it **only if** the process cwd is the repo root. Shipped apps / Unity / UE almost never have that cwd — **always set `SetModelDir` / `SetModelPath` explicitly**.
- `demo` vs `build_demo.sh`: the latter also copies GLES models into Android assets when building; `setup_models.sh demo` prepares **both** Android and iOS demo folders as a “stage the models first” step.

### 4.1 Set the model from code (recommended API)

`SetModelPath` / `SetModelDir` are **not** mandatory C parameters, but for real integrations treat them as **required in practice**: without them, Enable falls back to §4.4 implicit search, which often fails on mobile/engines (`-300009`).

Call **before** `Enable`:

| Layer | One file (`.bin`) | Directory of bins |
|-------|-------------------|-------------------|
| Native | `MC_Enable_SetModelPath("/abs/path/model.bin")` | `MC_Enable_SetModelDir("/abs/path/MagicSRModels")` |
| Unity | `MagicSR.SetModelPath(path)` | `MagicSR.SetModelDir(dir)` |
| Unreal | `SetModelPath(Path)` | `SetModelDir(Dir)` |

`SetModelPath` wins over `SetModelDir`. Pass empty/null to clear. Do **not** use environment variables for model paths.

`SetModelDir` looks up the known default basenames (see §4.3) under that folder **recursively** (up to 4 levels). It does not scan arbitrary `.bin` names.

### 4.2 After the script (per platform)

**Unity（recommended）**

1. Run `./tools/setup_models.sh unity --project <YourUnityProject>`  
   (fills `Assets/StreamingAssets/MagicSRModels/`)  
2. At runtime, copy StreamingAssets → a writable folder and call:

```csharp
MagicSR.SetModelDir(Application.persistentDataPath + "/MagicSRModels");
// or: MagicSR.SetModelPath(fullPathToOneBin);
```

**Unreal（recommended）**

1. Run `./tools/setup_models.sh ue --project <YourUEProject>`  
   (fills `Content/MagicSRModels/` for packaging)  
2. `Content/MagicSRModels/` is **not** auto-read by `Enable` at runtime. On device, either run `./tools/setup_models.sh adb` or copy the bins, then:

```cpp
UMagicSRBlueprintLibrary::SetModelDir(TEXT("/storage/emulated/0/Documents/MagicSRModels"));
// or one file:
// UMagicSRBlueprintLibrary::SetModelPath(TEXT("/storage/emulated/0/Documents/MagicSRModels/magic_vulkan_highspeed_gpu_params.bin"));
```

| Target | Concrete runtime path that works today |
|--------|----------------------------------------|
| Android | `/storage/emulated/0/Documents/MagicSRModels/<model>.bin` (also `/sdcard/Documents/MagicSRModels/`) |
| iOS | App sandbox / bundle path to `<model>.bin`, then `MC_Enable_SetModelPath(fullPath)` (demo pattern) |

Do **not** rely on relative `MagicSRModels/` alone on mobile — that is relative to the process working directory and is often not where you expect after packaging.

**Native（recommended）**

1. `./tools/setup_models.sh local` or `dir --dest <path>` (or `adb` for Android device)  
2. Set path from code before `MC_Enable`:

```c
/* Prefer: full path to one .bin */
MC_Enable_SetModelPath("/absolute/path/to/magic_gles_highspeed_gpu_params.bin");

/* Or: directory that contains the default filename for your backend */
MC_Enable_SetModelDir("/absolute/path/to/MagicSRModels");
```

| Platform | Practical absolute locations |
|----------|------------------------------|
| Android | App `filesDir` / assets extracted path; or `/storage/emulated/0/Documents/MagicSRModels/` |
| iOS | Bundle resource path, or sandbox `Documents/...` (see `demo/ios`) |
| Windows | e.g. `C:\\YourApp\\MagicSRModels\\magic_gl_highspeed_gpu_params.bin` |
| macOS | App resource / known install dir absolute path |

Relative search (`./MagicSRModels/`, `./<name>.bin`) only works if the process cwd happens to be that folder — fine for local tools, unreliable for shipped apps.

### 4.3 Platform × Backend × Default Model Name

| Platform | Backend | HighSpeed (`HIGH_SPEED_MODE`) | Speed (`SPEED_MODE`) |
|----------|---------|-------------------------------|----------------------|
| iOS / macOS | Metal | `magic_metal_highspeed_gpu_params.bin` | `magic_metal_speed_gpu_params.bin` |
| Windows | OpenGL | `magic_gl_highspeed_gpu_params.bin` | `magic_gl_speed_gpu_params.bin` |
| Android | OpenGLES | `magic_gles_highspeed_gpu_params.bin` | `magic_gles_speed_gpu_params.bin` |
| Android | Vulkan | `magic_vulkan_highspeed_gpu_params.bin` | `magic_vulkan_speed_gpu_params.bin` |

Legacy aliases still accepted: `magic_veryfast_gpu_params.bin`, `magic_veryfast_gles_params.bin`, `magic_highspeed_gpu_params.bin`.

### 4.4 How `Enable` finds the model (reference)

Search order:

1. **Explicit file**: `MC_Enable_SetModelPath` / `MagicSR.SetModelPath` / UE `SetModelPath` (if set and readable)  
2. For each default basename `<name>` in §4.3, try in order:  
   - **Explicit dir**: recursive lookup under `SetModelDir` (depth ≤ 4)  
   - **Relative dir**: `MagicSRModels/<name>` — relative to the process **current working directory (cwd)**. This is **not** “find any folder named MagicSRModels on disk”, and it is **not** automatically `<repo>/MagicSRModels`  
   - **Relative file**: `./<name>` (also cwd-relative)  
   - **Android fixed absolute fallbacks** (when SetModel* was not set):  
     `/sdcard/Documents/MagicSRModels/<name>`,  
     `/storage/emulated/0/Documents/MagicSRModels/<name>`,  
     `/storage/emulated/0/Documents/<name>`  
3. **iOS / macOS**: App Bundle lookup by default filename  

So:

| Approach | Reliable? |
|----------|-----------|
| `SetModelPath` / `SetModelDir` with absolute paths | Yes — use this for shipping |
| `setup_models.sh adb` + Android Documents fallback | OK for device debug; still prefer explicit `SetModelDir` |
| Only `setup_models.sh local`, no SetModel* | May work only if cwd is the repo root — **do not rely on this** |
| Relative `MagicSRModels/` alone | Depends on cwd — unreliable in packaged apps |

If no file is found, `Enable` fails with `MC_ENABLE_ERROR_MODEL_NOT_FOUND` (`-300009`).

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

Choose **one** of the two paths from §1.1.

#### A. Simple path (recommended) — Enable API

Link `libmagic_sr_enable.a` + include `mc_enable.h`. No need to compile `mc_enable.c` yourself.

1. Header: `interface/mc_enable.h`  
2. Link the combined library:  
   - Android: `lib/android/libmagic_sr_enable.a`  
   - iOS: `lib/ios/libmagic_sr_enable.a`  
3. On iOS, also link Metal / MetalKit (and related) system frameworks  
4. Call `MC_Enable_SetModelPath` / `MC_Enable_SetModelDir` (recommended) → `MC_Enable` / `MC_Disable`

**Rebuild the combined library locally (optional)**

Packages usually already ship `libmagic_sr_enable.a`. If you changed `interface/mc_enable*.c/m`, or you only have core `libmagic_sr.a` and need to regenerate the Enable combine lib, from the repo root:

```bash
./tools/build_enable_lib.sh          # default: Android + iOS
./tools/build_enable_lib.sh android  # Android only
./tools/build_enable_lib.sh ios      # iOS only
```

The script:

1. Locates an existing core `libmagic_sr.a` (prefers `lib/<platform>/`, then private `build/` outputs when present)  
2. Compiles `mc_enable.c` (plus `mc_enable_metal.m` on iOS)  
3. Merges them into:  
   - `lib/android/libmagic_sr_enable.a`  
   - `lib/ios/libmagic_sr_enable.a`  

Requires the matching toolchain (Android NDK; iOS needs Xcode / iphoneos SDK). This is unrelated to `setup_models.sh` (models only) — `build_enable_lib.sh` only **builds libraries**.

Example (Android NDK / CMake snippet):

```cmake
add_library(my_app SHARED native-lib.cpp)
add_library(magic_sr_enable STATIC IMPORTED)
set_target_properties(magic_sr_enable PROPERTIES
  IMPORTED_LOCATION ${MAGIC_SR_ROOT}/lib/android/libmagic_sr_enable.a)
target_link_libraries(my_app magic_sr_enable ...)
```

Example (iOS Xcode): link `-lmagic_sr_enable` (search path → `lib/ios/`), do **not** add `mc_enable.c` / `mc_enable_metal.m` to Compile Sources.

#### B. Professional / advanced path — session API

Link core-only `libmagic_sr.a` + include `mc_interface.h`. Use the session API without Enable:

1. Header: `interface/mc_interface.h`  
2. Link: `lib/android/libmagic_sr.a` or `lib/ios/libmagic_sr.a`  
3. Call `MC_Init` → `MC_Process` → `MC_Uninit` (see §7)

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

For explicit control of input/output textures, backend, or CPU buffer paths, use the **professional / advanced** path (§1.1 B): link `libmagic_sr.a`, include `mc_interface.h`, and call the session API:

| Platform | API |
|----------|-----|
| Native | `MC_Init` / `MC_Process` / `MC_Uninit` |
| Unity | `MagicSRSession.CreateEx` + `ProcessTexture` |
| Unreal | `CreateSessionEx` + `ProcessUTexture` / `ProcessNativeTexture` |

See `mc_interface.h` for `InputType` / `Backend` values.  
**For typical game / camera integration, prefer the simple Enable path (`libmagic_sr_enable.a` + `mc_enable.h`); the session API is not required.**

More detailed plugin guides (English):

- `plugin/unity/USAGE_GUIDE.md`  
- `plugin/ue/USAGE_GUIDE.md`

---

## 8. FAQ

**Q: Enable returns a null pointer?**  
A: Check the log line `[MagicSR][Enable] err=<code>`. Common causes: missing model (`-300009`), missing Android size hint (`-300006`), invalid size (`-300008`), or `MC_Process` / `MC_Init` failure. Also verify RGBA input and `SetModelDir` / model paths.

**Q: Why can't iOS use R8 anymore?**  
A: Current `MC_Enable` defaults to RGBA input. R8 applies only to older session-based Metal/GLES advanced paths, not to `Enable`.

**Q: Output is corrupted / all black?**  
A: Confirm RGBA8 input, no illegal release of the output, and (on Android GLES/Vulkan) that `SetInputSizeHint` matches the real texture size; check MagicSR error codes in logs.

**Q: How do I integrate camera / URP / HDRP?**  
A: Resolve / blit the frame to an RGBA `RenderTexture` (or equivalent native texture), then call the same `Enable(texture, scale)`. There is no dedicated Camera Enable API.

**Q: What about `EnableNative(scale, texture)` in Unreal?**  
A: Deprecated. It still works but uses the old **scale-first** order. Use `Enable(InputNativeTexture, Scale)` instead.

**Q: Can multiple instances Enable at once?**  
A: The `Enable` path is a process-wide singleton. For concurrent streams, use the session API, or share one `Enable` session serially.

**Q: Which Native library / header should I use?**  
A: Two paths (§1.1): **Simple (recommended)** — `libmagic_sr_enable.a` + `mc_enable.h` → `MC_Enable*` / `MC_Disable`. **Professional / advanced** — `libmagic_sr.a` + `mc_interface.h` → `MC_Init` / `MC_Process` / `MC_Uninit`. Pick one; do not mix.

**Q: Linker error: undefined `MC_Enable` / `MC_Disable`?**  
A: You linked core-only `libmagic_sr.a` (session path). For Enable, switch to `libmagic_sr_enable.a` and include `mc_enable.h` (see §1.1, §5.3 A).

**Q: How is `libmagic_sr_enable.a` built? How is that different from `setup_models.sh`?**  
A: Release packages usually ship the library. To rebuild locally, run `./tools/build_enable_lib.sh` (merges core `libmagic_sr.a` with `mc_enable`; see §5.3 A). `setup_models.sh` only copies model `.bin` files — it does not build libraries.

---

## 9. Version and Deliverables

| Item | Path | Integration path |
|------|------|------------------|
| Native Android / iOS (Enable + core) | `lib/*/libmagic_sr_enable.a` | **Simple** — with `mc_enable.h` → `MC_Enable*` / `MC_Disable` |
| Native Android / iOS (core only) | `lib/*/libmagic_sr.a` | **Professional** — with `mc_interface.h` → `MC_Init` / `MC_Process` / `MC_Uninit` |
| Enable header | `interface/mc_enable.h` | Simple path |
| Session / advanced header | `interface/mc_interface.h` | Professional path |
| Unity plugin | `plugin/unity/` | Plugin wraps Enable (simple path) |
| UE plugin | `plugin/ue/` | Plugin wraps Enable (simple path) |
| License | `doc/MagicCode Super-Resolution Software End User License Agreement (EULA) v1.1.pdf` | — |

---

## 10. Support

For technical issues, contact MagicCode support and include: platform, engine version, graphics backend, model filename, full error logs, and a minimal reproducible project when possible.
