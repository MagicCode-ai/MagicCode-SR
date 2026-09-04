# MagicCode Super-Resolution (MagicSR) User Guide

Version: v2.1.0
Applies to: Native SDK (C API)
Platforms: Android (Vulkan / OpenGLES), iOS / macOS (Metal), Windows (Vulkan / Direct3D 11 / OpenGL 4.3), macOS (Apple Silicon / x86_64)

中文版：[`用户使用说明书.md`](用户使用说明书.md)

---

## 1. Product Overview

MagicSR is the MagicCode super-resolution SDK. It upscales input images or GPU textures and enhances visual detail. Typical use cases include mobile camera preview/processing, game rendering, and UI scaling.

Starting in **v2.1.0**, MagicSR unifies all features into a single library per platform and a streamlined 3-function public C ABI:

```c
int MC_Enable(void **handle, magic_frame_t *frame, input_param_t *param, output_status_params_t *status_info);
int MC_Disable(void *handle);
char *MC_GetVersion(void);
```

### Key Advantages of v2.1.0

- **Single Deliverable per Platform**: Only one static library (`libmagic_sr.a` / `libmagic_sr.lib`) and one header (`interface/mc_interface.h`). No auxiliary enable wrapper libraries or separate headers needed.
- **Unified Public API**: All operations—initialization, per-frame execution, dynamic reconfiguration, and status querying—are performed via `MC_Enable`. Resource cleanup is handled by `MC_Disable`.
- **Spatial & Temporal Support**: Both single-frame spatial super-resolution and multi-frame temporal super-resolution share the same `MC_Enable` function signature.

---

## 2. Deliverables and Requirements

### 2.1 Static Libraries and Headers

| Platform | Static Library | Public Header |
|----------|----------------|---------------|
| Android | `lib/android/libmagic_sr.a` | `interface/mc_interface.h` |
| iOS | `lib/ios/libmagic_sr.a` | `interface/mc_interface.h` |
| macOS (Apple Silicon) | `lib/mac_arm/libmagic_sr.a` | `interface/mc_interface.h` |
| macOS (x86_64) | `lib/mac_x86/libmagic_sr.a` | `interface/mc_interface.h` |
| Windows | `lib/windows/libmagic_sr.lib` | `interface/mc_interface.h` |

### 2.2 System & Hardware Requirements

- **Android**: Android 8.0 (API Level 26) or higher. Supports Vulkan and OpenGLES 3.1+.
- **iOS / iPadOS**: iOS 13.0+ / iPadOS 13.0+ or higher. Supports Apple Metal.
- **macOS**: macOS 12.0+ (Apple Silicon arm64 or Intel x86_64).
- **Windows**: Windows 10 64-bit or higher. Supports Vulkan, Direct3D 11, and OpenGL 4.3.
- **Resolution Range**: Input width and height in `[64, 4032]`.
- **Scaling Factors**:
  - Spatial super-resolution: `[1.0, 8.0]`
  - Temporal super-resolution: `(1.0, 8.0]` (scale factor `1.0` is rejected)

---

## 3. Core API Specification

All functionality is provided through three public functions in `interface/mc_interface.h`:

### 3.1 `MC_Enable`

```c
int MC_Enable(void **handle, magic_frame_t *frame, input_param_t *param, output_status_params_t *status_info);
```

- **`handle` (void \*\*handle, Required)**: Pointer to the caller's algorithm handle variable.
  - **Implicit Initialization**: If `*handle == NULL`, `MC_Enable` treats this as the initial call and creates a new handle using `param` (in this case `param` must not be `NULL`).
  - **Integrity Validation**: If `*handle != NULL`, the handle integrity guard values (`0x11223344`, `0xaabbccdd`) are verified.
- **`frame` (magic_frame_t \*, Optional)**: Input and output resources for the current frame.
  - When `frame != NULL`: Executes super-resolution processing for this frame.
  - When `frame == NULL`: Bypasses frame processing (useful for initialization-only or status-query-only calls).
