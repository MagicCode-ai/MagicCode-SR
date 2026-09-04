#include <stdlib.h>
#include <stdio.h>

#include "mc_interface.h"
#include <stdint.h>
#include "metal.h"
#include "img_process.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <string.h>
#include <math.h>
#if defined(__GNUC__) // && (defined(__i386__) || defined(__x86_64__))
#include <sys/time.h>
#else
#include <time.h>
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define SCALER_FACTOR (2)
#define NUM_THREADS (4)
#define UT_TEST (0)
#define UT_TEST_USED_VIDEO (1)
#define SPEED_TEST (0)
#define CALC_PSNR (1)
#define OUTPUT_SR_IMAGE (0)

#define INPUT_YUV444 (0)
#define INPUT_TEXTURE (1)

#define CHECK_RLSP_CORRECTNESS (0)

#define MODEL_ROOT_PATH "../../model/"
#define MODEL_PATH MODEL_ROOT_PATH"magic_sr_cpu_params.bin"

#if CHECK_RLSP_CORRECTNESS

#define HR_PATH "/work/05.sequence/dataset/Set5/HR/"
#if SCALER_FACTOR==2
#define LR_PATH "/work/05.sequence/dataset/Set5/LR/"
#elif SCALER_FACTOR==3
#define LR_PATH "/work/05.sequence/dataset/Set5/LRx3/"
#elif SCALER_FACTOR==4
#define LR_PATH "/work/05.sequence/dataset/Set5/LRx4/"
#endif
#define OUT_PATH "./out/"
#define IMG_NAME ""  //"" //"Johnny-"
#else
#define HR_PATH  "/work/05.sequence/dataset/Set5/HR/"//"C:\\hanqr\\01.work\\04.seq\\dataset\\rlsp\\val\\HR\\014\\"
#if SCALER_FACTOR==2
#define LR_PATH  "/work/05.sequence/dataset/Set5/LR/"//"C:\\hanqr\\01.work\\04.seq\\dataset\\rlsp\\val\\LR\\014\\"
#elif SCALER_FACTOR==3
#define LR_PATH "/work/05.sequence/dataset/Set5/LRx3/"//"C:\\hanqr\\01.work\\04.seq\\dataset\\rlsp\\val\\LRx3\\014\\"
#elif SCALER_FACTOR==4
#define LR_PATH "/work/05.sequence/dataset/Set5/LRx4/"//"C:\\hanqr\\01.work\\04.seq\\dataset\\rlsp\\val\\LRx4\\014\\"
#endif
#define OUT_PATH "./out/"
#define IMG_NAME ""  //"" //"Johnny-"
#endif
#define SUFFIX ".png"

#define __DEBUG (0)

enum {
    MC_MTL_RGBA8UNORM = 70u, /* MTLPixelFormatRGBA8Unorm */
    MC_MTL_R8UNORM = 10u     /* MTLPixelFormatR8Unorm */
};

/* Same as product src/magic_backend.h scaled_dimension. */
static uint32_t scaled_dimension(uint32_t value, float scaler)
{
    double scaled = (double)value * (double)scaler;
    if (scaled < 1.0)
        return 1;
    if (scaled > (double)UINT32_MAX)
        return UINT32_MAX;
    return (uint32_t)floor(scaled + 0.5);
}

static int mc_wrap_process(void **handle, void *in, void *out, input_type_e input_type, magic_backend_e backend)
{
    magic_frame_t f;
    memset(&f, 0, sizeof(f));
    if (backend == MAGIC_BACKEND_METAL && input_type != INPUT_BUFFER && input_type != INPUT_BUFFER_R8) {
        f.image_in.handle.pointer = in;
        f.image_out.handle.pointer = out;
        if (input_type == INPUT_TEXTURE_R8Unorm) {
            f.image_in.format = MC_MTL_R8UNORM;
            f.image_out.format = MC_MTL_R8UNORM;
        } else {
            f.image_in.format = MC_MTL_RGBA8UNORM;
            f.image_out.format = MC_MTL_RGBA8UNORM;
        }
        f.image_in.mip_count = 1;
        f.image_out.mip_count = 1;
    } else if (backend == MAGIC_BACKEND_OPENGLES || backend == MAGIC_BACKEND_OPENGL) {
        f.image_in.handle.gl_texture = (uint32_t)(uintptr_t)in;
        f.image_out.handle.gl_texture = (uint32_t)(uintptr_t)out;
        f.image_in.format = 0x8058; /* GL_RGBA8 */
        f.image_out.format = 0x8058;
        f.image_in.target = 0x0DE1; /* GL_TEXTURE_2D */
        f.image_out.target = 0x0DE1;
        f.image_in.mip_count = 1;
        f.image_out.mip_count = 1;
    } else {
        f.image_in.handle.pointer = in;
        f.image_out.handle.pointer = out;
    }
    f.frame = NULL;
    f.command_buffer = NULL;
    return MC_Enable(handle, &f, NULL, NULL);
}

