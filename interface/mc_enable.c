#include "mc_enable.h"

/*
 * Private magic_net_t is only needed where we must read handle_gpu
 * (Metal / Android Vulkan). Windows OpenGL uses the public MC_* API only,
 * so MSVC builds do not depend on src/common.h for this translation unit.
 */
#if defined(__APPLE__) || (defined(SYS_ANDROID) && defined(VULKAN))
#include "common.h"
#endif

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(SYS_ANDROID) && defined(VULKAN)
#include "vulkan/magic_sr_vulkan.h"
#define MC_ENABLE_HAS_VULKAN 1
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
void* mc_enable_metal_acquire_output(void* gpu_handle, unsigned int out_w, unsigned int out_h, int prefer_r8);
void mc_enable_metal_release_output(void* output);
#endif

enum {
    MC_ENABLE_VULKAN_FALLBACK_W = 720,
    MC_ENABLE_VULKAN_FALLBACK_H = 1280
};

static const float MC_ENABLE_DEFAULT_SCALE = 2.0f;
static const float MC_ENABLE_SCALE_EPS = 1.0e-3f;

static void* g_session = NULL;
static void* g_output = NULL;
static float g_scale = 0.0f;
static unsigned int g_width = 0;
static unsigned int g_height = 0;
static alg_mode_e g_alg_mode = HIGH_SPEED_MODE;
static magic_backend_e g_backend = MAGIC_BACKEND_DEFAULT;
static int g_output_owned = 0; /* 1 = we created it (Metal/GL); 0 = borrowed (Vulkan) */
static unsigned int g_size_hint_w = 0;
static unsigned int g_size_hint_h = 0;

static magic_backend_e platform_default_backend(void);
static magic_backend_e resolve_backend(magic_backend_e backend);

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

static int resolve_default_model(char* out, size_t out_size)
{
    const char* env_model;
    const char* env_dir;
    const char* names[4];
    int name_count = 0;
    int i;

    if (out == NULL || out_size == 0)
    {
        return -1;
    }
    out[0] = '\0';

    env_model = getenv("MAGIC_SR_MODEL");
    if (try_copy_path(out, out_size, env_model))
    {
        return 0;
    }

#if defined(__APPLE__)
    /* iOS + macOS Metal */
    names[name_count++] = "magic_veryfast_gpu_params.bin";
    names[name_count++] = "magic_metal_highspeed_gpu_params.bin";
#elif defined(SYS_ANDROID) && defined(OpenGLES) && !defined(VULKAN)
    names[name_count++] = "magic_veryfast_gles_params.bin";
    names[name_count++] = "magic_gles_highspeed_gpu_params.bin";
#elif defined(_WIN32)
    /* Windows desktop OpenGL */
    names[name_count++] = "magic_veryfast_gpu_params.bin";
    names[name_count++] = "magic_gl_highspeed_gpu_params.bin";
    names[name_count++] = "magic_highspeed_gpu_params.bin";
#else
    names[name_count++] = "magic_veryfast_gpu_params.bin";
    names[name_count++] = "magic_highspeed_gpu_params.bin";
#if defined(SYS_ANDROID)
    names[name_count++] = "magic_veryfast_gles_params.bin";
#endif
#endif

    env_dir = getenv("MAGIC_SR_MODEL_DIR");
    for (i = 0; i < name_count; ++i)
    {
        char candidate[512];
        if (env_dir != NULL && env_dir[0] != '\0' &&
            join_dir_file(candidate, sizeof(candidate), env_dir, names[i]) &&
            try_copy_path(out, out_size, candidate))
        {
            return 0;
        }
        if (join_dir_file(candidate, sizeof(candidate), "MagicSRModels", names[i]) &&
            try_copy_path(out, out_size, candidate))
        {
            return 0;
        }
        if (try_copy_path(out, out_size, names[i]))
        {
            return 0;
        }
#if defined(SYS_ANDROID)
        if (join_dir_file(candidate, sizeof(candidate), "/sdcard/Documents/MagicSRModels", names[i]) &&
            try_copy_path(out, out_size, candidate))
        {
            return 0;
        }
        if (join_dir_file(candidate, sizeof(candidate), "/storage/emulated/0/Documents/MagicSRModels", names[i]) &&
            try_copy_path(out, out_size, candidate))
        {
            return 0;
        }
        if (join_dir_file(candidate, sizeof(candidate), "/storage/emulated/0/Documents", names[i]) &&
            try_copy_path(out, out_size, candidate))
        {
            return 0;
        }
#endif
    }

#if defined(__APPLE__)
    if (mc_enable_resolve_bundle_model(out, out_size) == 0 && path_is_readable(out))
    {
        return 0;
    }
#endif

    return -1;
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
        (void)input_texture;
        *width = MC_ENABLE_VULKAN_FALLBACK_W;
        *height = MC_ENABLE_VULKAN_FALLBACK_H;
        return 0;
    }
#endif
    return -1;
#elif MC_ENABLE_HAS_VULKAN
    (void)input_texture;
    (void)resolved;
    *width = MC_ENABLE_VULKAN_FALLBACK_W;
    *height = MC_ENABLE_VULKAN_FALLBACK_H;
    return 0;
#else
    (void)input_texture;
    (void)resolved;
    (void)width;
    (void)height;
    return -1;
#endif
}

