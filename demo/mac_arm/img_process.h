#ifndef IMG_PROCESS_H
#define IMG_PROCESS_H
#include <stdint.h>
typedef struct mv_t
{
	int16_t mvx;
	int16_t mvy;
} mv_t;

int32_t image_warp(int16_t(*mv)[2], uint8_t* warped, const uint8_t* img, const int32_t width, const int32_t height, const int32_t stride);

int32_t rgb_to_yuv420(void* input, int32_t stride, int32_t width, int32_t height, void* yuv[3]);

int32_t rgb24_to_rgb888(uint8_t* src, const int32_t width, const int32_t height, uint8_t* dst[3]);

int32_t rgb888_to_rgb24(uint8_t* dst, const int32_t width, const int32_t height, uint8_t* src[3]);

int32_t image_scale(unsigned char *dst, unsigned char *src, int src_w, int src_h, float scale_factor);

int32_t bicubic_resize(unsigned char *dst,unsigned char *src, int src_w, int src_h,
                         int dst_w, int dst_h);

unsigned char* yuv444_to_rgba(const unsigned char* yuv444_data, int width, int height);

unsigned char* yuv420_to_yuv444(const unsigned char* yuv420_data, int width, int height);

unsigned char* yuv444_to_yuv420(const unsigned char* yuv444_data, int width, int height);

unsigned char* rgba_to_yuv444(const uint8_t* rgba_data, int width, int height);

unsigned char* rgba_to_yuv420(const uint8_t* rgba_data, int width, int height);


#endif