/* Re-create the handle when mutable (or immutable) init fields change. */
static int mc_apply_param(void **handle, input_param_t *param)
{
    if (handle && *handle) {
        MC_Disable(*handle);
        *handle = NULL;
    }
    return MC_Enable(handle, NULL, param, NULL);
}

typedef struct ut_test_t
{
    input_type_e input_type[3];
    char* imag_path[4];
    char* model_path[6];
    char* gpu_model_path[2];
    alg_mode_e alg_mode[2];
    float scaler_factor[3];
    float psnr[64];
}ut_test_t;

ut_test_t uttest = {
    {INPUT_BUFFER, INPUT_TEXTURE_RGB8Unorm, INPUT_TEXTURE_R8Unorm},
    {HR_PATH,"/work/05.sequence/dataset/Set5/LR/",
    "/work/05.sequence/dataset/Set5/LRx3/","/work/05.sequence/dataset/Set5/LRx4/"},
    { MODEL_ROOT_PATH"magic_sr_cpu_params.bin", MODEL_ROOT_PATH"magic_sr_cpu_params.bin", MODEL_ROOT_PATH"magic_sr_cpu_params.bin",
      MODEL_ROOT_PATH"magic_sr_cpu_params.bin", MODEL_ROOT_PATH"magic_sr_cpu_params.bin", MODEL_ROOT_PATH"magic_sr_cpu_params.bin"},
    { MODEL_ROOT_PATH"magic_sr_gpu_params.bin", MODEL_ROOT_PATH"magic_sr_gpu_params.bin"},
    {SPATIAL_SPEED_MODE, SPATIAL_BALANCED_MODE},
    {1.5, 2, 3},
    {
        35.97,35.93,27.26,33.68,31.76,
        27.57,29.57,22.10,30.82,23.81/*,27.04,29.35,21.76,30.60,23.30 */ ,
        28.55,30.08,20.72,30.36,24.87,
        /*36.39,36.00,27.90,33.52,32.11,*/
        30.73,31.26,24.85,31.37,27.66,
        32.10,27.64,21.80,30.49,25.97,
        29.65,31.04,21.95,30.70,25.97
    }
};

static inline double calc_psnr(uint8_t *img1, uint8_t *img2, const int32_t width1, const int32_t height1, const int32_t width2, const int32_t height2)
{
    __int64_t sse = 0;
    for (int32_t i = 0; i < height1; i++)
    {
        for (int32_t j = 0; j < width1; j++)
        {
            int32_t diff = (img1[i * width1 + j] - img2[i * width2 + j]);
            sse += (diff*diff);
        }

    }
    double mse = (float)sse / (width1 * height1);
    double ret = log10((double)(255*255)/mse);
    //printf("sse = %lld, mse = %2.2f, ret = %2.2f \n",sse, mse, ret);
    return 10.0*ret;
}

int32_t get_img_path(char *dst, char *dir, char *name, char *count, char *suffix)
{
    strcpy(dst, dir);
    strcat(dst, name);
    strcat(dst, count);
    strcat(dst, suffix);

    return 0;
}