- **`param` (input_param_t \*, Optional on subsequent calls)**: Configuration parameters.
  - Required on the initial call (`*handle == NULL`).
  - On subsequent calls (`*handle != NULL`), passing non-NULL `param` checks if any mutable parameters (`width`, `height`, `scaler_factor`, `alg_mode`, `log_level`, `spatial_sharpen_level`, `input_type`) have changed. If changed, the handle automatically reinitializes. Pass `NULL` to retain current configuration.
- **`status_info` (output_status_params_t \*, Optional)**: Pointer to receive status and execution statistics. Pass `NULL` if not needed.
- **Return Value**: `0` on success; a negative error code (`MC_ERROR_*`) on failure.

### 3.2 `MC_Disable`

```c
int MC_Disable(void *handle);
```

- Releases all internal resources and memory associated with `handle`.
- Passing `NULL` is safe and performs no operation.
- After calling `MC_Disable`, the handle is invalid and must not be used again.

### 3.3 `MC_GetVersion`

```c
char *MC_GetVersion(void);
```

- Returns the library version string (e.g. `"v2.1.0"`). The returned string is statically allocated; do not attempt to free it.

---

## 4. Quick Start: Spatial Super-Resolution

Spatial super-resolution processes single frames without requiring motion vectors or depth buffers.

### 4.1 Basic Lifecycle Pattern

```c
#include "mc_interface.h"
#include <string.h>

void *g_handle = NULL;

int init_and_process(magic_resource_t in_tex, magic_resource_t out_tex, unsigned int in_w, unsigned int in_h)
{
    input_param_t param;
    memset(&param, 0, sizeof(param));
    param.struct_size = (uint32_t)sizeof(param);
    param.input_type = INPUT_TEXTURE_RGB8Unorm;
    param.width = in_w;
    param.height = in_h;
    param.scaler_factor = 2.0f;
    param.alg_mode = SPATIAL_SPEED_MODE;      /* or SPATIAL_BALANCED_MODE */
    param.backend = MAGIC_BACKEND_METAL;      /* or VULKAN, OPENGLES, D3D11, etc. */
    param.log_level = MAGIC_LOG_ERROR;
    strncpy(param.model_path, "/path/to/model.bin", sizeof(param.model_path) - 1);

    magic_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.image_in = in_tex;
    frame.image_out = out_tex;
    frame.frame = NULL;                       /* NULL for spatial */

    output_status_params_t status;
    int ret = MC_Enable(&g_handle, &frame, &param, &status);
    if (ret != 0) {
        /* Handle error: see status.error_code or return code */
        return ret;
    }

    /* Subsequent frames with same configuration: */
    /* ret = MC_Enable(&g_handle, &frame, NULL, &status); */

    return 0;
}

void shutdown(void)
{
    if (g_handle) {
        MC_Disable(g_handle);
        g_handle = NULL;
    }
}
```

### 4.2 Algorithm Modes

| Mode Value | Enumerator | Characteristics | Target Scenarios |
|------------|------------|-----------------|------------------|
| `0` | `SPATIAL_SPEED_MODE` | High throughput, lowest GPU/CPU latency | Mobile camera preview, battery-saving mode |
| `1` | `SPATIAL_BALANCED_MODE` | Enhanced detail reconstruction | Higher quality spatial upscaling |
| `2` | `TEMPORAL_SPEED_MODE` | High-throughput temporal reconstruction | Low-latency mobile/VR games |
| `3` | `TEMPORAL_BALANCED_MODE` | Canonical FSR-family temporal path | Desktop and high-fidelity gaming |

---

## 5. Temporal Super-Resolution

Temporal super-resolution accumulates historical information across consecutive frames using color, depth buffers, motion vectors, and sub-pixel camera jitter.

### 5.1 Temporal Input Requirements

To execute temporal super-resolution, set `param.alg_mode` to `TEMPORAL_SPEED_MODE` or `TEMPORAL_BALANCED_MODE`, and attach a populated `temporal_frame_t` to `frame.frame`:

