#include <jni.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES3/gl3.h>
#include <cstring>
#include <cstdlib>
#include <string>

extern "C" {
#include "mc_enable.h"
}

#define LOG_TAG "MagicMagnifierJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static void *g_sr_handle = nullptr;
static magic_backend_e g_backend = MAGIC_BACKEND_OPENGLES;
static float g_enable_scale = 2.0f;
static alg_mode_e g_enable_mode = HIGH_SPEED_MODE;
static const jlong kMcEnableApiHandle = 1;

static EGLDisplay g_display = EGL_NO_DISPLAY;
static EGLContext g_context = EGL_NO_CONTEXT;
static EGLSurface g_surface = EGL_NO_SURFACE;

static GLuint g_input_tex = 0;
static int g_input_w = 0;
static int g_input_h = 0;

static bool ensure_gles_context() {
    eglBindAPI(EGL_OPENGL_ES_API);
    if (g_display != EGL_NO_DISPLAY && g_context != EGL_NO_CONTEXT && g_surface != EGL_NO_SURFACE) {
        if (eglMakeCurrent(g_display, g_surface, g_surface, g_context)) return true;
        eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_surface != EGL_NO_SURFACE) eglDestroySurface(g_display, g_surface);
        if (g_context != EGL_NO_CONTEXT) eglDestroyContext(g_display, g_context);
        eglTerminate(g_display);
        g_display = EGL_NO_DISPLAY;
        g_context = EGL_NO_CONTEXT;
        g_surface = EGL_NO_SURFACE;
    }

    g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_display == EGL_NO_DISPLAY) return false;
    if (!eglInitialize(g_display, nullptr, nullptr)) return false;

    const EGLint config_attrs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint num = 0;
    if (!eglChooseConfig(g_display, config_attrs, &config, 1, &num) || num <= 0) return false;

    const EGLint ctx_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    g_context = eglCreateContext(g_display, config, EGL_NO_CONTEXT, ctx_attrs);
    if (g_context == EGL_NO_CONTEXT) return false;

    const EGLint surf_attrs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    g_surface = eglCreatePbufferSurface(g_display, config, surf_attrs);
    if (g_surface == EGL_NO_SURFACE) return false;

    return eglMakeCurrent(g_display, g_surface, g_surface, g_context) == EGL_TRUE;
}

static GLuint gles_create_texture(int width, int height) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (tex == 0) return 0;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

static void gles_destroy_texture(GLuint tex) {
    if (tex != 0) glDeleteTextures(1, &tex);
}

static void clear_gles_input_cache() {
    if (g_input_tex != 0) {
        gles_destroy_texture(g_input_tex);
        g_input_tex = 0;
    }
    g_input_w = g_input_h = 0;
}

static bool ensure_gles_input_texture(int in_w, int in_h) {
    if (!ensure_gles_context()) return false;
    if (g_input_tex != 0 && g_input_w == in_w && g_input_h == in_h) return true;
    clear_gles_input_cache();
    g_input_tex = gles_create_texture(in_w, in_h);
    if (g_input_tex == 0) return false;
    g_input_w = in_w;
    g_input_h = in_h;
    return true;
}