int32_t example()
{
    double dff;
    __int64_t  c1, c2;


    //time_t timeBegin, timeEnd;
    //timeBegin = time(NULL);
    double duration;

    float video_scale[3] = {1.5, 2,3 };
    char* video_gt_path[3] = {"/work/05.sequence/540p/xiaoxiongmao_1024x540.yuv",
                              "/work/05.sequence/1080p/xiaoxiongmao_2048x1080.yuv",
                              "/work/05.sequence/4k/IMG_gt_3840x2160.yuv" };
    
    char* video_path[3][3] = {{"/work/05.sequence/540p/xiaoxiongmao_682x360.yuv",
                                "/work/05.sequence/540p/xiaoxiongmao_512x270.yuv",
                                "/work/05.sequence/540p/xiaoxiongmao_512x270.yuv" },
                                {"/work/05.sequence/540p/xiaoxiongmao_1024x540.yuv",
                               "/work/05.sequence/540p/xiaoxiongmao_682x360.yuv",
                               "/work/05.sequence/540p/xiaoxiongmao_512x270.yuv" },
                             {"/work/05.sequence/4k/IMG_1920x1080.yuv",
                              "/work/05.sequence/4k/IMG_1280x720.yuv",
                              "/work/05.sequence/4k/IMG_960x540.yuv" }};

    int video_gt_width[3] = {1024, 2048, 3840};
    int video_gt_height[3] = {540, 1080, 2160};
    
    printf("sr version: %s.\n", MC_GetVersion());
    FILE* fp_gt = NULL;
    for (int v = 0; v < 2; v++)
    {
        for (int t = 1; t < 2; t++) //0 = buffer, 1 = texture
        {
            //  fseek(fp_gt,SEEK_SET,0);
#if OUTPUT_SR_IMAGE
            FILE* fp_sr_out = fopen("/Users/joey/Desktop/work/05.sequence/1080p/sr_out.yuv", "wb");
            if (fp_sr_out == NULL)
            {
                printf("fp_sr_out == NULL.\n");
                return -1;
            }
#endif
            
            int gt_pic_size = video_gt_width[v]*video_gt_height[v]*3/2;
            uint8_t* gt_yuv = (uint8_t*)malloc(gt_pic_size * sizeof(uint8_t));
            uint8_t* gt_uv = (uint8_t*)malloc((video_gt_width[v]*video_gt_height[v] / 2) * sizeof(uint8_t));
            
            int max_thread_nums = 8;
            if(uttest.input_type[t] > 0)
                max_thread_nums = 1;
            for (int thread_nums = 0; thread_nums < max_thread_nums; thread_nums++)
            {
                input_param_t param;
                memset(&param, 0, sizeof(param));
                param.struct_size = (uint32_t)sizeof(param);
                param.height = video_gt_height[v]/video_scale[0];
                param.width = video_gt_width[v]/video_scale[0];
                param.scaler_factor = uttest.scaler_factor[0];
                param.alg_mode = uttest.alg_mode[0];
                param.input_type = uttest.input_type[t];
                param.log_level = MAGIC_LOG_INFO;
                param.backend = MAGIC_BACKEND_NEON;
                param.spatial_sharpen_level = 3;
                if(param.input_type > 0)
                {
                    param.backend = MAGIC_BACKEND_METAL;
                }
                
                if(param.input_type == INPUT_BUFFER)
                    strcpy(param.model_path, uttest.model_path[3*param.alg_mode]);
                else
                    strcpy(param.model_path, uttest.gpu_model_path[(uint32_t)param.alg_mode]);
                
                void* handle = NULL;
                output_status_params_t init_st;
                memset(&init_st, 0, sizeof(init_st));
                int init_rc = MC_Enable(&handle, NULL, &param, &init_st);
                if (init_rc != 0 || handle == NULL)
                {
                    printf("ERR: MC_Enable init failed (%d).\n", init_rc);
                    return -5;
                }
                
                fp_gt = fopen(video_gt_path[v], "rb");
                if (fp_gt == NULL)
                {
                    printf("fp_gt == NULL.\n");
                    return -1;
                }
                
                for (int32_t y = 0; y < 2; y++)
                {
                    
                    for (int32_t x = 0; x < 2; x++)
                    {
                        FILE* fp = fopen(video_path[v][x], "rb");
                        if (fp == NULL)
                        {
                            printf("fp == NULL.\n");
                            return -1;
                        }
                        int src_width = video_gt_width[v]/video_scale[x];
                        int src_height = video_gt_height[v]/video_scale[x];
                        int pic_size = (src_width * src_height * 3 / 2);
                        unsigned char* yuv = (unsigned char*)malloc(pic_size * sizeof(unsigned char));
                        unsigned char* y_input = (unsigned char*)malloc(pic_size * sizeof(unsigned char));
                        if (y_input == NULL)
                        {
                            printf("yuv == NULL.\n");
                            return -2;
                        }
                        unsigned char* uv_input = (unsigned char*)malloc((src_width * src_height/2) * sizeof(unsigned char));
                        memset(uv_input, 0, (src_width * src_height / 2));
                        
                        int dst_width = src_width * uttest.scaler_factor[x];
                        int dst_height = src_height * uttest.scaler_factor[x];
                        
                        double sr_time_ms[8];
                        double sr_gpu_time_ms = 0.0;
                        double ffmpeg_ms = 0.0f;
                        double sr_psnr_t[8] = {0};
                        memset(sr_time_ms, 0, 8 * sizeof(double));
                        
                        uint8_t* out_y_sr = NULL;
                        void *rgba_texture_out = NULL;
                        int core_out_w = (int)scaled_dimension((uint32_t)src_width, uttest.scaler_factor[x]);
                        int core_out_h = (int)scaled_dimension((uint32_t)src_height, uttest.scaler_factor[x]);
                        if(param.input_type > INPUT_BUFFER)
                        {
                            rgba_texture_out = creat_texture2d(core_out_w, core_out_h,
                                                               param.input_type, 2);
                        }
                        else
                        {
                            out_y_sr = (uint8_t*)malloc((size_t)core_out_w * (size_t)core_out_h * sizeof(uint8_t));
                        }
                        
                        printf("\n-----loop: %d, input size: %dx%d, scaler_factor: %2.2f, sr_mode: %d, input_type: %d-----\n", v, src_width, src_height, uttest.scaler_factor[x], uttest.alg_mode[y], param.input_type);
                        
                        int frame_num = 0;
                        double sr_psnr = 0.0;
                        fseek(fp,SEEK_SET,0);
                        fseek(fp_gt,SEEK_SET,0);
                        
                        param.height = src_height;
                        param.width = src_width;
                        param.scaler_factor = uttest.scaler_factor[x];
                        param.alg_mode = uttest.alg_mode[y];
                        
                        if(param.input_type == INPUT_BUFFER)
                            strcpy(param.model_path, uttest.model_path[y*3+x]);
                        else
                            strcpy(param.model_path, uttest.gpu_model_path[(uint32_t)param.alg_mode]);
#if 0//OUTPUT_SR_IMAGE
                        fwrite(yuv, 1, src_width * src_height*3/2, fp_sr_out);
                        //memcpy(uv_input, &yuv[src_width*src_height], src_width * src_height / 2);
                        //fwrite(uv_input, 1, src_width * src_height / 2, fp_sr_out);
                        fflush(fp_sr_out);
#endif
                        int ret = mc_apply_param(&handle, &param);
                        if (ret < 0 || handle == NULL)
                        {
                            printf("ERR: MC_Enable reconfigure failure (%d).\n", ret);
                            return -6;
                        }
                        
                        while (1)
                        {
                            
                            int size = fread(gt_yuv, 1, gt_pic_size, fp_gt);
#if 0
                            image_scale(y_input, gt_yuv, video_gt_width, video_gt_height,video_scale[x]);
#else
                            size = fread(yuv, 1, pic_size, fp);
                            if (size < pic_size)
                                break;
                           // if(frame_num >=400)
                            //    break;
                            
                            unsigned char *rgba_texture = NULL;
                            if(param.input_type > INPUT_BUFFER)
                            {
                                memcpy(y_input, yuv, pic_size*sizeof(char));
                                unsigned char *yuv444 = yuv420_to_yuv444(y_input, src_width, src_height);
                                unsigned char *rgba = yuv444_to_rgba(yuv444, src_width, src_height);
                                rgba_texture = (unsigned char *)gen_rgba_texture(rgba, param.input_type, src_width, src_height);
                                
#if 0
                                // Download RGBA texture to CPU
                                unsigned char* downloadedRGB = download_rgba_texture((void *)rgba_texture, src_width, src_height);
                                
                                FILE* fp = fopen("rgba-10.rgb", "wb");
                                fwrite(downloadedRGB, sizeof(char), src_width*src_height*4, fp);
                                fclose(fp);
#endif
                                
                                free(yuv444);
                                free(rgba);
                            }
                            else
                            {
                                memcpy(y_input, yuv, src_width*src_height*sizeof(char));
                                
                                rgba_texture = y_input;
                                rgba_texture_out = out_y_sr;
                            }
                            
#endif
                            struct timeval begin, end;
                            gettimeofday(&begin, 0);
                            
                            int ret = mc_wrap_process(&handle, rgba_texture, rgba_texture_out, param.input_type, param.backend);
                            if (ret != 0) {
                                printf("ERR: MC_Enable process failure (%d).\n", ret);
                            }
                            //image_scale(out_y_sr, param.image, video_gt_width, video_gt_height,0.5);//video_scale[x]);
                            
                            gettimeofday(&end, 0);
                            long seconds = end.tv_sec - begin.tv_sec;
                            long microseconds = end.tv_usec - begin.tv_usec;
                            double ms = seconds*1000 + (double)microseconds/1000.0;
                            //printf("\nMagic scaler Time: %lfms.\n",ms);
                            double sr_ms = ms;
                            
                            output_status_params_t out_param;
                            memset(&out_param, 0, sizeof(out_param));
                            MC_Enable(&handle, NULL, NULL, &out_param);
                            
                            sr_time_ms[thread_nums] += ms;
                            sr_gpu_time_ms = out_param.gpu_time;
                            
                            if(param.input_type > INPUT_BUFFER)
                            {
                                // Download RGBA texture to CPU
                                // unsigned char* downloadedRGB = download_rgba_texture(rgba_texture_out, out_param.output_width, out_param.output_height);
                                unsigned char* downloadedRGB = down_private_texture_to_cpu((void *)rgba_texture_out);
                                out_y_sr = rgba_to_yuv420(downloadedRGB, out_param.output_width, out_param.output_height);
#if 0
                                FILE* fp_1 = fopen("rgba-out.rgb", "wb");
                                fwrite(downloadedRGB, sizeof(char), out_param.output_width*out_param.output_height*4, fp_1);
                                fclose(fp_1);
                                FILE* fp = fopen("yuv420-out.yuv", "wb");
                                int out_size = fwrite(out_y_sr, sizeof(char), out_param.output_width* out_param.output_height*3/2, fp);
                                if(out_size != out_param.output_width* out_param.output_height*3/2)
                                {
                                    printf("out yuv420 is fail.\n");
                                }
                                fclose(fp);
#endif
                                
                                // memcpy(out_y_sr, yuv420_out, out_param.output_width*out_param.output_height);
                                release_texture(rgba_texture);
                                //release_texture(rgba_texture_out);
                                free(downloadedRGB);
                            }
                            
                            
                            double ffmpeg_scaler_ms = 0.0;
                            double psnr = 0.0, psnr_scale = 0.0;
                            //  fread(gt_yuv, 1, gt_pic_size, fp_gt);
                            psnr = calc_psnr(out_y_sr, gt_yuv, out_param.output_width, out_param.output_height, video_gt_width[v], video_gt_height[v]);
                            
                            sr_psnr += psnr;
                            frame_num++;
                            
#if OUTPUT_SR_IMAGE
                            fwrite(out_y_sr, 1, out_param.output_width* out_param.output_height, fp_sr_out);
                            memcpy(gt_uv, &gt_yuv[dst_width * dst_height], dst_width* dst_height / 2);
                            fwrite(gt_uv, 1, dst_width * dst_height/2, fp_sr_out);
                            fflush(fp_sr_out);
#endif
                            if(param.input_type > INPUT_BUFFER)
                                free(out_y_sr);
                        }
                        
                        
                        sr_psnr /= frame_num;
                        sr_time_ms[thread_nums] /= frame_num;
                        sr_gpu_time_ms /= frame_num;
                        
                        if((param.input_type > 0 && param.scaler_factor > 2 && sr_psnr < 15.0)
                           ||(param.input_type > 0 && param.scaler_factor == 2 &&sr_psnr < 30.0)
                           ||(param.input_type == 0 && sr_psnr < 30.0))
                        {
                            printf("ERROR: psnr is too low(%4.4f), results is wrong\n", sr_psnr);
                        }

                        double frame_rate = (1.0f*1000.0)/(sr_time_ms[thread_nums]);
                        printf("Thread_nums: %d, total %d pictures, Psnr: %2.2fdb, Framerate: %2.2ffps, avg_time: %4.4fms, avg_gpu_time: %4.4fms\n",
                               thread_nums+1, frame_num, sr_psnr, frame_rate, sr_time_ms[thread_nums], sr_gpu_time_ms*1000);
                        
                        free(y_input);
                        free(uv_input);
                        fclose(fp);
                        
                        if(param.input_type > INPUT_BUFFER)
                            release_texture(rgba_texture_out);
                    }
                }
                
                MC_Disable(handle);
                handle = NULL;
            }
            
#if OUTPUT_SR_IMAGE
            fclose(fp_sr_out);
#endif
        }
    }

    fclose(fp_gt);
    
    return 0;
}


int32_t main() {
    
    example();

#ifdef _WIN32
	system("pause");
#endif
	return 0;
}