| Resource / Field | Description | Requirement |
|------------------|-------------|-------------|
| `magic_frame_t.image_in` | Current frame color buffer | Input resolution, RGB8Unorm / RGBA8 |
| `magic_frame_t.image_out` | Upscaled output color buffer | Scaled output resolution |
| `magic_frame_t.frame` | Pointer to `temporal_frame_t` | **Must not be NULL** |
| `magic_frame_t.command_buffer` | GPU command buffer | Vulkan: recording `VkCommandBuffer`; Metal: optional; others NULL |
| `temporal_frame_t.struct_size` | Structure size for ABI compatibility | `sizeof(temporal_frame_t)` |
| `temporal_frame_t.depth` | Device depth buffer in `[0, 1]` | Input resolution |
| `temporal_frame_t.motion` | Motion vectors | Input resolution, current → previous |
| `temporal_frame_t.jitter_offset_x/y` | Sub-pixel camera jitter in input pixels | Top-left origin, +X right, +Y down |
| `temporal_frame_t.frame_index` | Frame sequence index | Monotonically increasing |
| `temporal_frame_t.reset_history` | Discard temporal history | Non-zero on scene cuts or teleports |
| `temporal_frame_t.camera_near/far/fov_y` | Camera projection parameters | Near/far distances and vertical FOV (radians) |
| `temporal_frame_t.reactive` | Optional reactive mask | Caller-provided or internally derived |
| `temporal_frame_t.transparency` | Optional transparency/composition mask | Caller-provided or internally derived |

### 5.2 Vulkan Temporal Example

```c
#include "mc_interface.h"
#include <string.h>

static void *g_temporal_vk = NULL;

int temporal_vk_init(VkPhysicalDevice phys, VkDevice dev, unsigned int in_w, unsigned int in_h)
{
    input_param_t p;
    memset(&p, 0, sizeof(p));
    p.struct_size = (uint32_t)sizeof(p);
    p.input_type = INPUT_TEXTURE_RGB8Unorm;
    p.width = in_w;
    p.height = in_h;
    p.scaler_factor = 2.0f;              /* Must be in (1.0, 8.0] */
    p.alg_mode = TEMPORAL_SPEED_MODE;    /* or TEMPORAL_BALANCED_MODE */
    p.backend = MAGIC_BACKEND_VULKAN;
    p.log_level = MAGIC_LOG_ERROR;
    p.gpu_context.physical_device = phys;
    p.gpu_context.device = dev;
    /* Optional create-stage: p.depth_reversed / p.depth_infinite / p.hdr_color */

    /* Initial call creates g_temporal_vk handle */
    return MC_Enable(&g_temporal_vk, NULL, &p, NULL);
}

int temporal_vk_process(VkCommandBuffer cmd,
                        uint64_t color_img, uint64_t depth_img, uint64_t motion_img,
                        uint64_t output_img,
                        uint32_t color_fmt, uint32_t depth_fmt, uint32_t mv_fmt, uint32_t out_fmt,
                        uint32_t color_layout, uint32_t depth_layout, uint32_t mv_layout, uint32_t out_layout,
                        unsigned int frame_index, int reset_history)
{
    if (!g_temporal_vk) return -1;

    temporal_frame_t tf;
    memset(&tf, 0, sizeof(tf));
    tf.struct_size = (uint32_t)sizeof(tf);
    tf.depth.handle.vk_image = depth_img;
    tf.depth.format = depth_fmt;
    tf.depth.layout = depth_layout;
    tf.motion.handle.vk_image = motion_img;
    tf.motion.format = mv_fmt;
    tf.motion.layout = mv_layout;
    tf.jitter_offset_x = 0.0f;           /* in input pixels, +X right */
    tf.jitter_offset_y = 0.0f;           /* in input pixels, +Y down */
    tf.motion_vector_scale_x = 0.0f;     /* (0,0) defaults to (in_w, in_h) for UV MVs */
    tf.motion_vector_scale_y = 0.0f;
    tf.frame_index = frame_index;
    tf.reset_history = reset_history;
    tf.camera_near = 0.1f;
    tf.camera_far = 1000.0f;
    tf.camera_fov_y = 1.0f;              /* radians */
    tf.view_to_meters = 1.0f;
    tf.frame_time_delta_ms = 16.67f;

    magic_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.image_in.handle.vk_image = color_img;
    frame.image_in.format = color_fmt;
    frame.image_in.layout = color_layout;
    frame.image_out.handle.vk_image = output_img;
    frame.image_out.format = out_fmt;
    frame.image_out.layout = out_layout;
    frame.frame = &tf;
    frame.command_buffer = cmd;          /* Must already be recording */

    output_status_params_t status;
    return MC_Enable(&g_temporal_vk, &frame, NULL, &status);
}

void temporal_vk_shutdown(void)
{
    if (g_temporal_vk) {
        MC_Disable(g_temporal_vk);
        g_temporal_vk = NULL;
    }
}
```

