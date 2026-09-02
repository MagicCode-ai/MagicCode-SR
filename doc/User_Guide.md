# MagicCode Super-Resolution (MagicSR) User Guide

Version: v2.0.0
Applies to: Native SDK
Platforms: Android (Vulkan / OpenGLES), iOS / macOS (Metal), Windows (Vulkan / Direct3D 11 / OpenGL 4.3)

中文版：[`用户使用说明书.md`](用户使用说明书.md)

---

## 1. Product Overview

MagicSR is the MagicCode super-resolution SDK. It upscales an input GPU texture and enhances detail. Typical uses include game frames, camera preview, and UI textures. v2.0.0 provides spatial and temporal super-resolution: spatial is the simple single-frame path; temporal needs per-frame depth, motion vectors, and jitter through the session API (see §7).

**Recommended spatial integration** (two steps):

1. `Enable`: pass one GPU texture; receive the upscaled output texture
2. `Disable`: release the internal session and output texture when finished

You do not need to create a session, pick a backend, or configure input type manually; `Enable` handles that internally.

The Native simple spatial API is `MC_Enable` / `MC_Disable`; complete temporal integration uses `MC_Init` / `MC_Process` / `MC_Uninit`.

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
| macOS (Apple Silicon) | `lib/mac_arm/libmagic_sr_enable.a` | `lib/mac_arm/libmagic_sr.a` |
| Windows library | `lib/windows/libmagic_enable_sr.lib` | `lib/windows/libmagic_sr.lib` |

Notes:

- Simple path: you do **not** need to add `mc_enable.c` to your app; Enable is already inside `libmagic_sr_enable.a`.
- Core-only `libmagic_sr.a` does **not** export `MC_Enable*` — linking it alone causes undefined-symbol errors for Enable.
- **Native apps:** choose a path above (see §5.1). On Apple, still link Metal / MetalKit system frameworks.
- Release packages usually already include `libmagic_sr_enable.a`. To **rebuild** the combined library from core + Enable sources locally, see `tools/build_enable_lib.sh` in §5.1 A.

---

## 2. Quick Start

**Argument order (all layers):** `(input_texture, scale)` — texture first, scale second.

### 2.1 Native C

Before calling `MC_Enable`, link `libmagic_sr_enable.a` and `#include "mc_enable.h"`. See §1.1 and §5.1.

```c
#include "mc_enable.h"

/* Signature: MC_Enable(input_texture, scale) */
void* out = MC_Enable(input_texture, 2.0f);  // RGBA GPU texture
if (out == NULL) { /* failure — see [MagicSR][Enable] err=<code> in logs */ }

/* Use out (borrowed pointer — do not free / CFRelease / glDelete) */

MC_Disable(NULL);  // or MC_Disable(out); argument may be ignored
```

Optional variants (same argument order): `MC_Enable_3params(input, scale, mode)`, `MC_Enable_4params(input, scale, mode, backend)` for **spatial** mode/backend. Temporal enums may pass the range check, but Enable still has only one input texture and is not a complete temporal entry point (see §7.8).

---

## 3. Enable Behavior

| Item | Description |
|------|-------------|
| Role | Lazy-init session + process one frame + return output GPU texture |
| `scale` | `float`, spatial range `[1.0, 8.0]`; defaults to `2.0` when `<= 0`. Enable cannot supply temporal depth/motion/jitter |
| Session reuse | Same `scale` and input width/height reuse the internal session and output texture |
| Return value | Output texture pointer; failure is `NULL` / `IntPtr.Zero` / `0` |
| Ownership | Library owns the output; release **only** via `Disable` |

### 3.1 Input Texture Format (Important)

`Enable` requires **RGBA-family** GPU textures:

| Platform / Backend | Input format | `Enable` backend |
|--------------------|--------------|------------------|
| iOS / macOS Metal | `RGBA8Unorm` (`MTLTexture*`) | Metal |
| Windows OpenGL (Enable default) | `RGBA8` (`GLuint` as `void*`) | OpenGL |
| Android OpenGLES | `RGBA8` (`GLuint` as `void*`) | OpenGLES |
| Android / Windows Vulkan | `RGB8Unorm` / `RGBA8` | Vulkan |

`MC_Enable_4params` may select `MAGIC_BACKEND_VULKAN` explicitly. It does **not** accept `MAGIC_BACKEND_D3D11` (use the §7 session API for Direct3D 11 temporal).

