/************************************************************************************
 * Copyright (C) 2024-2026 MagicCode Technology Co., Ltd. All rights reserved.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file        mc_interface.h
 * @brief       API Interface Header File for MagicCode Super-Resolution (SR) Algorithm
 * @details     This file defines the core data structures and function interfaces
 *              for the MagicCode image super-resolution algorithm, including
 *              initialization, image processing, parameter control, resource release,
 *              and version query.
 * @author      MagicCode  Team
 * @date        2026-07-01
 * @version     V1.1.3
 * @copyright   Copyright (C) 2024-2026 MagicCode Technology Co., Ltd. All rights reserved.
 * @website     https://www.magiccode-ai.com
 ************************************************************************************/

#ifndef MC_INTERFACE_H
#define MC_INTERFACE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Command parameter enumeration for algorithm control
 * @details Defines the command types for setting parameters and querying status
 */
typedef enum cmd_params_e {
    SET_PARAM = 0,    // Set algorithm configuration parameters
    QUERY_STATUS,     // Query current algorithm running status and parameters
    MAX_CMD_NUMS      // Maximum number of command types (boundary marker)
} cmd_params_e;

typedef enum input_type_e {
    INPUT_BUFFER = 0,
    INPUT_TEXTURE_RGB8Unorm,
    INPUT_TEXTURE_R8Unorm,
    MAX_INPUT_TYPE
} input_type_e;

typedef enum alg_mode_e {
    HIGH_SPEED_MODE = 0,
    SPEED_MODE,
    MAX_ALG_MODE
} alg_mode_e;

typedef enum magic_backend_e {
    MAGIC_BACKEND_DEFAULT = 0,
    MAGIC_BACKEND_X86 = 1,
    MAGIC_BACKEND_NEON = 2,
    MAGIC_BACKEND_METAL = 3,
    MAGIC_BACKEND_OPENGL = 4,
    MAGIC_BACKEND_OPENGLES = 5,
    MAGIC_BACKEND_VULKAN = 6,
} magic_backend_e;

typedef enum log_level_e {
    MAGIC_LOG_NONE = 0,
    MAGIC_LOG_ERROR,
    MAGIC_LOG_WARNING,
    MAGIC_LOG_INFO,
    MAGIC_LOG_DEBUG
} log_level_e;