### 5.3 Backend Resource & Synchronization Contracts

| Backend | Platform | `input_param_t.gpu_context` | `magic_frame_t.command_buffer` | Resource Union Member |
|---------|----------|-----------------------------|--------------------------------|-----------------------|
| Metal | iOS / macOS | `device` (optional; defaults to system default) | Optional `MTLCommandBuffer` | `pointer` (`MTLTexture*`) |
| Vulkan | Android / Windows | `physical_device` and `device` **required** | **Must be a recording `VkCommandBuffer`** | `vk_image` (`VkImage`) |
| Direct3D 11 | Windows | `device` **required** (`device_context` optional) | Unused (`NULL`) | `pointer` (`ID3D11Texture2D*`) |
| OpenGL 4.3 | Windows | Current context; library does not make current | Unused (`NULL`) | `gl_texture` (`GLuint`) |
| OpenGLES | Android | Current context; library does not make current | Unused (`NULL`) | `gl_texture` (`GLuint`) |

**Resource Ownership & Synchronization:**
- **Caller Ownership**: The application owns all input/output textures, depth buffers, motion vectors, and command buffers. MagicSR owns only its internal history, scratch textures, and pipeline state objects.
- **Vulkan Command Buffer**: MagicSR only records compute commands into the supplied `command_buffer`. It never begins, ends, submits, or waits on command buffers.
- **Image Layouts**: In Vulkan, layouts passed in `magic_resource_t.layout` must represent the valid layout at the time of entry (`VK_IMAGE_LAYOUT_UNDEFINED` is rejected).

### 5.4 Motion Vectors, Depth, and Masks

- **Motion Vectors (MV)**: Defined as **current frame → previous frame**. The library never negates motion vectors. Coordinates are +X right, +Y down.
- **MV Scale (`motion_vector_scale_x/y`)**: Stored motion multiplied by this factor yields pixel displacement. `(0,0)` defaults to `(input_width, input_height)` for normalized UV motion vectors. Use `(1,1)` for pixel-space motion vectors.
- **Depth**: Must be GPU device depth in `[0, 1]`. Reversed-Z and infinite far plane configurations are specified at creation stage via `input_param_t.depth_reversed` and `depth_infinite`.
- **Mask Derivation**: If `reactive` or `transparency` masks are omitted, MagicSR derives approximate masks internally. Explicit caller-provided masks always take precedence.

---

## 6. Model Files & Deployment

### 6.1 Setup Script (`tools/setup_models.sh`)

Use the included helper script to copy model binaries from `model/` to your target directory:

```bash
# macOS / Linux
./tools/setup_models.sh demo     # Copies models to Android/iOS demo folders
./tools/setup_models.sh local    # Copies models to ./MagicSRModels/
./tools/setup_models.sh adb      # Pushes models to connected Android device
```

### 6.2 Model Filenames