Do not pass single-channel R8 textures to `Enable`.

### 3.2 Size Hint (Android GLES / Vulkan — Required)

On **Android OpenGLES** and **Android Vulkan**, the SDK cannot reliably query input texture size. You **must** call `SetInputSizeHint` / `MC_Enable_SetInputSizeHint(width, height)` before `Enable`. Without a valid hint (`width`/`height` in `[64, 4032]`), `Enable` fails with `MC_ENABLE_ERROR_SIZE_HINT_REQUIRED` (`-300006`).

Native callers must set a valid size hint before calling `Enable`.

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
Source is always `<repo>/model/`. By default it copies the Enable **GPU** `.bin` set (Metal / OpenGL / GLES / Vulkan × Speed + Balanced — 8 files; missing names are skipped).

Run from the repo root. See `./tools/setup_models.sh --help` for the full list.

#### What each subcommand does

| Command | Copies models to | Typical use | What you still must do |
|---------|------------------|-------------|------------------------|
| `demo` (default if you omit the command) | Android: `demo/android/app/src/main/assets/model/`<br>iOS: `demo/ios/MagicCameraSR/MagicSRModels/` | Stock camera demos only | Build/install the demo as usual; the demo sets its model path |
| `local` | `<repo>/MagicSRModels/` | Native / local debugging | **Must** call `MC_Enable_SetModelDir("<absolute path to that folder>")` (or `SetModelPath` to one `.bin`). Dropping files there does **not** make every app find them automatically |
| `dir --dest <path>` | Your chosen directory | Custom layout | `MC_Enable_SetModelDir("<absolute path>")` |
| `adb` | Device `/storage/emulated/0/Documents/MagicSRModels/` | Quick Android device check | `MC_Enable_SetModelDir("/storage/emulated/0/Documents/MagicSRModels")` |
| `all` | `demo` + `local` | Prepare several Native targets | Still set paths in code per target above |

```bash
# macOS / Linux examples
./tools/setup_models.sh demo
./tools/setup_models.sh local
./tools/setup_models.sh adb
```

```bat
REM Windows
tools\setup_models.bat demo
tools\setup_models.bat local
```

**Common misunderstandings:**

- `local` is **not** a global auto-search. It only creates `<repo>/MagicSRModels/`. The relative fallback `MagicSRModels/<filename>` may hit it **only if** the process cwd is the repo root. Shipped apps almost never have that cwd — **always set `SetModelDir` / `SetModelPath` explicitly**.
- `demo` vs `build_demo.sh`: the latter also copies GLES models into Android assets when building; `setup_models.sh demo` prepares **both** Android and iOS demo folders as a “stage the models first” step.

### 4.1 Set the model from code (recommended API)

`SetModelPath` / `SetModelDir` are **not** mandatory C parameters, but for real integrations treat them as **required in practice**: without them, Enable falls back to §4.4 implicit search, which often fails on mobile/engines (`-300009`).

Call **before** `Enable`:

Native API: `MC_Enable_SetModelPath("/abs/path/model.bin")` or `MC_Enable_SetModelDir("/abs/path/MagicSRModels")`.

`SetModelPath` wins over `SetModelDir`. Pass empty/null to clear. Do **not** use environment variables for model paths.

`SetModelDir` looks up the known default basenames (see §4.3) under that folder **recursively** (up to 4 levels). It does not scan arbitrary `.bin` names.

### 4.2 After the script (Native)

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
MC_Enable_SetModelPath("/absolute/path/to/magic_gles_speed_gpu_params.bin");