static void gles_upload_rgba(GLuint tex, int width, int height, const uint8_t *data) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static uint8_t *gles_readback_rgba(GLuint tex, int width, int height) {
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    if (fbo == 0) return nullptr;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        return nullptr;
    }
    size_t bytes = (size_t)width * (size_t)height * 4u;
    uint8_t *out = (uint8_t *)malloc(bytes);
    if (!out) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        return nullptr;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, out);
    GLenum err = glGetError();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    if (err != GL_NO_ERROR) {
        free(out);
        return nullptr;
    }
    return out;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_example_superresolution_natives_SuperResolutionLib_initSuperResolution(
        JNIEnv *env,
        jclass,
        jint width,
        jint height,
        jfloat scaler_factor,
        jint sr_mode,
        jint sr_on_gpu,
        jint backend,
        jstring model_path) {
    (void)sr_on_gpu;
    if (width <= 0 || height <= 0 || scaler_factor <= 0.0f) {
        LOGE("invalid init args");
        return 0;
    }
    const char *model_path_cstr = model_path ? env->GetStringUTFChars(model_path, nullptr) : "";

    MC_Disable(nullptr);
    g_sr_handle = nullptr;
    clear_gles_input_cache();

    g_backend = (magic_backend_e)backend;
    if (g_backend == MAGIC_BACKEND_DEFAULT) g_backend = MAGIC_BACKEND_OPENGLES;
    g_enable_scale = scaler_factor;
    g_enable_mode = (alg_mode_e)sr_mode;

    if (model_path_cstr && model_path_cstr[0] != '\0') {
        MC_Enable_SetModelPath(model_path_cstr);
    } else {
        MC_Enable_SetModelPath(nullptr);
    }
    if (model_path) env->ReleaseStringUTFChars(model_path, model_path_cstr);

    if (g_backend != MAGIC_BACKEND_OPENGLES) {
        LOGE("magnifier demo only supports OpenGLES MC_Enable path, backend=%d", (int)g_backend);
        return 0;
    }
    if (!ensure_gles_context()) {
        LOGE("gles context create failed");
        return 0;
    }

    MC_Enable_SetInputSizeHint((unsigned int)width, (unsigned int)height);
    g_sr_handle = (void *)(uintptr_t)kMcEnableApiHandle;
    LOGI("MC_Enable path ready scale=%.3f mode=%d", g_enable_scale, (int)g_enable_mode);
    return kMcEnableApiHandle;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_superresolution_natives_SuperResolutionLib_processImage(
        JNIEnv *env,
        jclass,
        jbyteArray input,
        jbyteArray output,
        jint channel_num,
        jint input_width,
        jint input_height,
        jint output_width,
        jint output_height) {
    if (!g_sr_handle) return -1;
    if (!env || !input || !output) return -2;
    if (channel_num != 4) return -3;
    if (g_backend != MAGIC_BACKEND_OPENGLES) return -4;

    jbyte *in_bytes = env->GetByteArrayElements(input, nullptr);
    jbyte *out_bytes = env->GetByteArrayElements(output, nullptr);
    if (!in_bytes || !out_bytes) {
        if (in_bytes) env->ReleaseByteArrayElements(input, in_bytes, JNI_ABORT);
        if (out_bytes) env->ReleaseByteArrayElements(output, out_bytes, 0);
        return -4;
    }

    int ret = 0;
    if (!ensure_gles_input_texture(input_width, input_height)) {
        env->ReleaseByteArrayElements(input, in_bytes, JNI_ABORT);
        env->ReleaseByteArrayElements(output, out_bytes, 0);
        return -5;
    }

    gles_upload_rgba(g_input_tex, input_width, input_height, (const uint8_t *)in_bytes);
    MC_Enable_SetInputSizeHint((unsigned int)input_width, (unsigned int)input_height);
    void *outPtr = MC_Enable_4params((void *)(uintptr_t)g_input_tex,
                                     g_enable_scale,
                                     g_enable_mode,
                                     MAGIC_BACKEND_OPENGLES);
    if (outPtr == nullptr) {
        LOGE("MC_Enable_4params failed");
        env->ReleaseByteArrayElements(input, in_bytes, JNI_ABORT);
        env->ReleaseByteArrayElements(output, out_bytes, 0);
        return -1;
    }

    GLuint outTex = (GLuint)(uintptr_t)outPtr;
    uint8_t *readback = gles_readback_rgba(outTex, output_width, output_height);
    if (!readback) {
        ret = -6;
    } else {
        size_t out_size = (size_t)output_width * (size_t)output_height * 4u;
        memcpy(out_bytes, readback, out_size);
        free(readback);
    }

    env->ReleaseByteArrayElements(input, in_bytes, JNI_ABORT);
    env->ReleaseByteArrayElements(output, out_bytes, 0);
    return ret;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_superresolution_natives_SuperResolutionLib_uninitSuperResolution(
        JNIEnv *,
        jclass) {
    MC_Disable(nullptr);
    g_sr_handle = nullptr;
    clear_gles_input_cache();
}