/* Public error codes returned by MC_* APIs or exposed through output_status_params_t.error_code. */
#define MC_ERROR_INIT_NULL_PARAM                  (-100001) /* MC_Init received a NULL input_param_t pointer. */
#define MC_ERROR_INIT_WIDTH_OUT_OF_RANGE          (-100002) /* Initialization width is outside the supported range. */
#define MC_ERROR_INIT_HEIGHT_OUT_OF_RANGE         (-100003) /* Initialization height is outside the supported range. */
#define MC_ERROR_INIT_SCALER_OUT_OF_RANGE         (-100004) /* Initialization scaler factor is outside the supported range. */
#define MC_ERROR_INIT_ALG_MODE_OUT_OF_RANGE       (-100005) /* Initialization algorithm mode is invalid. */
#define MC_ERROR_INIT_INPUT_TYPE_OUT_OF_RANGE     (-100006) /* Initialization input type is invalid. */
#define MC_ERROR_INIT_BACKEND_OUT_OF_RANGE        (-100007) /* Initialization backend enum value is invalid. */
#define MC_ERROR_INIT_BACKEND_UNAVAILABLE         (-100008) /* Requested backend is not available in this build. */
#define MC_ERROR_INIT_CPU_BACKEND_INPUT_TYPE      (-100009) /* CPU backend was requested with non-buffer input. */
#define MC_ERROR_INIT_GPU_BACKEND_INPUT_TYPE      (-100010) /* GPU backend was requested with buffer input. */
#define MC_ERROR_INIT_MODEL_PATH_TOO_LONG         (-100011) /* Model path exceeds the supported length. */
#define MC_ERROR_INIT_LOAD_PARAMS_FAILED          (-100012) /* Model parameters could not be loaded. */
#define MC_ERROR_INIT_TYPE_SIZE_MISMATCH          (-100013) /* Model primary data type size does not match the backend. */
#define MC_ERROR_INIT_HIGH_SPEED_TYPE_MISMATCH    (-100014) /* High-speed model secondary data type size is invalid. */
#define MC_ERROR_INIT_SPEED_TYPE_MISMATCH         (-100015) /* Speed model secondary data type size is invalid. */
#define MC_ERROR_INIT_QUANT_SCALER_INVALID        (-100016) /* Quantization scaler is incompatible with the current build. */
#define MC_ERROR_INIT_STORE_MODE_INVALID          (-100017) /* Model store mode is incompatible with data type sizes. */
#define MC_ERROR_INIT_NEON_QUANT_SHIFT_MISMATCH   (-100018) /* NEON speed mode quantization shift is unsupported. */
#define MC_ERROR_INIT_CPU_FEATURE_UNSUPPORTED     (-100019) /* Required CPU feature is not available at runtime. */
#define MC_ERROR_INIT_FUNC_UNSUPPORTED_SCALER     (-100020) /* No processing function exists for the selected scaler. */
#define MC_ERROR_INIT_FUNC_ASSIGN_FAILED          (-100021) /* Processing function assignment failed. */
#define MC_ERROR_INIT_THREAD_START_SEM_FAILED     (-100022) /* Worker thread start semaphore initialization failed. */
#define MC_ERROR_INIT_THREAD_DONE_SEM_FAILED      (-100023) /* Worker thread completion semaphore initialization failed. */
#define MC_ERROR_INIT_THREAD_CREATE_FAILED        (-100024) /* Worker thread creation failed. */
#define MC_ERROR_INIT_GPU_CREATE_FAILED           (-100025) /* GPU backend context creation failed. */
#define MC_ERROR_INIT_REPORT_THREAD_LOCK_FAILED   (-100026) /* Report thread mutex initialization failed. */
#define MC_ERROR_INIT_REPORT_THREAD_SEM_FAILED    (-100027) /* Report thread semaphore initialization failed. */
#define MC_ERROR_INIT_REPORT_THREAD_CREATE_FAILED (-100028) /* Report thread creation failed. */
#define MC_ERROR_INIT_MODEL_FILE_OPEN_FAILED      (-100029) /* Model file could not be opened. */
#define MC_ERROR_INIT_MODEL_READ_TAG_FAILED       (-100030) /* Failed to read model backend tag. */
#define MC_ERROR_INIT_MODEL_READ_TYPE_SIZE_FAILED (-100031) /* Failed to read model primary data type size. */
#define MC_ERROR_INIT_MODEL_READ_TYPE2_SIZE_FAILED (-100032) /* Failed to read model secondary data type size. */
#define MC_ERROR_INIT_MODEL_READ_QUANT_FAILED     (-100033) /* Failed to read model quantization factor. */
#define MC_ERROR_INIT_MODEL_READ_DIM_FAILED       (-100034) /* Failed to read model LUT dimension. */
#define MC_ERROR_INIT_MODEL_READ_LUT_NUMS_FAILED  (-100035) /* Failed to read model LUT count. */
#define MC_ERROR_INIT_MODEL_READ_LUT_LEN_FAILED   (-100036) /* Failed to read model primary LUT length. */
#define MC_ERROR_INIT_MODEL_READ_LUT2_LEN_FAILED  (-100037) /* Failed to read model secondary LUT length. */
#define MC_ERROR_INIT_MODEL_READ_SCALER_FAILED    (-100038) /* Failed to read model scaler factor. */
#define MC_ERROR_INIT_MODEL_READ_STORE_MODE_FAILED (-100039) /* Failed to read model store mode. */
#define MC_ERROR_INIT_MODEL_READ_TAB_SIZE_FAILED  (-100040) /* Failed to read fast x2 model table size. */
#define MC_ERROR_INIT_MODEL_TAB_SIZE_INVALID      (-100041) /* Fast x2 model table size is invalid. */
#define MC_ERROR_INIT_MODEL_TAG_INVALID           (-100042) /* Model backend tag is invalid. */
#define MC_ERROR_INIT_MODEL_LUT_NUMS_INVALID      (-100043) /* Model LUT count is unsupported. */
#define MC_ERROR_INIT_MODEL_SCALER_MISMATCH       (-100044) /* Model scaler factor does not match requested scaler. */
#define MC_ERROR_INIT_MODEL_STORE_MODE_INVALID    (-100045) /* Model store mode value is unsupported. */
#define MC_ERROR_INIT_MODEL_DATA_SHORT_READ       (-100046) /* Model payload is shorter than expected. */
#define MC_ERROR_INIT_MODEL_TAIL_INVALID          (-100047) /* Model tail marker is invalid. */
#define MC_ERROR_MEMORY_INIT_PARAMS_ALLOC_FAILED  (-200001) /* Memory allocation failed while parsing model parameters. */
#define MC_ERROR_MEMORY_INIT_REINIT_ALLOC_FAILED  (-200002) /* Memory allocation failed during reinitialization. */
#define MC_ERROR_MEMORY_INIT_THREAD_ALLOC_FAILED  (-200003) /* Memory allocation failed during thread pool initialization. */
#define MC_ERROR_MEMORY_INIT_FAST_X2_MODEL_ALLOC_FAILED (-200004) /* Memory allocation failed for fast x2 model data. */
#define MC_ERROR_MEMORY_INIT_MODEL_DATA_ALLOC_FAILED (-200005) /* Memory allocation failed for model payload data. */