/* Or: directory that contains the default filename for your backend */
MC_Enable_SetModelDir("/absolute/path/to/MagicSRModels");
```

| Platform | Practical absolute locations |
|----------|------------------------------|
| Android | App `filesDir` / assets extracted path; or `/storage/emulated/0/Documents/MagicSRModels/` |
| iOS | Bundle resource path, or sandbox `Documents/...` (see `demo/ios`) |
| Windows | e.g. `C:\\YourApp\\MagicSRModels\\magic_gl_speed_gpu_params.bin` |
| macOS | App resource / known install dir absolute path |

Relative search (`./MagicSRModels/`, `./<name>.bin`) only works if the process cwd happens to be that folder — fine for local tools, unreliable for shipped apps.

### 4.3 Platform × Backend × Default Model Name

| Platform | Backend | Speed (`SPATIAL_SPEED_MODE`) | Balanced (`SPATIAL_BALANCED_MODE`) |
|----------|---------|-------------------------------|----------------------|
| iOS / macOS | Metal | `magic_metal_speed_gpu_params.bin` | `magic_metal_balanced_gpu_params.bin` |
| Windows | OpenGL | `magic_gl_speed_gpu_params.bin` | `magic_gl_balanced_gpu_params.bin` |
| Android | OpenGLES | `magic_gles_speed_gpu_params.bin` | `magic_gles_balanced_gpu_params.bin` |
| Android | Vulkan | `magic_vulkan_speed_gpu_params.bin` | `magic_vulkan_balanced_gpu_params.bin` |

Legacy aliases still accepted: `magic_veryfast_gpu_params.bin`, `magic_veryfast_gles_params.bin`, `magic_speed_gpu_params.bin`.

### 4.4 How `Enable` finds the model (reference)

Search order:

1. **Explicit file**: `MC_Enable_SetModelPath` (if set and readable)
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

### 5.1 Native

Choose **one** of the two paths from §1.1.

#### A. Simple path (recommended) — Enable API

Link `libmagic_sr_enable.a` + include `mc_enable.h`. No need to compile `mc_enable.c` yourself.

1. Header: `interface/mc_enable.h`
2. Link the combined library:
   - Android: `lib/android/libmagic_sr_enable.a`
   - iOS: `lib/ios/libmagic_sr_enable.a`
   - Windows: `lib/windows/libmagic_enable_sr.lib` (Vulkan/D3D11/OpenGL temporal SR; link this one archive only — not `magic_sr_win_vk` / `magic_vulkan_temporal` / `magic_vulkan_taau`; do not use the old `libmagic_sr_enable.lib` filename)
3. On iOS, also link Metal / MetalKit (and related) system frameworks. On Windows, also link OS/SDK libs (`d3d11`, `opengl32`, `vulkan-1`, ...)
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
2. Link: `lib/android/libmagic_sr.a`, `lib/ios/libmagic_sr.a`, `lib/mac_arm/libmagic_sr.a`, or `lib/windows/libmagic_sr.lib`
3. Call `MC_Init` → `MC_Process` → `MC_Uninit` (temporal: §7)

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

## 7. Temporal Super-Resolution

Temporal super-resolution accumulates history from neighboring frames using color, depth, and motion vectors. It fits game/engine pipelines that already have a jittered camera and motion vectors. It is **not** an Enable flag. Production temporal integration must use the session API.

| Item | Contract |
|------|----------|
| Header / library | `interface/mc_interface.h` + `libmagic_sr.a` (Windows: `lib/windows/libmagic_sr.lib`) |
| API | `MC_Init` → per-frame `MC_Process` → `MC_Uninit` |
| Spatial scale | `[1.0, 8.0]` (actual backend/model support applies) |
| Temporal scale | `(1.0, 8.0]` (**1.0 itself is rejected**) |
| CPU | **Unavailable** (`MAGIC_BACKEND_X86` / `NEON` return `MC_ERROR_INIT_BACKEND_UNAVAILABLE`) |
| Verified vs API | The API accepts `(1, 8]`; exact 2× has optimized kernels. This document does not claim every scale or device is device-run |

### 7.1 Modes and when to use them

`alg_mode_e` in `mc_interface.h`:

| Value | Enum | Role |
|-------|------|------|
| 0 | `SPATIAL_SPEED_MODE` | Spatial, throughput first |
| 1 | `SPATIAL_BALANCED_MODE` | Spatial, quality/cost trade-off |
| 2 | `TEMPORAL_SPEED_MODE` | Temporal, throughput first |
| 3 | `TEMPORAL_BALANCED_MODE` | Temporal, canonical FSR-family path |

Choosing a temporal mode:

- **TEMPORAL_SPEED_MODE**: latency-sensitive. Apple Metal, Android/Windows Vulkan, and Android OpenGLES currently use a lightweight Convert→Activate→Upscale pipeline. Windows Direct3D 11 and desktop OpenGL 4.3 use the same FSR-family temporal path as Balanced (not a separate customer API).
- **TEMPORAL_BALANCED_MODE**: every GPU temporal backend uses the canonical FSR-family path. Desktop Balanced edge-resolve is supported on Windows Vulkan, Windows Direct3D 11, desktop OpenGL 4.3, and Apple Metal. Android Vulkan and Android OpenGLES do not run edge-resolve. If output sharpening is on (`enable_sharpening`), RCAS runs **after** edge-resolve.
- Temporal modes turn spatial preprocess off. Internal implementation names are not public configuration knobs.

### 7.2 Minimum inputs

`MC_Init` (`input_param_t`) must set at least:

- `struct_size = sizeof(input_param_t)` (`0` also means current sizeof)
- `alg_mode`: `TEMPORAL_SPEED_MODE` or `TEMPORAL_BALANCED_MODE`
- `backend`, `input_type` (GPU texture), `width` / `height` (`[64, 4032]`), `scaler_factor` ∈ `(1, 8]`
- `gpu_context` (see §7.4). Optional create-stage: `depth_reversed`, `depth_infinite`, `hdr_color` (all 0 = conventional-Z, finite far, LDR). Immutable for the handle lifetime; `MC_Control` resize does not reset them.

Each `MC_Process(handle, &frame)` needs:

| Where | Field | Requirement |
|-------|-------|-------------|
| `magic_frame_t` | `image_in` / `image_out` | Color and output GPU resources (caller-owned) |
| `magic_frame_t` | `frame` | Non-NULL `temporal_frame_t*` |
| `magic_frame_t` | `command_buffer` | Vulkan **must** be a recording `VkCommandBuffer`; Metal may be NULL (library submit) or a caller `MTLCommandBuffer`; D3D11 / GL / GLES leave NULL (library submit) |
| `temporal_frame_t` | `struct_size` | `sizeof(temporal_frame_t)` |
| `temporal_frame_t` | `depth` / `motion` | GPU resources at input resolution |
| `temporal_frame_t` | `jitter_offset_x/y` | Input pixels, top-left origin, +X right, +Y down |
| `temporal_frame_t` | `frame_index` / `reset_history` | Consecutive frames should be last+1; non-zero on load/teleport |
| `temporal_frame_t` | `camera_near` / `camera_far` / `camera_fov_y` | View-space distances and vertical FOV in radians |

Optional: `reactive`, `transparency`, `enable_sharpening` + `sharpness`, `exposure_texture` (1×1 R32F), `pre_exposure` (`0` means 1.0), `motion_vector_scale_*`, `mv_jitter`, `view_to_meters`, `frame_time_delta_ms`.

### 7.3 C example (Vulkan)

Field names match `mc_interface.h`. Write exactly one live `magic_data_e` member for the `MC_Init` backend.

Keep the handle across frames. `temporal_vk_process` only records into the caller's already-recording `VkCommandBuffer`. The library does not begin, end, submit, or wait. After the caller submits that buffer and waits for the GPU work, call `temporal_vk_shutdown` (`MC_Uninit`). `layout=0` is UNDEFINED and fails validation, including `image_out.layout`.

```c
#include "mc_interface.h"
#include <string.h>
/* Plus the Vulkan headers that define VkPhysicalDevice / VkDevice / VkCommandBuffer. */

