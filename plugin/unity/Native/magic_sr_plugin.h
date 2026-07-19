#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MagicSRStatus {
    uint32_t width;
    uint32_t height;
    uint32_t output_width;
    uint32_t output_height;
    float scaler_factor;
    uint32_t alg_mode;
    uint32_t input_type;
    uint32_t backend;
    uint32_t num_threads;
    double gpu_time;
    uint32_t error_code;
} MagicSRStatus;

typedef struct MagicSRControlParam {
    uint32_t width;
    uint32_t height;
    float scaler_factor;
    uint32_t alg_mode;
    char model_path[256];
} MagicSRControlParam;

enum {
    MAGIC_SR_CMD_SET_PARAM = 0,
    MAGIC_SR_CMD_QUERY_STATUS = 1,
};

const char* MagicSR_GetVersion(void);
void* MagicSR_Create(const char* model_path,
                     int width,
                     int height,
                     float scaler_factor,
                     int alg_mode,
                     int num_threads);
void* MagicSR_CreateEx(const char* model_path,
                       int width,
                       int height,
                       float scaler_factor,
                       int alg_mode,
                       int num_threads,
                       int input_type,
                       int backend);

/** Optional: set MAGIC_SR_MODEL_DIR for MC_Enable model resolution (e.g. Unity persistentDataPath/MagicSRModels). */
void MagicSR_SetModelDir(const char* model_dir);

/** Optional: hint input WxH before Enable* (needed for Android Vulkan). Pass 0,0 to clear. */
void MagicSR_SetInputSizeHint(unsigned int width, unsigned int height);

/** One-call: process frame via MC_Enable*. Returns borrowed output texture (owned until Disable).
 *  Do not free/CFRelease/glDelete the pointer; call MagicSR_Disable only. Session is reused.
 *  scale in [1,8]; <=0 => 2.0f */
void* MagicSR_Enable(void* input_texture, float scale);
void* MagicSR_Enable_3params(void* input_texture, float scale, int alg_mode);
void* MagicSR_Enable_4params(void* input_texture, float scale, int alg_mode, int backend);
/** Tear down MC_Enable session (handle ignored; may be NULL). */
int MagicSR_Disable(void* handle);

int MagicSR_Process(void* handle,
                    const uint8_t* input_y,
                    int input_size,
                    uint8_t* output_y,
                    int output_size);
int MagicSR_ProcessTexture(void* handle, void* input_texture, void* output_texture);
int MagicSR_SetParam(void* handle, int cmd, const MagicSRControlParam* param, MagicSRStatus* out_status);
int MagicSR_Destroy(void* handle);

#ifdef __cplusplus
}
#endif
