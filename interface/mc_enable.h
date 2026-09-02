/************************************************************************************
 * Copyright (C) 2024-2026 MagicCode Technology Co., Ltd. All rights reserved.
 *
 * @file        mc_enable.h
 * @brief       Two-call Enable/Disable API for MagicCode Super-Resolution
 * @details     Only MC_Enable* + MC_Disable are required for the simple path.
 *              Prefer linking libmagic_sr_enable.a (core + Enable). On Windows
 *              the product name is libmagic_enable_sr.lib. The core-only
 *              libmagic_sr.a / libmagic_sr.lib does not export MC_Enable*;
 *              advanced session users may still link that and omit Enable.
 *              MC_Enable lazily creates a session, runs one SR frame, and returns
 *              the output GPU texture. MC_Disable releases everything.
 *
 *              Public variants (C-style named overloads; one shared implementation):
 *                MC_Enable          — texture + scale (default mode/backend)
 *                MC_Enable_3params  — + alg_mode
 *                MC_Enable_4params  — + alg_mode + backend
 *
 *              Model path: call MC_Enable_SetModelPath / SetModelDir before Enable,
 *              or rely on default directory / bundle search.
 ************************************************************************************/

#ifndef MC_ENABLE_H
#define MC_ENABLE_H

#include "mc_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Enable-path failure codes (logged as [MagicSR][Enable] err=<code>).
 * MC_Process failures log the underlying MC_ERROR_PROCESS_* code instead. */
#define MC_ENABLE_ERROR_NULL_INPUT            (-300001) /* input_texture is NULL */
#define MC_ENABLE_ERROR_SCALE_OUT_OF_RANGE    (-300002) /* scale outside [1.0, 8.0] after defaulting */
#define MC_ENABLE_ERROR_MODE_INVALID          (-300003) /* alg_mode out of range */
#define MC_ENABLE_ERROR_BACKEND_INVALID       (-300004) /* backend out of range */
#define MC_ENABLE_ERROR_MODEL_PATH_TOO_LONG   (-300005) /* explicit model path >= 256 */
#define MC_ENABLE_ERROR_SIZE_HINT_REQUIRED    (-300006) /* Android GLES/Vulkan need SetInputSizeHint */
#define MC_ENABLE_ERROR_SIZE_QUERY_FAILED     (-300007) /* cannot determine input WxH */
#define MC_ENABLE_ERROR_SIZE_OUT_OF_RANGE     (-300008) /* width/height outside [64, 4032] */
#define MC_ENABLE_ERROR_MODEL_NOT_FOUND       (-300009) /* no readable default/env model file */
#define MC_ENABLE_ERROR_INIT_FAILED           (-300010) /* MC_Init returned NULL */
#define MC_ENABLE_ERROR_OUTPUT_ACQUIRE        (-300011) /* failed to acquire output GPU texture */

/**
 * @brief Enable SR (lazy init) and process one frame from input_texture.
 * @details Internally presets backend, input_type, model path, and dimensions.
 *          Reuses the session across calls when scale/size/mode/backend are unchanged.
 *          On failure returns NULL and logs a clear error code to logcat / stderr.
 * @param input_texture Native GPU texture (first argument — not scale):
 *        - macOS / iOS Metal: MTLTexture* (RGBA8Unorm)
 *        - Windows OpenGL (Enable default): GLuint texture id cast to void* (RGBA8)
 *        - Android OpenGLES: GLuint as void* (RGBA8)
 *        - Android / Windows Vulkan: VulkanTexture* / RGB8Unorm
 *        Enable does not accept MAGIC_BACKEND_D3D11.
 * @param scale Super-resolution scale in [1.0, 8.0]; pass 0 (or <= 0) for default 2.0.
 *        This path feeds a single color texture into MC_Process. It cannot
 *        supply per-frame depth, motion, or jitter.
 * @return Output GPU texture pointer owned by the library until MC_Disable().
 *         NULL on failure. Do not free / CFRelease / glDelete the returned
 *         pointer -- on Metal an erroneous CFRelease aborts at the release site
 *         (not on the next MC_Enable). Release only via MC_Disable().
 */
void* MC_Enable(void* input_texture, float scale);