static void *g_temporal_vk;

int temporal_vk_init(VkPhysicalDevice phys, VkDevice dev,
                     unsigned in_w, unsigned in_h)
{
    input_param_t p;

    memset(&p, 0, sizeof(p));
    p.struct_size = (uint32_t)sizeof(p);
    p.input_type = INPUT_TEXTURE_RGB8Unorm;
    p.width = in_w;
    p.height = in_h;
    p.scaler_factor = 2.0f;              /* (1, 8] */
    p.alg_mode = TEMPORAL_SPEED_MODE;    /* or TEMPORAL_BALANCED_MODE */
    p.backend = MAGIC_BACKEND_VULKAN;
    p.log_level = MAGIC_LOG_ERROR;
    p.gpu_context.physical_device = phys;
    p.gpu_context.device = dev;
    /* optional create-stage: p.depth_reversed / p.depth_infinite / p.hdr_color */

    g_temporal_vk = MC_Init(&p);
    return g_temporal_vk ? 0 : -1;
}

int temporal_vk_process(VkCommandBuffer cmd,
                        uint64_t color, uint64_t depth, uint64_t motion,
                        uint64_t output,
                        uint32_t color_fmt, uint32_t depth_fmt,
                        uint32_t mv_fmt, uint32_t out_fmt,
                        uint32_t color_layout, uint32_t depth_layout,
                        uint32_t mv_layout, uint32_t output_layout,
                        unsigned frame_index, int reset)
{
    temporal_frame_t tf;
    magic_frame_t frame;

    if (!g_temporal_vk)
        return -1;

    memset(&tf, 0, sizeof(tf));
    tf.struct_size = (uint32_t)sizeof(tf);
    tf.depth.handle.vk_image = depth;
    tf.depth.format = depth_fmt;
    tf.depth.layout = depth_layout;
    tf.motion.handle.vk_image = motion;
    tf.motion.format = mv_fmt;
    tf.motion.layout = mv_layout;
    tf.jitter_offset_x = 0.0f;           /* input pixels, +X right */
    tf.jitter_offset_y = 0.0f;           /* +Y down */
    tf.motion_vector_scale_x = 0.0f;     /* (0,0) → (in_w, in_h) for UV MVs */
    tf.motion_vector_scale_y = 0.0f;
    tf.frame_index = frame_index;
    tf.reset_history = reset;
    tf.camera_near = 5.0f;
    tf.camera_far = 65536.0f;
    tf.camera_fov_y = 1.0f;              /* radians */
    tf.view_to_meters = 1.0f;
    tf.frame_time_delta_ms = 16.67f;

    memset(&frame, 0, sizeof(frame));
    frame.image_in.handle.vk_image = color;
    frame.image_in.format = color_fmt;
    frame.image_in.layout = color_layout;
    frame.image_out.handle.vk_image = output;
    frame.image_out.format = out_fmt;
    frame.image_out.layout = output_layout;
    frame.frame = &tf;
    frame.command_buffer = cmd;          /* must already be recording */

    /* Records into cmd only. Caller submits and waits; this function does not. */
    return MC_Process(g_temporal_vk, &frame);
}

