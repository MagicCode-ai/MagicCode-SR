/************************************************************************************
 * Copyright (C) 2024-2025 MagicCode Technology Co., Ltd. All rights reserved.
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
 * @date        2025-12-01
 * @version     V1.0.0
 * @copyright   Copyright (C) 2024-2025 MagicCode Technology Co., Ltd. All rights reserved.
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
 * @param input Pointer to input parameters (used only when cmd = SET_PARAM; NULL for QUERY_STATUS)
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
