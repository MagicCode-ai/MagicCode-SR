#include <stdlib.h>
#include <stdio.h>

#include "../include/mc_interface.h"
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

#define VERYFAST_SR 0x00
#define FAST_SR 0x01
#define MAGIC_SR 0x10

#define SCALER_FACTOR (3)
#define MAGIC_SR_MODE FAST_SR
#define NUM_THREADS (4)
#define UT_TEST (0)
#define UT_TEST_USED_VIDEO (1)
#define SPEED_TEST (0)
#define CALC_PSNR (1)
#define OUTPUT_SR_IMAGE (0)

#define CHECK_RLSP_CORRECTNESS (0)

#define MODEL_ROOT_PATH "../model/"
#if MAGIC_SR_MODE==VERYFAST_SR
#if SCALER_FACTOR==2
#define MODEL_PATH MODEL_ROOT_PATH"MagicCode_SR_veryfastx2_params.bin"
#elif SCALER_FACTOR==3
#define MODEL_PATH MODEL_ROOT_PATH"MagicCode_SR_veryfastx3_params.bin"
#elif SCALER_FACTOR==4
#define MODEL_PATH MODEL_ROOT_PATH"MagicCode_SR_veryfastx4_params.bin"
#endif
#elif MAGIC_SR_MODE==FAST_SR
#if SCALER_FACTOR==2
#define MODEL_PATH MODEL_ROOT_PATH"MagicCode_SR_fastx2_params.bin"
#elif SCALER_FACTOR==3
#define MODEL_PATH MODEL_ROOT_PATH"MagicCode_SR_fastx3_params.bin"
#elif SCALER_FACTOR==4
#define MODEL_PATH MODEL_ROOT_PATH"MagicCode_SR_fastx4_params.bin"
#endif
#endif

#define HR_PATH  "../Set5/HR/"
#if SCALER_FACTOR==2
#define LR_PATH  "../Set5/LR/"
#elif SCALER_FACTOR==3
#define LR_PATH "../Set5/LRx3/"
#elif SCALER_FACTOR==4
#define LR_PATH "../Set5/LRx4/"
#endif
#define OUT_PATH "./out/"
#define IMG_NAME ""

#define SUFFIX ".png"


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

static float RGBYUV0_2990[256], RGBYUV0_5870[256], RGBYUV0_1140[256];
static float RGBYUV0_1684[256], RGBYUV0_3316[256];
static float RGBYUV0_4187[256], RGBYUV0_0813[256];

void InitLookupTable()
{
    int32_t i;

    for (i = 0; i < 256; i++) RGBYUV0_2990[i] = (float)0.2990 * i;
    for (i = 0; i < 256; i++) RGBYUV0_5870[i] = (float)0.5870 * i;
    for (i = 0; i < 256; i++) RGBYUV0_1140[i] = (float)0.1140 * i;
    for (i = 0; i < 256; i++) RGBYUV0_1684[i] = (float)0.1684 * i;
    for (i = 0; i < 256; i++) RGBYUV0_3316[i] = (float)0.3316 * i;
    for (i = 0; i < 256; i++) RGBYUV0_4187[i] = (float)0.4187 * i;
    for (i = 0; i < 256; i++) RGBYUV0_0813[i] = (float)0.0813 * i;
}

