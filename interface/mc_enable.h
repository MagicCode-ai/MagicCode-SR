/************************************************************************************
 * Copyright (C) 2024-2026 MagicCode Technology Co., Ltd. All rights reserved.
 *
 * @file        mc_enable.h
 * @brief       Two-call Enable/Disable API for MagicCode Super-Resolution
 * @details     Only MC_Enable* + MC_Disable are required for the simple path.
 *              MC_Enable lazily creates a session, runs one SR frame, and returns
 *              the output GPU texture. MC_Disable releases everything.
 *
 *              Public variants (C-style named overloads; one shared implementation):
 *                MC_Enable          — texture + scale (default mode/backend)
 *                MC_Enable_3params  — + alg_mode
 *                MC_Enable_4params  — + alg_mode + backend
 *
 *              Model path is resolved automatically (MAGIC_SR_MODEL / default bins).
 ************************************************************************************/

#ifndef MC_ENABLE_H
#define MC_ENABLE_H

#include "mc_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enable SR (lazy init) and process one frame from input_texture.
 * @details Internally presets backend, input_type, model path, and dimensions.
 *          Reuses the session across calls when scale/size/mode/backend are unchanged.
 * @param input_texture Native GPU texture:
 *        - macOS / iOS Metal: MTLTexture* (RGBA8Unorm)
 *        - Windows OpenGL: GLuint texture id cast to void* (RGBA8)
 *        - Android OpenGLES: GLuint as void* (RGBA8)
 *        - Android Vulkan: VulkanTexture* / RGB8Unorm
 * @param scale Super-resolution scale in [1.0, 8.0]; pass 0 (or <= 0) for default 2.0
 * @return Output GPU texture pointer owned by the library until MC_Disable().
 *         NULL on failure. Do not free / CFRelease / glDelete the returned
 *         pointer -- on Metal an erroneous CFRelease aborts at the release site
 *         (not on the next MC_Enable). Release only via MC_Disable().
 */
void* MC_Enable(void* input_texture, float scale);

/**
 * @brief Same as MC_Enable, with an explicit algorithm mode.
 * @param mode HIGH_SPEED_MODE or SPEED_MODE
 */
void* MC_Enable_3params(void* input_texture, float scale, alg_mode_e mode);

/**
 * @brief Same as MC_Enable_3params, with an explicit runtime backend.
 * @param backend MAGIC_BACKEND_DEFAULT selects the platform default
 *        (Metal / OpenGL / OpenGLES / Vulkan). Other values force that backend.
 */
void* MC_Enable_4params(void* input_texture, float scale, alg_mode_e mode, magic_backend_e backend);

/**
 * @brief Optional size hint for backends that cannot query texture dimensions
 *        (notably Android Vulkan). Call before MC_Enable* when input size is known.
 *        Pass 0,0 to clear. Hint is consumed by the next successful size query path.
 */
void MC_Enable_SetInputSizeHint(unsigned int width, unsigned int height);

/**
 * @brief Tear down the session and output texture owned by MC_Enable*.
 * @param handle Ignored; may be NULL or the pointer previously returned by MC_Enable*.
 * @return 0 on success; negative error code on failure
 */
int MC_Disable(void* handle);

#ifdef __cplusplus
}
#endif
#endif