int temporal_vk_shutdown(void)
{
    int rc;

    /* Caller must have submitted and waited on recorded GPU work first. */
    rc = MC_Uninit(g_temporal_vk);
    g_temporal_vk = NULL;
    return rc;
}
```

Other backends use the matching union member: Metal / D3D11 → `handle.pointer`; GL / GLES → `handle.gl_texture`. D3D11 requires `gpu_context.device` at `MC_Init` (`device_context` NULL = immediate). GL/GLES never make a context current; the first `MC_Process` still needs a current context on the calling thread.

### 7.4 Backend resource contract

| Backend | Platform | `gpu_context` | Per-frame `command_buffer` | Resource union |
|---------|----------|---------------|----------------------------|----------------|
| Metal | iOS / macOS | `device` may be NULL (system default) | Optional | `pointer` = `MTLTexture*` |
| Vulkan | Android / Windows | `physical_device` + `device` **required**; `get_device_proc_addr` may be NULL | **Must be recording** | `vk_image` = `VkImage` |
| Direct3D 11 | Windows | `device` **required**; `device_context` may be NULL | Unused (NULL) | `pointer` = `ID3D11Texture2D*` |
| OpenGL 4.3 | Windows | Current context; library does not make current | Unused (NULL) | `gl_texture` = `GLuint` |
| OpenGLES | Android | Current context; library does not make current | Unused (NULL) | `gl_texture` = `GLuint` |
| x86 / NEON | — | Temporal unavailable | — | — |

Ownership and sync:

- The caller owns all application GPU objects. The library does not retain or free them; it owns only internal history / scratch / pipelines.
- `magic_resource_t` has **no** width/height. Color/depth/MV/masks must match the handle input size; `image_out` must match the output size. That is a caller contract — the library does not query `VkImage` extents.
- `format` is the backend-native enum; `0` means “not provided” and is not guessed on the per-frame path.
- Vulkan: `layout` must be the real layout at `MC_Process` entry (`0` = UNDEFINED, validation fails). The library transitions inputs to `GENERAL` for compute and restores input layouts; output stays `GENERAL`. The library does not begin/end/submit/wait the command buffer. Complete prior recorded GPU work before destroying the MC handle or changing size.
- `physical_device`, `device`, and images must belong to the same `VkDevice`.

### 7.5 Reset, jitter, MV, depth

- **Motion**: current → previous. Current pixel plus the decoded MV lands on the previous frame. The library **never** negates MVs.
- **Coordinates**: jitter and output are top-left origin, +X right, +Y down. Jitter is in **input pixels**.
- **`motion_vector_scale_x/y`**: the only MV numeric conversion. `stored_mv * scale` is input-pixel displacement under the contract above. `(0,0)` defaults to `(input_width, input_height)` (UV storage). Mixed zero is rejected. Pixel MVs use `(1,1)`; negative Y converts +Y-up storage.
- **`mv_jitter`**: `0` / `MC_MV_JITTER_EXCLUDED` means camera jitter lives only in `jitter_offset_*` (recommended); `MC_MV_JITTER_INCLUDED` means stored MVs already include the current-frame jitter.
- **Depth**: always GPU device depth in `[0, 1]`. Linear view-space depth is not accepted; the library never converts it. Reversed-Z / infinite far / HDR are **create-stage** `MC_Init` fields only.
- **`reset_history`**: non-zero discards temporal history (load, teleport, hard camera cut). Do not set it every frame.
- **`frame_index`**: not last+1 logs `MC_WARNING_TEMPORAL_FRAME_INDEX` (history may be stale) but is not a hard failure.

### 7.6 Default mask behavior

- `reactive` and `transparency` are optional. An explicit caller texture **wins that channel**.
- When omitted, the library **derives an approximate mask internally**. That is **not** the same as owning a semantic transparency mask.
- Speed (Metal / Vulkan / GLES lightweight path): may derive reactive and an approximate transparency when missing (from color.a / coverage evidence; automatic transparency stays 0 without alpha).
- FSR-family path (Balanced on every backend; Speed on D3D11 / desktop GL): derives approximate reactive when reactive is missing; missing transparency binds an internal **zero** texture and is **not** generated from color.a.
- Both temporal modes enable depth+MV **history rejection** by default.
- Advanced diagnostics only (not a customer configuration path): `MAGICSR_TAAU_AUTO_MASK=0` and `MAGICSR_TAAU_HIST_REJECT=0` turn those defaults off. Production integrations should choose `alg_mode` and explicit masks instead of environment variables.

### 7.7 Errors and troubleshooting

| Symptom | Typical cause |
|---------|----------------|
| `MC_Init` NULL / `MC_ERROR_INIT_SCALER_OUT_OF_RANGE` | Temporal scale is 1.0 or outside `(1, 8]` |
| `MC_ERROR_INIT_BACKEND_UNAVAILABLE` | CPU backend, or this build has no GPU temporal for that backend |
| `MC_ERROR_INIT_GPU_CREATE_FAILED` | Vulkan missing physical/device; D3D11 missing device |
| `MC_ERROR_TEMPORAL_NULL_FRAME` | `magic_frame_t.frame == NULL` |
| `MC_ERROR_TEMPORAL_MISSING_COLOR/OUTPUT/DEPTH/MOTION` | Corresponding native handle is empty |
| `MC_ERROR_TEMPORAL_COMMAND_BUFFER` | Vulkan without a recording command buffer |
| `MC_ERROR_TEMPORAL_FORMAT` | Vulkan UNDEFINED layout, or format not accepted for that slot |
| `MC_ERROR_TEMPORAL_MV_SCALE` | Mixed-zero or non-finite scale |
| `MC_ERROR_TEMPORAL_CAMERA_NEAR_FAR` | Finite far needs `0 < near < far`; infinite far needs `near > 0` |
| Ghosting / trails | Missing `reset_history` on hard cuts; wrong MV direction or scale; jitter not +Y down |
| Edge flicker | Only approximate derived masks; supply explicit reactive/transparency |
| Enable temporal failure | Enable has no depth/motion/jitter; see §7.8 |

Use `MC_Control(..., QUERY_STATUS, ...)` and `output_status_params_t`. `temporal_status_valid` is non-zero only after the last successful temporal `MC_Process`.

### 7.8 Boundary with Enable

| | Enable (`MC_Enable*`) | Session API (`MC_Init` / `MC_Process`) |
|--|------------------------|----------------------------------------|
| Library | `libmagic_sr_enable.a` / `libmagic_enable_sr.lib` | `libmagic_sr.a` / `libmagic_sr.lib` |
| Per-frame input | **One** color texture | Color + output + depth + motion + jitter + camera |
| Temporal enums | Range check may accept them | The production temporal entry |
| Complete temporal | **No** | **Yes** |

Do not advertise `MC_Enable_3params(..., TEMPORAL_SPEED_MODE)` as complete temporal. integrate full temporal on the Native session API.

---

## 8. Advanced session API (optional)

For explicit spatial I/O textures, backend, or CPU buffers — or any temporal integration — use the **professional / advanced** path (§1.1 B): link `libmagic_sr.a`, include `mc_interface.h`:

Native API: `MC_Init` / `MC_Process` / `MC_Uninit`.

See `mc_interface.h` for `InputType` / `Backend`. Temporal minimum inputs are in §7.
**Typical game / camera spatial integration should keep the simple Enable path (`libmagic_sr_enable.a` + `mc_enable.h`).**


---

## 9. FAQ

**Q: Enable returns a null pointer?**
A: Check the log line `[MagicSR][Enable] err=<code>`. Common causes: missing model (`-300009`), missing Android size hint (`-300006`), invalid size (`-300008`), or `MC_Process` / `MC_Init` failure. Also verify RGBA input and `SetModelDir` / model paths.

**Q: Why can't iOS use R8 anymore?**
A: Current `MC_Enable` defaults to RGBA input. R8 applies only to older session-based Metal/GLES advanced paths, not to `Enable`.

**Q: Output is corrupted / all black?**
A: Confirm RGBA8 input, no illegal release of the output, and (on Android GLES/Vulkan) that `SetInputSizeHint` matches the real texture size; check MagicSR error codes in logs.



**Q: Can multiple instances Enable at once?**
A: The `Enable` path is a process-wide singleton. For concurrent streams, use the session API, or share one `Enable` session serially.

**Q: Which Native library / header should I use?**
A: Two paths (§1.1): **Simple (recommended for spatial)** — `libmagic_sr_enable.a` + `mc_enable.h` → `MC_Enable*` / `MC_Disable`. **Professional / advanced (including complete temporal)** — `libmagic_sr.a` + `mc_interface.h` → `MC_Init` / `MC_Process` / `MC_Uninit`. Pick one; do not mix.

**Q: Linker error: undefined `MC_Enable` / `MC_Disable`?**
A: You linked core-only `libmagic_sr.a` (session path). For Enable, switch to `libmagic_sr_enable.a` and include `mc_enable.h` (see §1.1, §5.1 A).

**Q: How is `libmagic_sr_enable.a` built? How is that different from `setup_models.sh`?**
A: Release packages usually ship the library. To rebuild locally, run `./tools/build_enable_lib.sh` (merges core `libmagic_sr.a` with `mc_enable`; see §5.1 A). `setup_models.sh` only copies model `.bin` files — it does not build libraries.

**Q: Can temporal run through Enable?**
A: Not as a complete entry. Enable has a single color texture and no per-frame depth / motion / jitter. See §7.

**Q: Which backends support temporal?**
A: Android Vulkan / OpenGLES, Apple Metal, Windows Vulkan / Direct3D 11 / OpenGL 4.3. CPU temporal is unavailable. The API accepts scale `(1, 8]`; this document does not claim every scale is device-run.

**Q: If I omit masks, do I have a semantic transparency mask?**
A: No. The library derives an **approximate** mask when inputs are missing; explicit textures win per channel. The FSR-family path does not generate semantic transparency from color.a.

---

## 10. Version and Deliverables

| Item | Path | Integration path |
|------|------|------------------|
| Native Android / iOS (Enable + core) | `lib/*/libmagic_sr_enable.a` | **Simple** — with `mc_enable.h` → `MC_Enable*` / `MC_Disable` |
| Native Android / iOS (core only) | `lib/*/libmagic_sr.a` | **Professional** — with `mc_interface.h` → `MC_Init` / `MC_Process` / `MC_Uninit` |
| Enable header | `interface/mc_enable.h` | Simple path |
| Session / advanced header | `interface/mc_interface.h` | Professional path |
| Native Windows (Enable + core) | `lib/windows/libmagic_enable_sr.lib` | **Simple** |
| Native Windows (core, including temporal) | `lib/windows/libmagic_sr.lib` | **Professional** — temporal session API |
| Native macOS arm (Enable + core) | `lib/mac_arm/libmagic_sr_enable.a` | **Simple** |
| Native macOS arm (core only) | `lib/mac_arm/libmagic_sr.a` | **Professional** |
| License | `doc/版本文档/MagicCode Super-Resolution Software End User License Agreement (EULA).pdf` | — |

---

## 11. Support

For technical issues, contact MagicCode support and include: platform, SDK version, graphics backend, model filename, full error logs, and a minimal reproducible project when possible.