int32_t rgb_to_yuv420(void* bmp, int32_t stride, int32_t width, int32_t height, void* yuv[3])
{
    static int32_t init_done = 0;
    int32_t i, j, size;

    void* y_out, *u_out, *v_out;
    unsigned char* rp, * gp, * bp;
    unsigned char* yp, * up, * vp;
    unsigned char* pu1, * pu2, * pv1, * pv2, * psu, * psv;
    unsigned char* sub_u_buf, * sub_v_buf;
    unsigned char* y_buffer, * u_buffer, * v_buffer;

    if (init_done == 0)
    {
        InitLookupTable();
        init_done = 1;
    }

    y_out = yuv[0];
    u_out = yuv[1];
    v_out = yuv[2];

    size = width * height;
    bp = (unsigned char*)bmp;
    yp = (unsigned char*)y_out;
    sub_u_buf = (unsigned char*)u_out;
    sub_v_buf = (unsigned char*)v_out;
    up = (unsigned char*)malloc(size * sizeof(unsigned char));
    vp = (unsigned char*)malloc(size * sizeof(unsigned char));
    if (up == NULL || vp == NULL)
    {
        printf("wrong up || vp\n");
        exit(1);
    }

    y_buffer = yp;
    u_buffer = up;
    v_buffer = vp;

    for (j = 0; j < height; j++)//to change
    {
        bp = (unsigned char*)bmp+j*stride;
        for (i = 0; i < width; i++)
        {
            gp = bp + 1;
            rp = bp + 2;
            *yp = (unsigned char)(RGBYUV0_2990[*rp] + RGBYUV0_5870[*gp] + RGBYUV0_1140[*bp]);
            *up = (unsigned char)(-RGBYUV0_1684[*rp] - RGBYUV0_3316[*gp] + (*bp) / 2 + 128);
            *vp = (unsigned char)((*rp) / 2 - RGBYUV0_4187[*gp] - RGBYUV0_0813[*bp] + 128);
            bp += 3;
            yp++;
            up++;
            vp++;
        }
    }

    for (j = 0; j < height; j++)//to upside down
    {
        for (i = 0; i < width; i++)
        {
            yp = y_buffer + (height - j - 1) * width;
            up = u_buffer + (height - j - 1) * width;
            vp = v_buffer + (height - j - 1) * width;
        }
    }

    for (j = 0; j < height / 2; j++)//to down sample
    {
        psu = sub_u_buf + j * width / 2;
        psv = sub_v_buf + j * width / 2;
        pu1 = up + 2 * j * width;
        pu2 = up + (2 * j + 1) * width;
        pv1 = vp + 2 * j * width;
        pv2 = vp + (2 * j + 1) * width;
        for (i = 0; i < width / 2; i++)
        {
            *psu = (*pu1 + *(pu1 + 1) + *pu2 + *(pu2 + 1)) / 4;
            *psv = (*pv1 + *(pv1 + 1) + *pv2 + *(pv2 + 1)) / 4;

            psu++;  psv++;
            pu1 += 2;  pu2 += 2;
            pv1 += 2;  pv2 += 2;
        }
    }

    free(up);
    free(vp);

    return 0;
}