/**
 * @brief Same as MC_Enable, with an explicit algorithm mode.
 * @param mode Any alg_mode_e in [SPATIAL_SPEED_MODE, MAX_ALG_MODE).
 *        Spatial modes are the intended Enable path.
 *        Temporal enums are accepted by range check, but Enable still
 *        provides only one input texture and no per-frame depth / motion /
 *        jitter / camera / command-buffer contract. Complete temporal
 *        integration must use libmagic_sr + mc_interface.h
 *        (MC_Init / MC_Process / MC_Uninit), not this API.
 */
void* MC_Enable_3params(void* input_texture, float scale, alg_mode_e mode);

/**
 * @brief Same as MC_Enable_3params, with an explicit runtime backend.
 * @param backend MAGIC_BACKEND_DEFAULT selects the platform default
 *        (Metal / OpenGL / OpenGLES / Vulkan). Values through
 *        MAGIC_BACKEND_VULKAN are accepted; MAGIC_BACKEND_D3D11 is not
 *        an Enable backend. Temporal completeness still requires the
 *        session API regardless of backend.
 */
void* MC_Enable_4params(void* input_texture, float scale, alg_mode_e mode, magic_backend_e backend);

/**
 * @brief Set input width/height for backends that cannot query texture size.
 * @details Required before MC_Enable* on Android OpenGLES and Android Vulkan
 *          (both return MC_ENABLE_ERROR_SIZE_HINT_REQUIRED without a valid hint).
 *          Optional on Metal / Windows OpenGL (and Windows Vulkan when
 *          size can be queried). Pass 0,0 to clear.
 *          Hint values must be in [64, 4032].
 */
void MC_Enable_SetInputSizeHint(unsigned int width, unsigned int height);

/**
 * @brief Set sharpen grade [0, 5] for the next MC_Enable* session.
 * @details 0 = off. BALANCED applies output sharpening; SPEED selects combined-bin segment 1+level.
 *          The new grade is applied the next time MC_Enable* runs (no immediate uninit).
 */
void MC_Enable_SetSharpenLevel(unsigned int level);

/**
 * @brief Set the full path to one model .bin used by the next MC_Enable*.
 * @details Preferred over SetModelDir. Pass NULL or "" to clear.
 *          Path length must be < 256. Changing the path rebuilds the Enable session.
 * @param model_path Absolute (recommended) or readable path to a .bin file
 */
void MC_Enable_SetModelPath(const char* model_path);

/**
 * @brief Set the directory that contains default model .bin filenames.
 * @details Used when SetModelPath is not set. Looks up known basenames under
 *          model_dir recursively (depth up to 4; not a wildcard scan).
 *          Pass NULL or "" to clear. Changing the dir rebuilds the Enable session.
 * @param model_dir Directory root, e.g. .../MagicSRModels
 */
void MC_Enable_SetModelDir(const char* model_dir);

/**
 * @brief Tear down the session and output texture owned by MC_Enable*.
 * @param handle Ignored; may be NULL or the pointer previously returned by MC_Enable*.
 * @return 0 on success; negative error code on failure
 */
int MC_Disable(void* handle);

/**
 * @brief Wall-clock microseconds of the last MC_Process call inside MC_Enable*.
 * @details Measured immediately around MC_Process only (excludes session init /
 *          output acquire). Returns 0 before the first successful timing sample.
 */
int64_t MC_Enable_GetLastProcessTimeUs(void);

/**
 * @brief Latest completed 30-frame average (ms) of MC_Process inside MC_Enable*.
 * @details Updated every 30 MC_Process samples; 0.0 until the first window completes.
 */
double MC_Enable_GetLastProcessAvg30Ms(void);

/**
 * @brief Wall-clock microseconds of the last MC_Enable* call (full enable_ex).
 * @details Includes resolve/query/session/acquire/MC_Process/QUERY_STATUS bookkeeping.
 */
int64_t MC_Enable_GetLastEnableTimeUs(void);

/**
 * @brief Latest completed 30-frame average (ms) of full MC_Enable* wall time.
 */
double MC_Enable_GetLastEnableAvg30Ms(void);

/**
 * @brief Last Metal BALANCED fused 1-CB GPU time in ms (preprocess + CS), or 0 if unavailable.
 */
double MC_Enable_GetLastFusedCbMs(void);

#ifdef __cplusplus
}
#endif
#endif
