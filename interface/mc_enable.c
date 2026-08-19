#include "mc_enable.h"

/*
 * Enable helpers compile outside libmagic_sr.a (app / plugin).
 * Same pattern on all backends: create output texture → MC_Process(..., out) → return out.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#endif

/* Max subdirectory depth when resolving names under SetModelDir. */
#define MC_ENABLE_MODEL_DIR_MAX_DEPTH 4

#if defined(SYS_ANDROID) && defined(VULKAN)
#define MC_ENABLE_HAS_VULKAN 1
#include <vulkan/vulkan.h>
#else
#define MC_ENABLE_HAS_VULKAN 0
#endif

/* Desktop Windows OpenGL + Android OpenGLES (including dual-backend Android). */
#if defined(SYS_ANDROID)
#include <GLES3/gl3.h>
#define MC_ENABLE_GL_TEXTURE 1
#elif defined(_WIN32)
#include <glad/glad.h>
#define MC_ENABLE_GL_TEXTURE 1
#else
#define MC_ENABLE_GL_TEXTURE 0
#endif

#if defined(__APPLE__)
int mc_enable_query_texture_size(void* texture, unsigned int* width, unsigned int* height);
int mc_enable_resolve_bundle_model(char* out_path, size_t out_size);
void* mc_enable_metal_acquire_output(void* input_texture, unsigned int out_w, unsigned int out_h, int prefer_r8);
void mc_enable_metal_release_output(void* output);
#endif

#if MC_ENABLE_HAS_VULKAN
/* Layout matches core lib vulkan_device_t prefix (linked from libmagic_sr.a). */
typedef struct {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue computeQueue;
    uint32_t queueFamilyIndex;
} mc_enable_vk_device_t;

typedef struct {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
} mc_enable_vk_texture_t;

extern mc_enable_vk_device_t* get_shared_vulkan_device(void);

static mc_enable_vk_texture_t g_vk_out;
static int g_vk_out_valid = 0;
#endif

#if defined(SYS_ANDROID)
#include <android/log.h>
#define MC_ENABLE_LOGE(...) \
    __android_log_print(ANDROID_LOG_ERROR, "MagicSR", "[Enable] " __VA_ARGS__)
#define MC_ENABLE_LOGI(...) \
    __android_log_print(ANDROID_LOG_INFO, "MagicSR", "[Enable] " __VA_ARGS__)
#else
#define MC_ENABLE_LOGE(...) \
    do { fprintf(stderr, "[MagicSR][Enable] " __VA_ARGS__); fputc('\n', stderr); } while (0)
#define MC_ENABLE_LOGI(...) \
    do { fprintf(stderr, "[MagicSR][Enable] " __VA_ARGS__); fputc('\n', stderr); } while (0)
#endif

static const float MC_ENABLE_DEFAULT_SCALE = 2.0f;
static const float MC_ENABLE_SCALE_EPS = 1.0e-3f;
static const int MC_ENABLE_PROCESS_AVG_WINDOW = 30;

enum {
    MC_ENABLE_STAGE_RESOLVE_MODEL = 0,
    MC_ENABLE_STAGE_QUERY_SIZE,
    MC_ENABLE_STAGE_ENSURE_SESSION,
    MC_ENABLE_STAGE_ACQUIRE_OUTPUT,
    MC_ENABLE_STAGE_MC_PROCESS,
    MC_ENABLE_STAGE_TOTAL,
    MC_ENABLE_STAGE_COUNT
};

static int64_t g_last_mc_process_us = 0;
static double g_last_mc_process_avg30_ms = 0.0;
static int64_t g_last_mc_enable_us = 0;
static double g_last_mc_enable_avg30_ms = 0.0;
static double g_last_fused_cb_ms = 0.0;
static int64_t g_stage_last_us[MC_ENABLE_STAGE_COUNT];
static int64_t g_stage_sum_us[MC_ENABLE_STAGE_COUNT];
static int g_stage_count = 0;
static int64_t g_pending_resolve_model_us = 0;