#define MC_ERROR_PROCESS_NULL_HANDLE              (-101001) /* MC_Process received a NULL handle. */
#define MC_ERROR_PROCESS_HANDLE_CORRUPTED         (-101002) /* MC_Process detected an invalid handle guard value. */
#define MC_ERROR_PROCESS_NULL_IMAGE               (-101003) /* MC_Process received a NULL input or output image. */
#define MC_ERROR_PROCESS_CPU_FUNC_MISSING         (-101004) /* CPU processing function pointers are not initialized. */
#define MC_ERROR_PROCESS_TEXTURE_TYPE_INVALID     (-101005) /* Texture input or output pointer is invalid. */
#define MC_ERROR_PROCESS_TEXTURE_TYPE_CONFLICT    (-101006) /* Texture type conflicts with the configured input type. */
#define MC_ERROR_PROCESS_GPU_SR_FAILED            (-101007) /* GPU super-resolution stage failed. */
#define MC_ERROR_PROCESS_GPU_RESIZE_FAILED        (-101008) /* GPU resize/post-filter stage failed. */
#define MC_ERROR_PROCESS_METALFX_FAILED           (-101009) /* MetalFX processing stage failed. */

#define MC_ERROR_CONTROL_NULL_HANDLE              (-102001) /* MC_Control received a NULL handle. */
#define MC_ERROR_CONTROL_HANDLE_CORRUPTED         (-102002) /* MC_Control detected an invalid handle guard value. */
#define MC_ERROR_CONTROL_CMD_OUT_OF_RANGE         (-102003) /* Control command is outside the supported range. */
#define MC_ERROR_CONTROL_NULL_PARAMS              (-102004) /* SET_PARAM command received a NULL control_param_t pointer. */
#define MC_ERROR_CONTROL_WIDTH_OUT_OF_RANGE       (-102005) /* Control width is outside the supported range. */
#define MC_ERROR_CONTROL_HEIGHT_OUT_OF_RANGE      (-102006) /* Control height is outside the supported range. */
#define MC_ERROR_CONTROL_SCALER_OUT_OF_RANGE      (-102007) /* Control scaler factor is outside the supported range. */
#define MC_ERROR_CONTROL_ALG_MODE_OUT_OF_RANGE    (-102008) /* Control algorithm mode is invalid. */
#define MC_ERROR_CONTROL_MODEL_PATH_TOO_LONG      (-102009) /* Control model path exceeds the supported length. */
#define MC_ERROR_CONTROL_INIT_FUNCS_FAILED        (-102010) /* Processing function re-assignment failed during control. */
#define MC_ERROR_CONTROL_REINIT_FAILED            (-102011) /* Full reinitialization failed during control. */
#define MC_ERROR_CONTROL_NULL_OUTPUT              (-102012) /* QUERY_STATUS command received a NULL output pointer. */
#define MC_ERROR_MEMORY_CONTROL_ALLOC_FAILED      (-202001) /* Memory allocation failed during control. */