| Platform | Backend | Speed Model | Balanced Model | Temporal Model |
|----------|---------|-------------|----------------|----------------|
| Android | OpenGLES | `magic_gles_speed_gpu_params.bin` | `magic_gles_balanced_gpu_params.bin` | `magic_temporal_sr_gpu_params.bin` |
| Android | Vulkan | `magic_vulkan_speed_gpu_params.bin` | `magic_vulkan_balanced_gpu_params.bin` | `magic_temporal_sr_gpu_params.bin` |
| iOS / macOS | Metal | `magic_metal_speed_gpu_params.bin` | `magic_metal_balanced_gpu_params.bin` | `magic_temporal_sr_gpu_params.bin` |
| Windows | Vulkan / D3D11 / GL | `magic_gl_speed_gpu_params.bin` | `magic_gl_balanced_gpu_params.bin` | `magic_temporal_sr_gpu_params.bin` |

In production applications, pass the explicit path to the model file or model directory in `input_param_t.model_path`.

---

## 7. Status Query and Error Handling

### 7.1 Reading Execution Status

Pass a pointer to `output_status_params_t` into `MC_Enable` to query execution metrics:

```c
output_status_params_t status;
int ret = MC_Enable(&handle, &frame, NULL, &status);
if (ret == 0) {
    printf("GPU Time: %.2f ms, Error Code: %u\n", status.gpu_time, status.error_code);
    if (status.temporal_status_valid) {
        printf("Last Temporal Frame Index: %u\n", status.temporal_frame_index);
    }
}
```

### 7.2 Common Error Codes

| Error Code | Constant | Meaning / Resolution |
|------------|----------|----------------------|
| `-100001` | `MC_ERROR_INIT_NULL_PARAM` | Initial call (`*handle == NULL`) received `param == NULL`. |
| `-100004` | `MC_ERROR_INIT_SCALER_OUT_OF_RANGE` | Scale factor is invalid (spatial must be `[1.0, 8.0]`, temporal `(1.0, 8.0]`). |
| `-100008` | `MC_ERROR_INIT_BACKEND_UNAVAILABLE` | Requested backend is not compiled into this binary build. |
| `-100025` | `MC_ERROR_INIT_GPU_CREATE_FAILED` | Failed to initialize GPU device (e.g. missing `gpu_context` device pointers). |
| `-100029` | `MC_ERROR_INIT_MODEL_FILE_OPEN_FAILED` | Model binary could not be opened (verify `param.model_path`). |
| `-101001` | `MC_ERROR_PROCESS_NULL_HANDLE` | `MC_Enable` received `handle == NULL` or `*handle == NULL`. |
| `-101002` | `MC_ERROR_PROCESS_HANDLE_CORRUPTED` | Handle memory integrity check failed (corrupted memory). |
| `-105001` | `MC_ERROR_TEMPORAL_NULL_FRAME` | Temporal mode requires `frame->frame != NULL`. |
| `-105003..6` | `MC_ERROR_TEMPORAL_MISSING_*` | Required color, output, depth, or motion resource handle is missing. |
| `-105017` | `MC_ERROR_TEMPORAL_COMMAND_BUFFER` | Vulkan temporal requires a valid recording `VkCommandBuffer`. |

---

## 8. Migration from Previous Versions

If upgrading from v1.x or v2.0:
1. **Single Library**: Remove any references to `libmagic_sr_enable.a` or `libmagic_enable_sr.lib`. Link only `libmagic_sr.a` (or `libmagic_sr.lib`).
2. **Single Header**: Remove `#include "mc_enable.h"`. Include only `mc_interface.h`.
3. **Consolidated API**:
   - Replace separate `MC_Init(...)` and `MC_Process(...)` calls with unified `MC_Enable(&handle, &frame, &param, &status)`.
   - Replace `MC_Uninit(...)` with `MC_Disable(handle)`.
   - Remove usage of removed structures: `control_param_t`, `cmd_params_e`. Mutable parameter updates are handled by passing `param` to `MC_Enable`.
4. **Version Query**: Verify `MC_GetVersion()` returns `"v2.1.0"`.