int32_t example()
{
    int32_t start_pos = 0;
    int32_t end_pos = 5;
    char count[16];
    long i = 10000000L;
    
    double dff;
    __int64_t  c1, c2;
    
    //time_t timeBegin, timeEnd;
    //timeBegin = time(NULL);
    double duration;
    
    FILE* f_yuv = fopen("out.yuv", "w+");
    if (f_yuv == NULL)
        return -1;
    
    //_itoa(start_pos, count, 10);
    char* png_l = (char*)malloc(strlen(LR_PATH) + sizeof(count) + strlen(IMG_NAME) + 1);
    if (png_l == NULL)
        return -1;
    char* png_out = (char*)malloc(strlen(OUT_PATH) + sizeof(count) + strlen(IMG_NAME) + 1);
    if (png_out == NULL)
        return -1;
    char* png_h = (char*)malloc(strlen(HR_PATH) + sizeof(count) + strlen(IMG_NAME) + 1);
    if (png_h == NULL)
        return -1;
    
    double all_psnr = 0.0, all_psnr_scale = 0.0;
    double magic_sr_ms = 0.0, resize_ms = 0.0;
    int32_t frame_num = 0;
    for (int32_t i = start_pos; i < end_pos; i++)
    {
        //_itoa(i, count, 10);
        snprintf(count, 8, "%05d", i);
        get_img_path(png_l, LR_PATH, IMG_NAME, count, SUFFIX);
        get_img_path(png_h, HR_PATH, IMG_NAME, count, SUFFIX);
        
        int32_t src_width = 0;
        int32_t src_height = 0;
        int32_t dst_width = 0;
        int32_t dst_height = 0;
        int32_t n = 0;
        
        uint8_t *rgb = stbi_load(png_l, &src_width, &src_height, &n, 0);
        
        void* out_yuv[3], * gt_yuv[3];
        out_yuv[0] = (uint8_t*)malloc(src_width * src_height * sizeof(uint8_t));
        out_yuv[1] = (uint8_t*)malloc((src_width * src_height >> 2) * sizeof(uint8_t));
        out_yuv[2] = (uint8_t*)malloc((src_width * src_height >> 2) * sizeof(uint8_t));
        
        uint8_t* out1 = (uint8_t*)malloc(src_width * SCALER_FACTOR * src_height * SCALER_FACTOR * sizeof(uint8_t));
        uint8_t* out2 = (uint8_t*)malloc(src_width * SCALER_FACTOR * src_height * SCALER_FACTOR * sizeof(uint8_t));
        input_param_t param;
        param.height = src_height;
        param.width = src_width;
        param.scaler_factor = SCALER_FACTOR;
        param.image = NULL;
        strcpy(param.model_path, MODEL_PATH);
        param.num_threads = NUM_THREADS;
        
        void* rlsp = NULL;
        if (MAGIC_SR_MODE == FAST_SR)
            param.alg_mode = FAST_SR;
        else if (MAGIC_SR_MODE == VERYFAST_SR)
            param.alg_mode = VERYFAST_SR;
        
        rlsp = MC_Init(&param);
        
        if (rlsp == NULL)
            return -5;
        
        uint8_t* in = NULL;
        uint8_t* out = NULL;
        
        rgb_to_yuv420(rgb, src_width*n, src_width, src_height, out_yuv);
        
        param.image = out_yuv[0];
        
        int32_t ret = 0;
        double bilinear_ms = 0.0;
        
        int32_t nums = 1;
#if SPEED_TEST
        nums = 20000;
#endif
        struct timeval begin, end;
        gettimeofday(&begin, 0);
        unsigned char *out_sr = NULL;
        for (int32_t c = 0; c < nums; c++)
        {
            out_sr = MC_Process(rlsp, param.image);
        }
        gettimeofday(&end, 0);
        long seconds = end.tv_sec - begin.tv_sec;
        long microseconds = end.tv_usec - begin.tv_usec;
        double ms = seconds*1000 + (double)microseconds/1000.0;
        printf("\nMagic scaler Time: %lfms.\n",ms/nums);
        double sr_ms = ms;
        
        
        frame_num++;
        get_img_path(png_h, HR_PATH, IMG_NAME, count, SUFFIX);
        uint8_t *gt_rgb = stbi_load(png_h, &dst_width, &dst_height, &n, 0);
        gt_yuv[0] = (uint8_t*)malloc(dst_width * dst_height * sizeof(uint8_t));
        gt_yuv[1] = (uint8_t*)malloc((dst_width * dst_height >> 2) * sizeof(uint8_t));
        gt_yuv[2] = (uint8_t*)malloc((dst_width * dst_height >> 2) * sizeof(uint8_t));
        
        rgb_to_yuv420(gt_rgb, dst_width*n, dst_width, dst_height, gt_yuv);
        
        output_status_params_t out_params;
        ret = MC_Control(rlsp, QUERY_STATUS,&param,&out_params);
        if(ret < 0)
        {
            printf("ERROR: QUERY_STATUS is fail(%d).\n",ret);
            return -2;
        }
        memcpy(out2, out_sr, out_params.output_width*out_params.output_height);
        
        double psnr = 0.0, psnr_scale = 0.0;
        psnr = calc_psnr(out2, gt_yuv[0], out_params.output_width, out_params.output_height, dst_width, dst_height);

        printf("%s processed, MagicCode SR psnr = %2.2f. \n", png_l, psnr);

        all_psnr += psnr;
        all_psnr_scale += psnr_scale;

        resize_ms += bilinear_ms;
        magic_sr_ms += sr_ms;
        
        MC_Uninit(rlsp);
    }

    int32_t total = end_pos - start_pos;
    all_psnr = all_psnr / total;
    magic_sr_ms = magic_sr_ms / total;

    printf("\ntotal %d pictures, Psnr: Magic_sr = %2.2f, Time: Magic_sr = %4.4fms\n",
        total, all_psnr, magic_sr_ms);

    return 0;
}


int32_t main() {
    
    example();

	system("pause");
	return 0;
}