#define MC_ERROR_UNINIT_NULL_HANDLE               (-103001) /* MC_Uninit received a NULL handle. */
#define MC_ERROR_UNINIT_HANDLE_CORRUPTED          (-103002) /* MC_Uninit detected an invalid handle guard value. */
#define MC_ERROR_MEMORY_UNINIT_DOUBLE_FREE        (-203001) /* Memory release count indicates a double free. */

#define MC_ERROR_REPORT_INVALID_PARAM                (-104001) /* Report packet arguments are invalid. */
#define MC_ERROR_REPORT_INVALID_SERVER_IP            (-104002) /* Report server IP address is invalid. */
#define MC_ERROR_REPORT_SOCKET_ENV_INIT_FAILED       (-104003) /* Socket environment initialization failed. */
#define MC_ERROR_REPORT_SOCKET_CREATE_FAILED         (-104004) /* Report socket creation failed. */
#define MC_ERROR_REPORT_SOCKET_REUSEADDR_FAILED      (-104005) /* Setting SO_REUSEADDR on the report socket failed. */
#define MC_ERROR_REPORT_SOCKET_SEND_TIMEOUT_FAILED   (-104006) /* Setting report socket send timeout failed. */
#define MC_ERROR_REPORT_SOCKET_RECV_TIMEOUT_FAILED   (-104007) /* Setting report socket receive timeout failed. */
#define MC_ERROR_REPORT_CONNECT_FAILED               (-104008) /* Connecting to the report server failed. */
#define MC_ERROR_REPORT_SEND_FAILED                  (-104009) /* Sending the report packet failed. */
#define MC_ERROR_REPORT_PARTIAL_SEND                 (-104010) /* Report packet was only partially sent. */

#define MC_WARNING_MEMORY_LEAK_ON_FREE            (100001) /* Memory leak detected while freeing an internal handle. */
#define MC_WARNING_MEMORY_LEAK_ON_UNINIT          (100002) /* Memory leak detected while uninitializing the API handle. */

/**
 * @brief Input parameter structure for algorithm initialization and configuration
 * @details Contains all input parameters required for MC algorithm initialization,
 * including image data, model path, and algorithm runtime settings
 */
typedef struct input_param_t {
    input_type_e input_type; //0 = buffer, 1 = r8_texture
    char model_path[256];          // File path of the pre-trained model (max 255 characters + null terminator)
    unsigned int width;            // Width of the input image (pixel units), valid range: [64, 4032]
    unsigned int height;           // Height of the input image (pixel units), valid range: [64, 4032]
    float scaler_factor;           // Requested super-resolution scaling factor, valid range: [1, 8]. x86/neon accept implemented integer scales only.
    alg_mode_e alg_mode;         // Algorithm runtime mode. 0 = high speed mode, 1 = speed mode.
    unsigned int num_threads;      // Number of CPU threads for parallel computation, valid range: [1, 8]
                                   // Note: Multi-threading takes effect only when the image height >= 256 lines
    log_level_e log_level;
    magic_backend_e backend;       // Runtime backend selector: x86/neon/metal/opengl/opengles/vulkan.
} input_param_t;

