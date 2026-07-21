#include "magic_sr_plugin.h"

#include <cstdlib>
#include <cstring>

#include "../../../interface/mc_interface.h"
#include "../../../interface/mc_enable.h"

namespace {
constexpr float kMinScale = 1.0f;
constexpr float kMaxScale = 8.0f;
constexpr int kMinMode = 0;
constexpr int kMaxMode = MAX_ALG_MODE - 1;
constexpr int kDefaultThreads = 1;
constexpr int kMinThreads = 1;
constexpr int kMaxThreads = 8;

bool IsValidInputType(int input_type) {
    return input_type >= INPUT_BUFFER && input_type < MAX_INPUT_TYPE;
}

bool IsValidBackend(int backend) {
    return backend >= MAGIC_BACKEND_DEFAULT && backend <= MAGIC_BACKEND_VULKAN;
}
}  // namespace

extern "C" const char* MagicSR_GetVersion(void) { return MC_GetVersion(); }

extern "C" void* MagicSR_Create(const char* model_path,
                                int width,
                                int height,
                                float scaler_factor,
                                int alg_mode,
                                int num_threads) {
    return MagicSR_CreateEx(model_path, width, height, scaler_factor, alg_mode, num_threads,
                            INPUT_BUFFER, MAGIC_BACKEND_NEON);
}

extern "C" void* MagicSR_CreateEx(const char* model_path,
                                  int width,
                                  int height,
                                  float scaler_factor,
                                  int alg_mode,
                                  int num_threads,
                                  int input_type,
                                  int backend) {
    if (model_path == nullptr || width <= 0 || height <= 0) {
        return nullptr;
    }

    if (scaler_factor < kMinScale || scaler_factor > kMaxScale) {
        return nullptr;
    }

    if (alg_mode < kMinMode || alg_mode > kMaxMode) {
        return nullptr;
    }

    if (num_threads < kMinThreads || num_threads > kMaxThreads) {
        num_threads = kDefaultThreads;
    }

    if (!IsValidInputType(input_type) || !IsValidBackend(backend)) {
        return nullptr;
    }

    input_param_t params;
    std::memset(&params, 0, sizeof(params));
    params.input_type = static_cast<input_type_e>(input_type);
    std::strncpy(params.model_path, model_path, sizeof(params.model_path) - 1);
    params.width = static_cast<unsigned int>(width);
    params.height = static_cast<unsigned int>(height);
    params.scaler_factor = scaler_factor;
    params.alg_mode = static_cast<alg_mode_e>(alg_mode);
    params.num_threads = static_cast<unsigned int>(num_threads);
    params.log_level = MAGIC_LOG_INFO;
    params.backend = static_cast<magic_backend_e>(backend);
    return MC_Init(&params);
}

extern "C" void MagicSR_SetModelDir(const char* model_dir) {
    MC_Enable_SetModelDir(model_dir);
}

extern "C" void MagicSR_SetModelPath(const char* model_path) {
    MC_Enable_SetModelPath(model_path);
}



extern "C" void MagicSR_SetInputSizeHint(unsigned int width, unsigned int height) {
    MC_Enable_SetInputSizeHint(width, height);
}

extern "C" void* MagicSR_Enable_4params(void* input_texture, float scale, int alg_mode, int backend) {
    if (input_texture == nullptr) {
        return nullptr;
    }
    if (alg_mode < kMinMode || alg_mode > kMaxMode) {
        return nullptr;
    }
    if (!IsValidBackend(backend)) {
        return nullptr;
    }
    const float resolved_scale = scale <= 0.0f ? 2.0f : scale;
    if (resolved_scale < kMinScale || resolved_scale > kMaxScale) {
        return nullptr;
    }
    return MC_Enable_4params(input_texture,
                             resolved_scale,
                             static_cast<alg_mode_e>(alg_mode),
                             static_cast<magic_backend_e>(backend));
}

extern "C" void* MagicSR_Enable_3params(void* input_texture, float scale, int alg_mode) {
    return MagicSR_Enable_4params(input_texture, scale, alg_mode, MAGIC_BACKEND_DEFAULT);
}

extern "C" void* MagicSR_Enable(void* input_texture, float scale) {
    return MagicSR_Enable_3params(input_texture, scale, HIGH_SPEED_MODE);
}

extern "C" int MagicSR_Disable(void* handle) {
    if (handle == nullptr) {
        return 0;
    }
    return MC_Disable(handle);
}

extern "C" int MagicSR_Process(void* handle,
                               const uint8_t* input_y,
                               int input_size,
                               uint8_t* output_y,
                               int output_size) {
    if (handle == nullptr || input_y == nullptr || output_y == nullptr) {
        return -1001;
    }

    output_status_params_t status;
    std::memset(&status, 0, sizeof(status));
    const int status_ret = MC_Control(handle, QUERY_STATUS, nullptr, &status);
    if (status_ret != 0) {
        return -1002;
    }

    const int expected_input = static_cast<int>(status.width * status.height);
    const int expected_output =
        static_cast<int>(status.output_width * status.output_height);
    if (input_size < expected_input || output_size < expected_output) {
        return -1003;
    }

    return MC_Process(handle, const_cast<uint8_t*>(input_y), output_y);
}

extern "C" int MagicSR_ProcessTexture(void* handle, void* input_texture, void* output_texture) {
    if (handle == nullptr || input_texture == nullptr || output_texture == nullptr) {
        return -1001;
    }
    return MC_Process(handle, input_texture, output_texture);
}

extern "C" int MagicSR_SetParam(void* handle,
                                 int cmd,
                                 const MagicSRControlParam* param,
                                 MagicSRStatus* out_status) {
    if (handle == nullptr) {
        return -1001;
    }

    if (cmd == QUERY_STATUS) {
        if (out_status == nullptr) {
            return -1002;
        }

        output_status_params_t status;
        std::memset(&status, 0, sizeof(status));
        const int ret = MC_Control(handle, QUERY_STATUS, nullptr, &status);
        if (ret != 0) {
            return ret;
        }

        out_status->width = status.width;
        out_status->height = status.height;
        out_status->output_width = status.output_width;
        out_status->output_height = status.output_height;
        out_status->scaler_factor = status.scaler_factor;
        out_status->alg_mode = static_cast<uint32_t>(status.alg_mode);
        out_status->input_type = static_cast<uint32_t>(status.input_type);
        out_status->backend = static_cast<uint32_t>(status.backend);
        out_status->num_threads = status.num_threads;
        out_status->gpu_time = status.gpu_time;
        out_status->error_code = status.error_code;
        return 0;
    }

    if (cmd != SET_PARAM) {
        return -1003;
    }

    if (param == nullptr) {
        return -1004;
    }

    if (param->width == 0 || param->height == 0) {
        return -1005;
    }

    if (param->scaler_factor < kMinScale || param->scaler_factor > kMaxScale) {
        return -1006;
    }

    if (param->alg_mode < kMinMode || param->alg_mode > kMaxMode) {
        return -1007;
    }

    control_param_t control;
    std::memset(&control, 0, sizeof(control));
    control.width = param->width;
    control.height = param->height;
    control.scaler_factor = param->scaler_factor;
    control.alg_mode = static_cast<alg_mode_e>(param->alg_mode);
    std::strncpy(control.model_path, param->model_path, sizeof(control.model_path) - 1);

    return MC_Control(handle, SET_PARAM, &control, nullptr);
}

extern "C" int MagicSR_Destroy(void* handle) {
    if (handle == nullptr) {
        return 0;
    }
    return MC_Uninit(handle);
}