static int64_t mc_enable_now_us(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER counter;
    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (int64_t)((counter.QuadPart * 1000000LL) / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + (int64_t)ts.tv_nsec / 1000LL;
#endif
}

static void mc_enable_note_stage(int stage, int64_t elapsed_us)
{
    if (stage < 0 || stage >= MC_ENABLE_STAGE_COUNT)
        return;
    if (elapsed_us < 0)
        elapsed_us = 0;
    g_stage_last_us[stage] = elapsed_us;
    g_stage_sum_us[stage] += elapsed_us;
    if (stage == MC_ENABLE_STAGE_MC_PROCESS)
        g_last_mc_process_us = elapsed_us;
    if (stage == MC_ENABLE_STAGE_TOTAL)
        g_last_mc_enable_us = elapsed_us;
}

static void mc_enable_finish_frame_timing(void)
{
    g_stage_count++;
    if (g_stage_count < MC_ENABLE_PROCESS_AVG_WINDOW)
        return;

    {
        double avg[MC_ENABLE_STAGE_COUNT];
        int i;
        for (i = 0; i < MC_ENABLE_STAGE_COUNT; ++i)
            avg[i] = ((double)g_stage_sum_us[i] / (double)MC_ENABLE_PROCESS_AVG_WINDOW) / 1000.0;
        g_last_mc_process_avg30_ms = avg[MC_ENABLE_STAGE_MC_PROCESS];
        g_last_mc_enable_avg30_ms = avg[MC_ENABLE_STAGE_TOTAL];
        MC_ENABLE_LOGI(
            "Enable avg30(ms): total=%.2f resolve=%.2f query=%.2f session=%.2f acquire=%.2f process=%.2f | last process=%.2f enable=%.2f",
            avg[MC_ENABLE_STAGE_TOTAL],
            avg[MC_ENABLE_STAGE_RESOLVE_MODEL],
            avg[MC_ENABLE_STAGE_QUERY_SIZE],
            avg[MC_ENABLE_STAGE_ENSURE_SESSION],
            avg[MC_ENABLE_STAGE_ACQUIRE_OUTPUT],
            avg[MC_ENABLE_STAGE_MC_PROCESS],
            (double)g_last_mc_process_us / 1000.0,
            (double)g_last_mc_enable_us / 1000.0);
        for (i = 0; i < MC_ENABLE_STAGE_COUNT; ++i)
            g_stage_sum_us[i] = 0;
        g_stage_count = 0;
    }
}

static void enable_fail(int code, const char* detail)
{
    if (detail != NULL && detail[0] != '\0')
    {
        MC_ENABLE_LOGE("failed err=%d (%s)", code, detail);
    }
    else
    {
        MC_ENABLE_LOGE("failed err=%d", code);
    }
}

static void* g_session = NULL;
static void* g_output = NULL;
static float g_scale = 0.0f;
static unsigned int g_width = 0;
static unsigned int g_height = 0;
static alg_mode_e g_alg_mode = SPEED_MODE;
static magic_backend_e g_backend = MAGIC_BACKEND_DEFAULT;
static int g_output_owned = 0; /* 1 = we created it (Metal/GL); 0 = borrowed (Vulkan) */
static unsigned int g_size_hint_w = 0;
static unsigned int g_size_hint_h = 0;
static unsigned int g_sharpen_level = 0;
static unsigned int g_session_sharpen_level = 0;
static char g_model_path[256];
static int g_has_model_path = 0;
static char g_model_dir[512];
static int g_has_model_dir = 0;
/* Once resolve_default_model succeeds, reuse the path until mode/backend/path config changes. */
static char g_cached_resolved_path[256];
static int g_has_cached_resolved = 0;
static alg_mode_e g_cached_resolved_mode = SPEED_MODE;
static magic_backend_e g_cached_resolved_backend = MAGIC_BACKEND_DEFAULT;

static magic_backend_e platform_default_backend(void);
static magic_backend_e resolve_backend(magic_backend_e backend);
static void reset_session(void);
static void invalidate_resolved_model_cache(void);

/* Called from Metal dealloc guard before abort() if caller released our texture. */
void mc_enable_note_output_stolen(void* output)
{
    if (output != NULL && output == g_output)
    {
        g_output = NULL;
        g_output_owned = 0;
    }
}

void MC_Enable_SetInputSizeHint(unsigned int width, unsigned int height)
{
    g_size_hint_w = width;
    g_size_hint_h = height;
}

void MC_Enable_SetSharpenLevel(unsigned int level)
{
    if (level > 5u)
        level = 5u;
    if (g_sharpen_level == level)
        return;
    g_sharpen_level = level;
    reset_session();
}

static void invalidate_resolved_model_cache(void)
{
    g_has_cached_resolved = 0;
    g_cached_resolved_path[0] = '\0';
}

static int cache_resolved_model(const char* path,
                                alg_mode_e mode,
                                magic_backend_e resolved_backend)
{
    size_t len;
    if (path == NULL || path[0] == '\0')
        return -1;
    len = strlen(path);
    if (len + 1 > sizeof(g_cached_resolved_path))
        return -1;
    if (path != g_cached_resolved_path)
        memcpy(g_cached_resolved_path, path, len + 1);
    g_has_cached_resolved = 1;
    g_cached_resolved_mode = mode;
    g_cached_resolved_backend = resolved_backend;
    return 0;
}

static int copy_and_cache_resolved_model(const char* path,
                                         alg_mode_e mode,
                                         magic_backend_e resolved_backend,
                                         char* out,
                                         size_t out_size)
{
    size_t len;
    if (path == NULL || path[0] == '\0' || out == NULL || out_size == 0)
        return -1;
    len = strlen(path);
    if (len + 1 > out_size)
        return -1;
    if (path != out)
        memcpy(out, path, len + 1);
    return cache_resolved_model(out, mode, resolved_backend);
}

void MC_Enable_SetModelPath(const char* model_path)
{
    if (model_path == NULL || model_path[0] == '\0')
    {
        g_model_path[0] = '\0';
        g_has_model_path = 0;
        invalidate_resolved_model_cache();
        reset_session();
        return;
    }
    if (strlen(model_path) >= sizeof(g_model_path))
    {
        MC_ENABLE_LOGE("SetModelPath failed: path longer than %zu", sizeof(g_model_path) - 1u);
        return;
    }
    if (g_has_model_path && strcmp(g_model_path, model_path) == 0)
    {
        return;
    }
    memcpy(g_model_path, model_path, strlen(model_path) + 1);
    g_has_model_path = 1;
    invalidate_resolved_model_cache();
    reset_session();
}

void MC_Enable_SetModelDir(const char* model_dir)
{
    if (model_dir == NULL || model_dir[0] == '\0')
    {
        g_model_dir[0] = '\0';
        g_has_model_dir = 0;
        invalidate_resolved_model_cache();
        reset_session();
        return;
    }
    if (strlen(model_dir) >= sizeof(g_model_dir))
    {
        MC_ENABLE_LOGE("SetModelDir failed: path longer than %zu", sizeof(g_model_dir) - 1u);
        return;
    }
    if (g_has_model_dir && strcmp(g_model_dir, model_dir) == 0)
    {
        return;
    }
    memcpy(g_model_dir, model_dir, strlen(model_dir) + 1);
    g_has_model_dir = 1;
    invalidate_resolved_model_cache();
    reset_session();
}

#if MC_ENABLE_GL_TEXTURE
static GLuint g_gl_out_tex = 0;
#endif

static int path_is_readable(const char* path)
{
    FILE* fp;
    if (path == NULL || path[0] == '\0')
    {
        return 0;
    }
    fp = fopen(path, "rb");
    if (fp == NULL)
    {
        return 0;
    }
    fclose(fp);
    return 1;
}

static int try_copy_path(char* out, size_t out_size, const char* path)
{
    size_t len;
    if (!path_is_readable(path))
    {
        return 0;
    }
    len = strlen(path);
    if (len + 1 > out_size)
    {
        return 0;
    }
    memcpy(out, path, len + 1);
    return 1;
}

static int join_dir_file(char* out, size_t out_size, const char* dir, const char* file)
{
    int n;
    if (dir == NULL || file == NULL || out == NULL || out_size == 0)
    {
        return 0;
    }
#if defined(_WIN32)
    n = snprintf(out, out_size, "%s\\%s", dir, file);
#else
    n = snprintf(out, out_size, "%s/%s", dir, file);
#endif
    return n > 0 && (size_t)n < out_size;
}

/*
 * Look for exact file_name under dir (this level first, then subdirs).
 * Not a wildcard scan — only the given basename. Depth capped.
 */
static int try_find_file_under_dir(char* out,
                                  size_t out_size,
                                  const char* dir,
                                  const char* file_name,
                                  int depth_left)
{
    char candidate[512];
#if defined(_WIN32)
    char pattern[512];
    WIN32_FIND_DATAA fd;
    HANDLE h;
#else
    DIR* dp;
    struct dirent* ent;
#endif

    if (out == NULL || out_size == 0 || dir == NULL || dir[0] == '\0' ||
        file_name == NULL || file_name[0] == '\0' || depth_left < 0)
    {
        return 0;
    }

    if (join_dir_file(candidate, sizeof(candidate), dir, file_name) &&
        try_copy_path(out, out_size, candidate))
    {
        return 1;
    }

    if (depth_left == 0)
    {
        return 0;
    }

#if defined(_WIN32)
    if (!join_dir_file(pattern, sizeof(pattern), dir, "*"))
    {
        return 0;
    }
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    do
    {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            continue;
        }
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
        {
            continue;
        }
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        {
            continue; /* skip junctions/symlinks */
        }
        if (!join_dir_file(candidate, sizeof(candidate), dir, fd.cFileName))
        {
            continue;
        }
        if (try_find_file_under_dir(out, out_size, candidate, file_name, depth_left - 1))
        {
            FindClose(h);
            return 1;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    dp = opendir(dir);
    if (dp == NULL)
    {
        return 0;
    }
    while ((ent = readdir(dp)) != NULL)
    {
        struct stat st;
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
        {
            continue;
        }
        if (!join_dir_file(candidate, sizeof(candidate), dir, ent->d_name))
        {
            continue;
        }
        if (lstat(candidate, &st) != 0 || !S_ISDIR(st.st_mode))
        {
            continue; /* only real directories; skip files and symlinks */
        }
        if (try_find_file_under_dir(out, out_size, candidate, file_name, depth_left - 1))
        {
            closedir(dp);
            return 1;
        }
    }
    closedir(dp);
#endif
    return 0;
}

/*
 * Default model matrix (platform × backend × alg_mode):
 *
 * | Platform       | Backend   | SPEED_MODE                         | BALANCED_MODE                              |
 * |----------------|-----------|-----------------------------------------|-----------------------------------------|
 * | iOS / macOS    | Metal     | magic_metal_speed_gpu_params.bin    | magic_metal_balanced_gpu_params.bin        |
 * | Windows        | OpenGL    | magic_gl_speed_gpu_params.bin       | magic_gl_balanced_gpu_params.bin           |
 * | Android        | OpenGLES  | magic_gles_speed_gpu_params.bin     | magic_gles_balanced_gpu_params.bin         |
 * | Android        | Vulkan    | magic_vulkan_speed_gpu_params.bin   | magic_vulkan_balanced_gpu_params.bin       |
 *
 * Search order:
 *   1) MC_Enable_SetModelPath() override
 *   2) For each candidate name below:
 *        MC_Enable_SetModelDir() tree (exact basename, recursive, depth<=4)
 *        MagicSRModels/<name>
 *        ./<name>
 *        Android: Documents/MagicSRModels and related paths
 *   3) iOS/macOS: App Bundle lookup
 *
 * Candidate names: primary (mode+backend), alternate mode same backend, then legacy aliases.
 */
static void fill_default_model_names(magic_backend_e backend,
                                     alg_mode_e mode,
                                     const char** names,
                                     int* name_count)
{
    const char* primary = NULL;
    const char* alternate = NULL;
    int n = 0;

    if (names == NULL || name_count == NULL)
    {
        return;
    }

    switch (backend)
    {
    case MAGIC_BACKEND_METAL:
        /* BALANCED: 4-dir A/B; SPEED: 2-dir A/B (same LUT layout, different ensemble). */
        primary = "magic_sr_gpu_params.bin";
        alternate = (mode == BALANCED_MODE) ? "magic_srlut_gpu_params.bin"
                                         : "magic_srlut_2dir_gpu_params.bin";
        break;
    case MAGIC_BACKEND_OPENGL:
    case MAGIC_BACKEND_OPENGLES:
    case MAGIC_BACKEND_VULKAN:
    case MAGIC_BACKEND_D3D11:
        /* Combined 7-seg bin: BALANCED=seg0, SPEED=seg(1+sharpen_level). */
        primary = "magic_sr_gpu_params.bin";
        alternate = (mode == BALANCED_MODE) ? "magic_srlut_gpu_params.bin"
                                         : "magic_srlut_2dir_gpu_params.bin";
        break;
    case MAGIC_BACKEND_X86:
    case MAGIC_BACKEND_NEON:
        primary = "magic_sr_cpu_params.bin";
        alternate = "magic_speed_cpu_params.bin";
        break;
    default:
        primary = "magic_sr_gpu_params.bin";
        alternate = "magic_srlut_gpu_params.bin";
        break;
    }

    names[n++] = primary;
    if (alternate != NULL && strcmp(alternate, primary) != 0)
    {
        names[n++] = alternate;
    }
    /* Legacy aliases kept for older packages. */
    names[n++] = "magic_veryfast_gpu_params.bin";
    names[n++] = "magic_veryfast_gles_params.bin";
    names[n++] = "magic_speed_gpu_params.bin";
    *name_count = n;
}

static int resolve_default_model(char* out,
                                 size_t out_size,
                                 alg_mode_e mode,
                                 magic_backend_e resolved_backend)
{
    const char* names[8];
    int name_count = 0;
    int i;

    if (out == NULL || out_size == 0)
    {
        return -1;
    }
    out[0] = '\0';

    /* Hit cache: no fopen / directory walk on subsequent frames. */
    if (g_has_cached_resolved &&
        g_cached_resolved_mode == mode &&
        g_cached_resolved_backend == resolved_backend &&
        g_cached_resolved_path[0] != '\0')
    {
        if (!g_has_model_path || strcmp(g_cached_resolved_path, g_model_path) == 0)
        {
            const size_t len = strlen(g_cached_resolved_path);
            if (len + 1 <= out_size)
            {
                memcpy(out, g_cached_resolved_path, len + 1);
                return 0;
            }
        }
    }

    /* Explicit SetModelPath: trust the configured path (no per-frame readability check). */
    if (g_has_model_path)
    {
        return copy_and_cache_resolved_model(g_model_path, mode, resolved_backend, out, out_size);
    }

    fill_default_model_names(resolved_backend, mode, names, &name_count);

    for (i = 0; i < name_count; ++i)
    {
        char candidate[512];
        if (g_has_model_dir &&
            try_find_file_under_dir(out,
                                   out_size,
                                   g_model_dir,
                                   names[i],
                                   MC_ENABLE_MODEL_DIR_MAX_DEPTH))
        {
            return cache_resolved_model(out, mode, resolved_backend);
        }
        if (join_dir_file(candidate, sizeof(candidate), "MagicSRModels", names[i]) &&
            try_copy_path(out, out_size, candidate))
        {
            return cache_resolved_model(out, mode, resolved_backend);
        }
        if (try_copy_path(out, out_size, names[i]))
        {
            return cache_resolved_model(out, mode, resolved_backend);
        }
#if defined(SYS_ANDROID)
        if (join_dir_file(candidate, sizeof(candidate), "/sdcard/Documents/MagicSRModels", names[i]) &&
            try_copy_path(out, out_size, candidate))
        {
            return cache_resolved_model(out, mode, resolved_backend);
        }
        if (join_dir_file(candidate, sizeof(candidate), "/storage/emulated/0/Documents/MagicSRModels", names[i]) &&
            try_copy_path(out, out_size, candidate))
        {
            return cache_resolved_model(out, mode, resolved_backend);
        }
        if (join_dir_file(candidate, sizeof(candidate), "/storage/emulated/0/Documents", names[i]) &&
            try_copy_path(out, out_size, candidate))
        {
            return cache_resolved_model(out, mode, resolved_backend);
        }
#endif
    }

#if defined(__APPLE__)
    if (mc_enable_resolve_bundle_model(out, out_size) == 0 && path_is_readable(out))
    {
        return cache_resolved_model(out, mode, resolved_backend);
    }
#endif

    return -1;
}

static int backend_requires_size_hint(magic_backend_e resolved_backend)
{
#if defined(SYS_ANDROID)
    return resolved_backend == MAGIC_BACKEND_OPENGLES ||
           resolved_backend == MAGIC_BACKEND_VULKAN;
#else
    (void)resolved_backend;
    return 0;
#endif
}

static int query_texture_size(void* input_texture,
                              unsigned int* width,
                              unsigned int* height,
                              magic_backend_e backend)
{
    magic_backend_e resolved;

    if (input_texture == NULL || width == NULL || height == NULL)
    {
        return -1;
    }

    if (g_size_hint_w >= 64 && g_size_hint_h >= 64)
    {
        *width = g_size_hint_w;
        *height = g_size_hint_h;
        return 0;
    }

    resolved = resolve_backend(backend);

#if defined(__APPLE__)
    (void)resolved;
    /* iOS + macOS Metal MTLTexture* */
    return mc_enable_query_texture_size(input_texture, width, height);
#elif MC_ENABLE_GL_TEXTURE
    if (resolved == MAGIC_BACKEND_OPENGLES || resolved == MAGIC_BACKEND_OPENGL
#if !MC_ENABLE_HAS_VULKAN
        || resolved == MAGIC_BACKEND_DEFAULT
#endif
        )
    {
#if defined(SYS_ANDROID)
        /* GLES often cannot query texture size portably; require MC_Enable_SetInputSizeHint. */
        (void)input_texture;
        return -1;
#else
        GLuint tex = (GLuint)(uintptr_t)input_texture;
        GLint w = 0;
        GLint h = 0;
        if (tex == 0)
        {
            return -1;
        }
        glBindTexture(GL_TEXTURE_2D, tex);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
        if (w <= 0 || h <= 0)
        {
            return -1;
        }
        *width = (unsigned int)w;
        *height = (unsigned int)h;
        return 0;
#endif
    }
#if MC_ENABLE_HAS_VULKAN
    if (resolved == MAGIC_BACKEND_VULKAN || resolved == MAGIC_BACKEND_DEFAULT)
    {
        /* Android Vulkan cannot query VkImage size here; require SetInputSizeHint. */
        (void)input_texture;
        return -1;
    }
#endif
    return -1;
#elif MC_ENABLE_HAS_VULKAN
    /* Vulkan-only Android build: size hint is mandatory. */
    (void)input_texture;
    (void)resolved;
    (void)width;
    (void)height;
    return -1;
#else
    (void)input_texture;
    (void)resolved;
    (void)width;
    (void)height;
    return -1;
#endif
}

#if MC_ENABLE_HAS_VULKAN
static uint32_t mc_enable_vk_find_memory_type(VkPhysicalDevice phys,
                                             uint32_t type_bits,
                                             VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    uint32_t i;
    vkGetPhysicalDeviceMemoryProperties(phys, &mem_props);
    for (i = 0; i < mem_props.memoryTypeCount; ++i)
    {
        if ((type_bits & (1u << i)) != 0 &&
            (mem_props.memoryTypes[i].propertyFlags & props) == props)
        {
            return i;
        }
    }
    return 0;
}

static void mc_enable_vk_destroy_output(void)
{
    mc_enable_vk_device_t* dev;
    if (!g_vk_out_valid)
    {
        return;
    }
    dev = get_shared_vulkan_device();
    if (dev != NULL && dev->device != VK_NULL_HANDLE)
    {
        if (g_vk_out.view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(dev->device, g_vk_out.view, NULL);
        }
        if (g_vk_out.image != VK_NULL_HANDLE)
        {
            vkDestroyImage(dev->device, g_vk_out.image, NULL);
        }
        if (g_vk_out.memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(dev->device, g_vk_out.memory, NULL);
        }
    }
    memset(&g_vk_out, 0, sizeof(g_vk_out));
    g_vk_out_valid = 0;
}

static int mc_enable_vk_create_output(unsigned int out_w, unsigned int out_h)
{
    mc_enable_vk_device_t* dev;
    VkImageCreateInfo info;
    VkMemoryRequirements req;
    VkMemoryAllocateInfo alloc;
    VkImageViewCreateInfo view_info;
    VkResult res;

    mc_enable_vk_destroy_output();
    dev = get_shared_vulkan_device();
    if (dev == NULL || dev->device == VK_NULL_HANDLE || out_w == 0 || out_h == 0)
    {
        return -1;
    }

    memset(&info, 0, sizeof(info));
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent.width = out_w;
    info.extent.height = out_h;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    res = vkCreateImage(dev->device, &info, NULL, &g_vk_out.image);
    if (res != VK_SUCCESS)
    {
        return -1;
    }

    vkGetImageMemoryRequirements(dev->device, g_vk_out.image, &req);
    memset(&alloc, 0, sizeof(alloc));
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = mc_enable_vk_find_memory_type(
        dev->physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    res = vkAllocateMemory(dev->device, &alloc, NULL, &g_vk_out.memory);
    if (res != VK_SUCCESS)
    {
        vkDestroyImage(dev->device, g_vk_out.image, NULL);
        g_vk_out.image = VK_NULL_HANDLE;
        return -1;
    }
    vkBindImageMemory(dev->device, g_vk_out.image, g_vk_out.memory, 0);

    memset(&view_info, 0, sizeof(view_info));
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = g_vk_out.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;
    res = vkCreateImageView(dev->device, &view_info, NULL, &g_vk_out.view);
    if (res != VK_SUCCESS)
    {
        vkFreeMemory(dev->device, g_vk_out.memory, NULL);
        vkDestroyImage(dev->device, g_vk_out.image, NULL);
        memset(&g_vk_out, 0, sizeof(g_vk_out));
        return -1;
    }

    g_vk_out_valid = 1;
    return 0;
}
#endif

static void release_owned_output(void)
{
    if (g_output == NULL)
    {
        g_output_owned = 0;
#if MC_ENABLE_HAS_VULKAN
        mc_enable_vk_destroy_output();
#endif
        return;
    }

#if defined(__APPLE__)
    if (g_output_owned)
    {
        mc_enable_metal_release_output(g_output);
    }
#elif MC_ENABLE_HAS_VULKAN
    if (g_output_owned)
    {
        mc_enable_vk_destroy_output();
    }
#elif MC_ENABLE_GL_TEXTURE
    if (g_output_owned && g_gl_out_tex != 0)
    {
        glDeleteTextures(1, &g_gl_out_tex);
        g_gl_out_tex = 0;
    }
#endif
    g_output = NULL;
    g_output_owned = 0;
}

static void reset_session(void)
{
    release_owned_output();
    if (g_session != NULL)
    {
        MC_Uninit(g_session);
        g_session = NULL;
    }
    g_scale = 0.0f;
    g_width = 0;
    g_height = 0;
    g_alg_mode = SPEED_MODE;
    g_backend = MAGIC_BACKEND_DEFAULT;
}

static magic_backend_e platform_default_backend(void)
{
#if defined(__APPLE__)
    return MAGIC_BACKEND_METAL;
#elif defined(_WIN32)
    return MAGIC_BACKEND_OPENGL;
#elif defined(SYS_ANDROID) && defined(OpenGLES) && !defined(VULKAN)
    return MAGIC_BACKEND_OPENGLES;
#elif defined(SYS_ANDROID)
    return MAGIC_BACKEND_VULKAN;
#else
    return MAGIC_BACKEND_DEFAULT;
#endif
}

static magic_backend_e resolve_backend(magic_backend_e backend)
{
    if (backend == MAGIC_BACKEND_DEFAULT)
    {
        return platform_default_backend();
    }
    return backend;
}

static void* acquire_output(void* session, void* input_texture)
{
    output_status_params_t status;
    unsigned int out_w;
    unsigned int out_h;

    if (session == NULL || input_texture == NULL)
    {
        return NULL;
    }

    memset(&status, 0, sizeof(status));
    if (MC_Control(session, QUERY_STATUS, NULL, &status) != 0)
    {
        return NULL;
    }

    out_w = status.output_width;
    out_h = status.output_height;
    if (out_w == 0 || out_h == 0)
    {
        return NULL;
    }

    if (g_output != NULL && g_output_owned &&
        g_width == status.width && g_height == status.height && g_scale > 0.0f)
    {
        return g_output;
    }

#if defined(__APPLE__)
    release_owned_output();
    g_output = mc_enable_metal_acquire_output(
        input_texture,
        out_w,
        out_h,
        status.input_type == INPUT_TEXTURE_R8Unorm);
    g_output_owned = (g_output != NULL) ? 1 : 0;
    return g_output;
#else
#if MC_ENABLE_HAS_VULKAN
    if (status.backend == MAGIC_BACKEND_VULKAN)
    {
        release_owned_output();
        if (mc_enable_vk_create_output(out_w, out_h) != 0)
        {
            return NULL;
        }
        /* Pass VulkanTexture* so *(VkImage*)out works (image is first field). */
        g_output = &g_vk_out;
        g_output_owned = 1;
        return g_output;
    }
#endif
#if MC_ENABLE_GL_TEXTURE
    if (status.backend == MAGIC_BACKEND_OPENGLES || status.backend == MAGIC_BACKEND_OPENGL
#if !MC_ENABLE_HAS_VULKAN
        || status.backend == MAGIC_BACKEND_DEFAULT
#endif
        )
    {
        GLenum format = (status.input_type == INPUT_TEXTURE_R8Unorm) ? GL_R8 : GL_RGBA8;
        release_owned_output();
        glGenTextures(1, &g_gl_out_tex);
        glBindTexture(GL_TEXTURE_2D, g_gl_out_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if defined(_WIN32)
        if (format == GL_R8)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, (GLsizei)out_w, (GLsizei)out_h, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)out_w, (GLsizei)out_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        }
#else
        glTexStorage2D(GL_TEXTURE_2D, 1, format, (GLsizei)out_w, (GLsizei)out_h);
#endif
        g_output = (void*)(uintptr_t)g_gl_out_tex;
        g_output_owned = 1;
        return g_output;
    }
#endif
    (void)out_w;
    (void)out_h;
    return NULL;
#endif
}

/* Returns 0 on success. On failure sets *out_err and returns -1. */
static int ensure_session(float scale,
                          unsigned int width,
                          unsigned int height,
                          alg_mode_e mode,
                          magic_backend_e backend,
                          const char* model_path,
                          int* out_err)
{
    input_param_t params;
    char resolved_model[256];
    magic_backend_e resolved_backend = resolve_backend(backend);

    if (out_err != NULL)
    {
        *out_err = 0;
    }

    if (g_session != NULL &&
        fabsf(g_scale - scale) <= MC_ENABLE_SCALE_EPS &&
        g_width == width &&
        g_height == height &&
        g_alg_mode == mode &&
        g_backend == resolved_backend &&
        g_session_sharpen_level == g_sharpen_level)
    {
        return 0;
    }

    /* model_path must already be selected by MC_Enable / _3params / _4params. */
    if (model_path == NULL || model_path[0] == '\0')
    {
        if (out_err != NULL)
        {
            *out_err = MC_ENABLE_ERROR_MODEL_NOT_FOUND;
        }
        return -1;
    }
    {
        const size_t len = strlen(model_path);
        if (len + 1 > sizeof(resolved_model))
        {
            if (out_err != NULL)
            {
                *out_err = MC_ENABLE_ERROR_MODEL_PATH_TOO_LONG;
            }
            return -1;
        }
        memset(resolved_model, 0, sizeof(resolved_model));
        memcpy(resolved_model, model_path, len + 1);
    }

    reset_session();
    memset(&params, 0, sizeof(params));

    params.input_type = INPUT_TEXTURE_RGB8Unorm;
    params.backend = resolved_backend;

    strncpy(params.model_path, resolved_model, sizeof(params.model_path) - 1);
    params.width = width;
    params.height = height;
    params.scaler_factor = scale;
    params.alg_mode = mode;
    params.num_threads = 1;
    params.log_level = MAGIC_LOG_INFO;
    params.sharpen_level = g_sharpen_level;

    g_session = MC_Init(&params);
    if (g_session == NULL)
    {
        if (out_err != NULL)
        {
            *out_err = MC_ENABLE_ERROR_INIT_FAILED;
        }
        MC_ENABLE_LOGE("MC_Init failed model=%s backend=%d mode=%d %ux%u scale=%.3f",
                       resolved_model,
                       (int)resolved_backend,
                       (int)mode,
                       width,
                       height,
                       scale);
        return -1;
    }

    g_scale = scale;
    g_width = width;
    g_height = height;
    g_alg_mode = mode;
    g_backend = resolved_backend;
    g_session_sharpen_level = g_sharpen_level;
    return 0;
}

/*
 * Internal implementation. All args (including model_path) must already be chosen.
 * Not exported from mc_enable.h (D1).
 */
static void* enable_ex(void* input_texture,
                       float scale,
                       alg_mode_e mode,
                       magic_backend_e backend,
                       const char* model_path)
{
    unsigned int width = 0;
    unsigned int height = 0;
    void* output;
    int ret;
    int session_err = 0;
    magic_backend_e resolved_backend;
    const int64_t t_total0 = mc_enable_now_us();
    int64_t t0;

    if (input_texture == NULL)
    {
        enable_fail(MC_ENABLE_ERROR_NULL_INPUT, "input_texture is NULL");
        return NULL;
    }

    if (scale <= 0.0f)
    {
        scale = MC_ENABLE_DEFAULT_SCALE;
    }
    if (scale < 1.0f || scale > 8.0f)
    {
        enable_fail(MC_ENABLE_ERROR_SCALE_OUT_OF_RANGE, "scale must be in [1.0, 8.0]");
        return NULL;
    }
    if (mode < SPEED_MODE || mode >= MAX_ALG_MODE)
    {
        enable_fail(MC_ENABLE_ERROR_MODE_INVALID, "alg_mode invalid");
        return NULL;
    }
    if (backend < MAGIC_BACKEND_DEFAULT || backend > MAGIC_BACKEND_VULKAN)
    {
        enable_fail(MC_ENABLE_ERROR_BACKEND_INVALID, "backend invalid");
        return NULL;
    }
    if (model_path == NULL || model_path[0] == '\0')
    {
        enable_fail(MC_ENABLE_ERROR_MODEL_NOT_FOUND, "model_path not selected");
        return NULL;
    }
    if (strlen(model_path) >= 256)
    {
        enable_fail(MC_ENABLE_ERROR_MODEL_PATH_TOO_LONG, "model path too long");
        return NULL;
    }

    mc_enable_note_stage(MC_ENABLE_STAGE_RESOLVE_MODEL, g_pending_resolve_model_us);
    g_pending_resolve_model_us = 0;

    resolved_backend = resolve_backend(backend);
    t0 = mc_enable_now_us();
    if (query_texture_size(input_texture, &width, &height, backend) != 0)
    {
        mc_enable_note_stage(MC_ENABLE_STAGE_QUERY_SIZE, mc_enable_now_us() - t0);
        if (backend_requires_size_hint(resolved_backend))
        {
            enable_fail(MC_ENABLE_ERROR_SIZE_HINT_REQUIRED,
                        "call MC_Enable_SetInputSizeHint(width,height) before Enable on Android GLES/Vulkan");
        }
        else
        {
            enable_fail(MC_ENABLE_ERROR_SIZE_QUERY_FAILED, "cannot query input texture size");
        }
        return NULL;
    }
    mc_enable_note_stage(MC_ENABLE_STAGE_QUERY_SIZE, mc_enable_now_us() - t0);
    if (width < 64 || width > 4032 || height < 64 || height > 4032)
    {
        enable_fail(MC_ENABLE_ERROR_SIZE_OUT_OF_RANGE, "width/height must be in [64, 4032]");
        return NULL;
    }

    t0 = mc_enable_now_us();
    if (ensure_session(scale, width, height, mode, backend, model_path, &session_err) != 0)
    {
        mc_enable_note_stage(MC_ENABLE_STAGE_ENSURE_SESSION, mc_enable_now_us() - t0);
        if (session_err == MC_ENABLE_ERROR_MODEL_PATH_TOO_LONG)
        {
            enable_fail(session_err, "model path too long");
        }
        else if (session_err == MC_ENABLE_ERROR_INIT_FAILED)
        {
            enable_fail(session_err, "MC_Init returned NULL (see prior log)");
        }
        else if (session_err != 0)
        {
            enable_fail(session_err, "session setup failed");
        }
        else
        {
            enable_fail(MC_ENABLE_ERROR_INIT_FAILED, "session setup failed");
        }
        return NULL;
    }
    mc_enable_note_stage(MC_ENABLE_STAGE_ENSURE_SESSION, mc_enable_now_us() - t0);

    t0 = mc_enable_now_us();
    output = acquire_output(g_session, input_texture);
    mc_enable_note_stage(MC_ENABLE_STAGE_ACQUIRE_OUTPUT, mc_enable_now_us() - t0);
    if (output == NULL)
    {
        enable_fail(MC_ENABLE_ERROR_OUTPUT_ACQUIRE, "failed to acquire output texture");
        reset_session();
        return NULL;
    }

    t0 = mc_enable_now_us();
    ret = MC_Process(g_session, input_texture, output);
    mc_enable_note_stage(MC_ENABLE_STAGE_MC_PROCESS, mc_enable_now_us() - t0);
    if (ret != 0)
    {
        enable_fail(ret, "MC_Process failed");
        return NULL;
    }

    {
        output_status_params_t st;
        memset(&st, 0, sizeof(st));
        if (MC_Control(g_session, QUERY_STATUS, NULL, &st) == 0 && st.gpu_time > 0.0)
            g_last_fused_cb_ms = st.gpu_time * 1000.0;
        else
            g_last_fused_cb_ms = 0.0;
    }

    /* total = resolve(outside enable_ex) + timed stages inside enable_ex */
    mc_enable_note_stage(MC_ENABLE_STAGE_TOTAL,
                         g_stage_last_us[MC_ENABLE_STAGE_RESOLVE_MODEL] +
                             (mc_enable_now_us() - t_total0));
    mc_enable_finish_frame_timing();
    return output;
}

int64_t MC_Enable_GetLastProcessTimeUs(void)
{
    return g_last_mc_process_us;
}

double MC_Enable_GetLastProcessAvg30Ms(void)
{
    return g_last_mc_process_avg30_ms;
}

int64_t MC_Enable_GetLastEnableTimeUs(void)
{
    return g_last_mc_enable_us;
}

double MC_Enable_GetLastEnableAvg30Ms(void)
{
    return g_last_mc_enable_avg30_ms;
}

double MC_Enable_GetLastFusedCbMs(void)
{
    return g_last_fused_cb_ms;
}

void* MC_Enable_4params(void* input_texture, float scale, alg_mode_e mode, magic_backend_e backend)
{
    char model_path[256];
    int64_t t0;

    if (mode < SPEED_MODE || mode >= MAX_ALG_MODE)
    {
        enable_fail(MC_ENABLE_ERROR_MODE_INVALID, "alg_mode invalid");
        return NULL;
    }
    if (backend < MAGIC_BACKEND_DEFAULT || backend > MAGIC_BACKEND_VULKAN)
    {
        enable_fail(MC_ENABLE_ERROR_BACKEND_INVALID, "backend invalid");
        return NULL;
    }
    t0 = mc_enable_now_us();
    if (resolve_default_model(model_path,
                              sizeof(model_path),
                              mode,
                              resolve_backend(backend)) != 0)
    {
        g_pending_resolve_model_us = mc_enable_now_us() - t0;
        enable_fail(MC_ENABLE_ERROR_MODEL_NOT_FOUND,
                    "call MC_Enable_SetModelPath/SetModelDir or place default model bin");
        return NULL;
    }
    g_pending_resolve_model_us = mc_enable_now_us() - t0;
    return enable_ex(input_texture, scale, mode, backend, model_path);
}

void* MC_Enable_3params(void* input_texture, float scale, alg_mode_e mode)
{
    char model_path[256];
    int64_t t0;

    if (mode < SPEED_MODE || mode >= MAX_ALG_MODE)
    {
        enable_fail(MC_ENABLE_ERROR_MODE_INVALID, "alg_mode invalid");
        return NULL;
    }
    t0 = mc_enable_now_us();
    if (resolve_default_model(model_path,
                              sizeof(model_path),
                              mode,
                              resolve_backend(MAGIC_BACKEND_DEFAULT)) != 0)
    {
        g_pending_resolve_model_us = mc_enable_now_us() - t0;
        enable_fail(MC_ENABLE_ERROR_MODEL_NOT_FOUND,
                    "call MC_Enable_SetModelPath/SetModelDir or place default model bin");
        return NULL;
    }
    g_pending_resolve_model_us = mc_enable_now_us() - t0;
    return enable_ex(input_texture, scale, mode, MAGIC_BACKEND_DEFAULT, model_path);
}

void* MC_Enable(void* input_texture, float scale)
{
    char model_path[256];
    int64_t t0;

    t0 = mc_enable_now_us();
    if (resolve_default_model(model_path,
                              sizeof(model_path),
                              SPEED_MODE,
                              resolve_backend(MAGIC_BACKEND_DEFAULT)) != 0)
    {
        g_pending_resolve_model_us = mc_enable_now_us() - t0;
        enable_fail(MC_ENABLE_ERROR_MODEL_NOT_FOUND,
                    "call MC_Enable_SetModelPath/SetModelDir or place default model bin");
        return NULL;
    }
    g_pending_resolve_model_us = mc_enable_now_us() - t0;
    return enable_ex(input_texture,
                     scale,
                     SPEED_MODE,
                     MAGIC_BACKEND_DEFAULT,
                     model_path);
}

int MC_Disable(void* handle)
{
    (void)handle;
    reset_session();
    return 0;
}