typedef struct control_param_t {
    unsigned int width;            // Width of the input image (pixel units), valid range: [64, 4032]
    unsigned int height;           // Height of the input image (pixel units), valid range: [64, 4032]
    float scaler_factor;           // Requested super-resolution scaling factor, valid range: [1, 8]. x86/neon accept implemented integer scales only.
    alg_mode_e alg_mode;         // Algorithm runtime mode. 0 = high speed mode, 1 = speed mode.
    char model_path[256];          // File path of the pre-trained model (max 255 characters + null terminator)
} control_param_t;

/**
 * @brief Output status parameter structure for algorithm query
 * @details Stores the returned status and parameters when querying the algorithm,
 * including input/output image dimensions, runtime settings, and error code
 */
typedef struct output_status_params_t {
    unsigned int width;            // Width of the original input image (pixel units)
    unsigned int height;           // Height of the original input image (pixel units)
    unsigned int output_width;     // Width of the super-resolved output image (pixel units)
    unsigned int output_height;    // Height of the super-resolved output image (pixel units)
    float scaler_factor;           // Current requested super-resolution scaling factor in use
    alg_mode_e alg_mode;         // Current algorithm runtime mode 
    input_type_e input_type;
    magic_backend_e backend;
    unsigned int num_threads;      // Current number of CPU threads in use
    double gpu_time;
    unsigned int error_code;       // Algorithm error code: 0 = No error, non-zero = specific error (refer to error code specification)
} output_status_params_t;

/**
 * @brief Initialize the MC super-resolution algorithm and create a handle
 * @param param Pointer to the input parameter structure (input_param_t), contains initialization configuration
 * @return void* - Algorithm handle (opaque pointer) for subsequent API calls; NULL = Initialization failed (invalid params/model not found/etc.)
 * @note The handle must be retained for other API functions and released via MC_Uninit()
 */
void* MC_Init(input_param_t* param);

/**
 * @brief Perform super-resolution processing on the input image
 * @param handle Valid algorithm handle created by MC_Init()
 * @param image_in Pointer to the input image data buffer (raw pixel data)
 * @return unsigned char* - Pointer to the super-resolved output image data buffer; NULL = Processing failed
 * @note The output buffer is allocated internally by the algorithm. Copy the data immediately after retrieval,
 * as the buffer may be reclaimed or overwritten in subsequent calls
 */
int MC_Process(void* handle, void *image_in, void *image_out);

/**
 * @brief Control interface for setting parameters or querying status
 * @param handle Valid algorithm handle created by MC_Init()
 * @param cmd Command type (cmd_params_e): SET_PARAM for setting, QUERY_STATUS for querying
 * @param ctrl Pointer to input parameters (used only when cmd = SET_PARAM; NULL for QUERY_STATUS)
 * @param output Pointer to output status structure (used only when cmd = QUERY_STATUS)
 * @return int - 0 = Operation succeeded; -1 = Operation failed (invalid handle/cmd/params)
 */
int MC_Control(void* handle, cmd_params_e cmd, control_param_t* ctrl, output_status_params_t* output);

/**
 * @brief Release all resources allocated by the MC algorithm
 * @param handle Algorithm handle created by MC_Init() (NULL is allowed, no operation performed)
 * @return int - 0 = Resource release succeeded; -1 = Release failed (invalid handle/residual resources)
 * @note After calling this function, the handle becomes invalid and cannot be used in other APIs
 */
int MC_Uninit(void* handle);

/**
 * @brief Get the version string of the MC algorithm library
 * @return char* - Pointer to the null-terminated version string (e.g., "v1.2.0"); never returns NULL
 * @note The version string is a static constant, do not free the pointer
 */
char *MC_GetVersion(void);

#ifdef __cplusplus
}
#endif
#endif