static void release_owned_output(void)
{
    if (g_output == NULL)
    {
        g_output_owned = 0;
        return;
    }

#if defined(__APPLE__)
    if (g_output_owned)
    {
        mc_enable_metal_release_output(g_output);
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
    g_alg_mode = HIGH_SPEED_MODE;
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

static void* acquire_output(void* session)
{
    output_status_params_t status;
    unsigned int out_w;
    unsigned int out_h;
#if defined(__APPLE__) || (defined(SYS_ANDROID) && defined(VULKAN))
    magic_net_t* h;
#endif

    if (session == NULL)
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

#if defined(__APPLE__)
    h = (magic_net_t*)session;
    if (h->handle_gpu == NULL)
    {
        return NULL;
    }
    if (g_output != NULL && g_output_owned &&
        g_width == status.width && g_height == status.height && g_scale > 0.0f)
    {
        /* Keep existing Metal output while session size is unchanged. */
        return g_output;
    }
    release_owned_output();
    g_output = mc_enable_metal_acquire_output(
        h->handle_gpu,
        out_w,
        out_h,
        status.input_type == INPUT_TEXTURE_R8Unorm);
    g_output_owned = (g_output != NULL) ? 1 : 0;
    return g_output;
#else
#if MC_ENABLE_HAS_VULKAN
    if (status.backend == MAGIC_BACKEND_VULKAN)
    {
        SRLutVulkanInfo* vk;
        h = (magic_net_t*)session;
        if (h->handle_gpu == NULL)
        {
            return NULL;
        }
        vk = (SRLutVulkanInfo*)h->handle_gpu;
        if (vk == NULL || vk->y_outTexture == VK_NULL_HANDLE)
        {
            return NULL;
        }
        g_output = &vk->y_outTexture;
        g_output_owned = 0;
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
        if (g_gl_out_tex != 0 && g_output != NULL &&
            g_width == status.width && g_height == status.height)
        {
            return g_output;
        }
        if (g_gl_out_tex != 0)
        {
            glDeleteTextures(1, &g_gl_out_tex);
            g_gl_out_tex = 0;
        }
        glGenTextures(1, &g_gl_out_tex);
        glBindTexture(GL_TEXTURE_2D, g_gl_out_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if defined(_WIN32)
        /* Prefer TexImage2D for broader desktop GL compatibility. */
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

static void* ensure_session(float scale,
                            unsigned int width,
                            unsigned int height,
                            alg_mode_e mode,
                            magic_backend_e backend,
                            const char* model_path)
{
    input_param_t params;
    char resolved_model[256];
    magic_backend_e resolved_backend = resolve_backend(backend);

    if (g_session != NULL &&
        fabsf(g_scale - scale) <= MC_ENABLE_SCALE_EPS &&
        g_width == width &&
        g_height == height &&
        g_alg_mode == mode &&
        g_backend == resolved_backend)
    {
        return g_session;
    }

    memset(resolved_model, 0, sizeof(resolved_model));
    if (model_path != NULL && model_path[0] != '\0')
    {
        const size_t len = strlen(model_path);
        if (len + 1 > sizeof(resolved_model))
        {
            return NULL;
        }
        memcpy(resolved_model, model_path, len + 1);
    }
    else if (resolve_default_model(resolved_model, sizeof(resolved_model)) != 0)
    {
        return NULL;
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

    g_session = MC_Init(&params);
    if (g_session == NULL)
    {
        return NULL;
    }

    g_scale = scale;
    g_width = width;
    g_height = height;
    g_alg_mode = mode;
    g_backend = resolved_backend;
    return g_session;
}

/* Internal 5-arg implementation. Not exported from mc_enable.h (D1). */
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

    if (input_texture == NULL)
    {
        return NULL;
    }

    if (scale <= 0.0f)
    {
        scale = MC_ENABLE_DEFAULT_SCALE;
    }
    if (scale < 1.0f || scale > 8.0f)
    {
        return NULL;
    }
    if (mode < HIGH_SPEED_MODE || mode >= MAX_ALG_MODE)
    {
        return NULL;
    }
    if (backend < MAGIC_BACKEND_DEFAULT || backend > MAGIC_BACKEND_VULKAN)
    {
        return NULL;
    }
    if (model_path != NULL && model_path[0] != '\0' && strlen(model_path) >= 256)
    {
        return NULL;
    }

    if (query_texture_size(input_texture, &width, &height, backend) != 0)
    {
        return NULL;
    }
    if (width < 64 || width > 4032 || height < 64 || height > 4032)
    {
        return NULL;
    }

    if (ensure_session(scale, width, height, mode, backend, model_path) == NULL)
    {
        return NULL;
    }

    output = acquire_output(g_session);
    if (output == NULL)
    {
        reset_session();
        return NULL;
    }

    ret = MC_Process(g_session, input_texture, output);
    if (ret != 0)
    {
        return NULL;
    }

    return output;
}

void* MC_Enable_4params(void* input_texture, float scale, alg_mode_e mode, magic_backend_e backend)
{
    return enable_ex(input_texture, scale, mode, backend, NULL);
}

void* MC_Enable_3params(void* input_texture, float scale, alg_mode_e mode)
{
    return enable_ex(input_texture, scale, mode, MAGIC_BACKEND_DEFAULT, NULL);
}

void* MC_Enable(void* input_texture, float scale)
{
    return enable_ex(input_texture, scale, HIGH_SPEED_MODE, MAGIC_BACKEND_DEFAULT, NULL);
}

int MC_Disable(void* handle)
{
    (void)handle;
    reset_session();
    return 0;
}
