#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "img_process.h"

#ifndef CLAMP
#define CLAMP(x, min_val, max_val) ((x) < (min_val) ? (min_val) : ((x) > (max_val) ? (max_val) : (x)))
#endif

// Bicubic interpolation basis function
static float cubic_hermite(float x)
{
    x = fabsf(x);
    if (x < 1.0f) {
        return (2.0f * x - 3.0f) * x * x + 1.0f;
    } else if (x < 2.0f) {
        return (-x + 5.0f) * x * x - 8.0f * x + 4.0f;
    }
    return 0.0f;
}

static void precompute_weights(float weights[256][4])
{
    for (int i = 0; i < 256; i++) {
        float x = (float)i / 256.0f;  // fractional part 0-1
        for (int m = -1; m <= 2; m++) {
            float t = fabsf(m - x);
            if (t < 1.0f) {
                weights[i][m + 1] = (2.0f * t - 3.0f) * t * t + 1.0f;
            } else if (t < 2.0f) {
                weights[i][m + 1] = (-t + 5.0f) * t * t - 8.0f * t + 4.0f;
            } else {
                weights[i][m + 1] = 0.0f;
            }
        }
    }
}

// Get bicubic-interpolated pixel from source image
static unsigned char get_bicubic_pixel(unsigned char *src, int src_w, int src_h,
                                      float x, float y)
{
    int x_int = (int)x;
    int y_int = (int)y;
    float dx = x - x_int;
    float dy = y - y_int;

    float weights[4][4];
    float sum_weights = 0.0f;

    // Compute weights
    for (int m = -1; m <= 2; m++) {
        for (int n = -1; n <= 2; n++) {
            weights[m + 1][n + 1] = cubic_hermite(m - dx) * cubic_hermite(n - dy);
            sum_weights += weights[m + 1][n + 1];
        }
    }

    // Normalize weights
    if (sum_weights > 0.0f) {
        for (int m = 0; m < 4; m++) {
            for (int n = 0; n < 4; n++) {
                weights[m][n] /= sum_weights;
            }
        }
    }

    float pixel_sum = 0.0f;

    // Compute interpolated pixel
    for (int m = -1; m <= 2; m++) {
        for (int n = -1; n <= 2; n++) {
            int x_coord = x_int + m;
            int y_coord = y_int + n;

            // Boundary handling
            x_coord = (x_coord < 0) ? 0 : (x_coord >= src_w ? src_w - 1 : x_coord);
            y_coord = (y_coord < 0) ? 0 : (y_coord >= src_h ? src_h - 1 : y_coord);

            int index = y_coord * src_w + x_coord;
            pixel_sum += src[index] * weights[m + 1][n + 1];
        }
    }

    // Clamp pixel to valid range
    return (unsigned char)fmaxf(0.0f, fminf(255.0f, pixel_sum));
}

// Bicubic resize image
int32_t bicubic_resize(unsigned char *dst,unsigned char *src, int src_w, int src_h,
                             int dst_w, int dst_h)
{
    if (!src || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
        return -1;
    }

    // Compute scale factors
    float scale_x = (float)src_w / dst_w;
    float scale_y = (float)src_h / dst_h;

    // Interpolate each destination pixel
    for (int y = 0; y < dst_h; y++) {
        for (int x = 0; x < dst_w; x++) {
            // Map to source coordinates
            float src_x = (x + 0.5f) * scale_x - 0.5f;
            float src_y = (y + 0.5f) * scale_y - 0.5f;

            int dst_index = y * dst_w + x;
            dst[dst_index] = get_bicubic_pixel(src, src_w, src_h, src_x, src_y);
        }
    }

    return 0;
}

// Image scale API (2x/3x/4x up/down sampling)
int32_t image_scale(unsigned char *dst, unsigned char *src, int src_w, int src_h, float scale_factor)
{
    if (!dst || !src || src_w <= 0 || src_h <= 0) {
        return -1;
    }

    int dst_w, dst_h;

    // Compute destination size
    dst_w = src_w / scale_factor;
    dst_h = src_h / scale_factor;

    // Call bicubic resize
    return bicubic_resize(dst, src, src_w, src_h, dst_w, dst_h);
}

int32_t image_warp(int16_t(*flow)[2], uint8_t* warped, const uint8_t* img, const int32_t width, const int32_t height, const int32_t stride)
{
	for (int32_t i = 0; i < height; i++)
	{
		for (int32_t j = 0; j < width; j++)
		{
			const int32_t idx = i * stride + j;
			int16_t mvx = flow[idx][0];
			int16_t mvy = flow[idx][1];

			int16_t mvx1 = mvx & 0x03;
			int16_t mvy1 = mvy & 0x03;
			int16_t mv4x = (mvx - mvx1)/4;
			int16_t mv4y = (mvy - mvy1)/4;
			float fmvx = (float)mvx1 / 4;
			float fmvy = (float)mvy1 / 4;

			int32_t int_pos_x = idx + mv4x;
			int32_t int_pos_y = idx + mv4y * stride;
			int32_t pix_int_x = img[idx + mv4x];
			int32_t pix_int_y = img[idx + mv4y * stride];

			int32_t idx_a = int_pos_x;
			int32_t idx_b = int_pos_x + 1;
			int32_t idx_c = int_pos_y + stride;
			int32_t idx_d = int_pos_y + stride + 1;

			int32_t Ia = img[idx_a];
			int32_t Ib = img[idx_b];
			int32_t Ic = img[idx_c];
			int32_t Id = img[idx_d];

			float wa = (1 - fmvx) * (1 - fmvy);
			float wb = (1 - fmvx) * fmvy;
			float wc = fmvx * (1 - fmvy);
			float wd = fmvx * fmvy;

			warped[idx] = wa * Ia + wb * Ib + wc * Ic + wd * Id;
		}
	}

	return 0;
}


int32_t rgb24_to_rgb888(uint8_t* src, const int32_t width, const int32_t height, uint8_t* dst[3])
{
	uint8_t* r = dst[0];
	uint8_t* g = dst[1];
	uint8_t* b = dst[2];

	int32_t r_cnt = 0, g_cnt = 0, b_cnt = 0;
	int32_t dst_width = width / 3;
	for (int32_t i = 0; i < height; i++)
	{
		for (int32_t j = 0; j < width; j += 3)
		{
			r[i * dst_width + r_cnt] = src[i * width + j];
			g[i * dst_width + g_cnt] = src[i * width + j + 1];
			b[i * dst_width + b_cnt] = src[i * width + j + 2];
			r_cnt++;
			g_cnt++;
			b_cnt++;
		}
		r_cnt = 0;
		g_cnt = 0;
		b_cnt = 0;
	}

	return 0;
}

int32_t rgb888_to_rgb24(uint8_t* dst, const int32_t width, const int32_t height, uint8_t* src[3])
{
	uint8_t* r = src[0];
	uint8_t* g = src[1];
	uint8_t* b = src[2];

	int32_t r_cnt = 0, g_cnt = 0, b_cnt = 0;
	int32_t dst_width = width / 3;
	for (int32_t i = 0; i < height; i++)
	{
		for (int32_t j = 0; j < width; j += 3)
		{
			dst[i * width + j] = r[i * dst_width + r_cnt];
			dst[i * width + j + 1] = g[i * dst_width + g_cnt];
			dst[i * width + j + 2] = b[i * dst_width + b_cnt];
			r_cnt++;
			g_cnt++;
			b_cnt++;
		}
		r_cnt = 0;
		g_cnt = 0;
		b_cnt = 0;
	}

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

// YUV444 to RGBA (C: standard conversion)
unsigned char* yuv444_to_rgba(const unsigned char* yuv444_data, int width, int height) {
    if (!yuv444_data || width <= 0 || height <= 0) return NULL;
    
    // RGBA size: width*height*4 (1 byte per R/G/B/A)
    int rgba_size = width * height * 4;
    unsigned char* rgba_data = (unsigned char*)malloc(rgba_size);
    if (!rgba_data) return NULL;
    
    int y_size = width * height;
    const unsigned char* y = yuv444_data;
    const unsigned char* u = y + y_size;
    const unsigned char* v = u + y_size;
    
    for (int i = 0; i < width * height; i++) {
        // BT.601 conversion (U/V must subtract 128)
        int Y = y[i];
        int U = u[i] - 128;
        int V = v[i] - 128;
        
        // Integer math avoids float precision issues, range 0~255
        int R = Y + (1402 * V) / 1000;  // 1.402 as integer
        int G = Y - (344 * U + 714 * V) / 1000; // 0.34414=344/1000, 0.71414=714/1000
        int B = Y + (1772 * U) / 1000;  // 1.772 as integer
        
        // Clamp to 0~255
        R = R < 0 ? 0 : (R > 255 ? 255 : R);
        G = G < 0 ? 0 : (G > 255 ? 255 : G);
        B = B < 0 ? 0 : (B > 255 ? 255 : B);
        
        // Fill RGBA (A=255 opaque)
        rgba_data[i*4]   = (unsigned char)R; // R channel
        rgba_data[i*4+1] = (unsigned char)G; // G channel
        rgba_data[i*4+2] = (unsigned char)B; // B channel
        rgba_data[i*4+3] = 255;              // A channel (required; missing caused misalignment)
    }
    
    return rgba_data;
}

unsigned char* yuv444_to_yuv420(const unsigned char* yuv444_data, int width, int height) {
    // 1. Validate args (even width/height required for YUV420 downsample)
    if (!yuv444_data || width <= 0 || height <= 0 ||
        (width % 2 != 0) || (height % 2 != 0)) {
        printf("yuv444_to_yuv420: invalid parameters (width/height must be even)\n");
        return NULL;
    }

    // 2. Split YUV444 input buffers
    int pixel_count = width * height;
    const unsigned char* Y444 = yuv444_data;
    const unsigned char* U444 = Y444 + pixel_count;
    const unsigned char* V444 = U444 + pixel_count;

    // 3. Compute YUV420 size (I420)
    int y_size = pixel_count;
    int uv_size = (width / 2) * (height / 2);
    int yuv420_size = y_size + uv_size * 2;
    unsigned char* yuv420_data = (unsigned char*)malloc(yuv420_size);
    if (!yuv420_data) {
        printf("yuv444_to_yuv420: memory allocation failed\n");
        return NULL;
    }

    // 4. Split YUV420 output buffers
    unsigned char* Y420 = yuv420_data;
    unsigned char* U420 = Y420 + y_size;
    unsigned char* V420 = U420 + uv_size;

    // 5. Step 1: copy Y plane (YUV420 Y matches YUV444 Y)
    memcpy(Y420, Y444, y_size);

    // 6. Step 2: UV 2:1 downsample (4:1 average, no blocking)
    int uv_idx = 0;
    for (int y = 0; y < height; y += 2) {
        for (int x = 0; x < width; x += 2) {
            // Sample 4 UV pixels from 2x2 block in YUV444
            int idx00 = y * width + x;
            int idx01 = y * width + (x + 1);
            int idx10 = (y + 1) * width + x;
            int idx11 = (y + 1) * width + (x + 1);

            // Average UV over 4 pixels
            int U_avg = (U444[idx00] + U444[idx01] + U444[idx10] + U444[idx11]) / 4;
            int V_avg = (V444[idx00] + V444[idx01] + V444[idx10] + V444[idx11]) / 4;

            // Clamp and write to YUV420 buffer
            U420[uv_idx] = (unsigned char)CLAMP(U_avg, 0, 255);
            V420[uv_idx] = (unsigned char)CLAMP(V_avg, 0, 255);
            uv_idx++;
        }
    }

    return yuv420_data;
}

unsigned char* yuv420_to_yuv444(const unsigned char* yuv420_data, int width, int height) {
    if (!yuv420_data || width <= 0 || height <= 0) return NULL;
    
    int y_size = width * height;
    int uv_size = y_size / 4;
    int yuv444_size = width * height * 3;;
    unsigned char* yuv444_data = (unsigned char*)malloc(yuv444_size);
    if (!yuv444_data) return NULL;
    
    // Copy Y plane (no interpolation)
    memcpy(yuv444_data, yuv420_data, y_size);
    
    // UV interpolation: YUV420 has one UV per 2x2 block; YUV444 needs one per pixel
    const unsigned char* u_src = yuv420_data + y_size;
    const unsigned char* v_src = u_src + uv_size;
    unsigned char* u_dst = yuv444_data + y_size;
    unsigned char* v_dst = u_dst + y_size;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // UV index in YUV420
            int uv_x = x / 2;
            int uv_y = y / 2;
            int uv_idx = uv_y * (width / 2) + uv_x;
            
            u_dst[y * width + x] = u_src[uv_idx];
            v_dst[y * width + x] = v_src[uv_idx];
        }
    }
    
    return yuv444_data;
}

unsigned char* rgba_to_yuv444(const uint8_t* rgba_data, int width, int height) {
    // Validate parameters
    if (rgba_data == NULL || width <= 0 || height <= 0) {
        fprintf(stderr, "Invalid args: width/height must be > 0, pointers must be non-NULL\n");
        return -1;
    }

    int pixel_count = width * height;
    unsigned char* yuv444_data = (unsigned char*)malloc(pixel_count*3);
    
    uint8_t* Y = yuv444_data;                // Y plane start
    uint8_t* U = yuv444_data + pixel_count;  // U plane start
    uint8_t* V = yuv444_data + 2 * pixel_count; // V plane start

    // Per-pixel Y/U/V conversion
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            // RGBA index for this pixel (4 bytes: R/G/B/A)
            int rgba_idx = (i * width + j) * 4;
            // Y/U/V index (one sample per component)
            int yuv_idx = i * width + j;

            // Extract R/G/B (ignore Alpha)
            uint8_t R = rgba_data[rgba_idx];     // Red
            uint8_t G = rgba_data[rgba_idx + 1]; // Green
            uint8_t B = rgba_data[rgba_idx + 2]; // Blue

            // ========== BT.601 conversion ==========
            // Y (luma): Y = 0.299*R + 0.587*G + 0.114*B
            // U (chroma): U = -0.14713*R - 0.28886*G + 0.436*B + 128
            // V (chroma): V = 0.615*R - 0.51499*G - 0.10001*B + 128
            // +0.5 for rounding
            Y[yuv_idx] = (uint8_t)(0.299 * R + 0.587 * G + 0.114 * B + 0.5);
            U[yuv_idx] = (uint8_t)(-0.14713 * R - 0.28886 * G + 0.436 * B + 128 + 0.5);
            V[yuv_idx] = (uint8_t)(0.615 * R - 0.51499 * G - 0.10001 * B + 128 + 0.5);

            // ========== For BT.709 (HD), use formulas below ==========
            // Y = 0.2126*R + 0.7152*G + 0.0722*B + 0.5;
            // U = -0.1146*R - 0.3854*G + 0.5000*B + 128 + 0.5;
            // V = 0.5000*R - 0.4542*G - 0.0458*B + 128 + 0.5;
        }
    }

    return yuv444_data;
}

// BT.601 conversion coefficients (integer, avoid float cost)
#define Y_R  77
#define Y_G  150
#define Y_B  29
#define U_R -43
#define U_G -85
#define U_B 128
#define V_R 128
#define V_G -107
#define V_B -21

// Compute total bytes for YUV420P
size_t calc_yuv420p_size(int w, int h)
{
    int half_w = w / 2;
    int half_h = h / 2;
    return (size_t)w * h + 2 * (size_t)half_w * half_h;
}

// RGBA to YUV420P, handles odd width/height
// rgba: input RGBA buffer, w*h*4 bytes
// yuv: output YUV420P buffer, layout Y + U + V
// w,h: source width/height (may be odd)
uint8_t* rgba_to_yuv420(const uint8_t* rgba, int w, int h)
{
    // Chroma plane size rounded down to even
    int half_w = w / 2;
    int half_h = h / 2;
    int stride_y = w;
    int stride_u = half_w;
    int stride_v = half_w;

    size_t yuv_size = calc_yuv420p_size(w+1, h);
    unsigned char* yuv = (unsigned char*)malloc(yuv_size);
    if (!yuv) {
        fprintf(stderr, "Memory allocation failed: need %d bytes\n", yuv_size);
        return NULL;
    }

    uint8_t* y_plane = yuv;
    uint8_t* u_plane = y_plane + stride_y * h;
    uint8_t* v_plane = u_plane + stride_u * half_h;

    // Process 2 rows at a time for 420 sampling
    for (int y = 0; y < h; y += 2)
    {
        for (int x = 0; x < w; x += 2)
        {
            // Four sample coords with bounds check
            int x0 = x;
            int x1 = x + 1 >= w ? x : x + 1;
            int y0 = y;
            int y1 = y + 1 >= h ? y : y + 1;

            // Fetch 4 RGBA pixels
            const uint8_t* p00 = rgba + (y0 * w + x0) * 4;
            const uint8_t* p01 = rgba + (y0 * w + x1) * 4;
            const uint8_t* p10 = rgba + (y1 * w + x0) * 4;
            const uint8_t* p11 = rgba + (y1 * w + x1) * 4;

            uint8_t r00 = p00[0], g00 = p00[1], b00 = p00[2];
            uint8_t r01 = p01[0], g01 = p01[1], b01 = p01[2];
            uint8_t r10 = p10[0], g10 = p10[1], b10 = p10[2];
            uint8_t r11 = p11[0], g11 = p11[1], b11 = p11[2];

            // Compute 4 Y values, write Y plane
            int yy00 = (Y_R * r00 + Y_G * g00 + Y_B * b00) >> 8;
            int yy01 = (Y_R * r01 + Y_G * g01 + Y_B * b01) >> 8;
            int yy10 = (Y_R * r10 + Y_G * g10 + Y_B * b10) >> 8;
            int yy11 = (Y_R * r11 + Y_G * g11 + Y_B * b11) >> 8;

            y_plane[y0 * stride_y + x0] = (uint8_t)(yy00 < 0 ? 0 : (yy00 > 255 ? 255 : yy00));
            y_plane[y0 * stride_y + x1] = (uint8_t)(yy01 < 0 ? 0 : (yy01 > 255 ? 255 : yy01));
            y_plane[y1 * stride_y + x0] = (uint8_t)(yy10 < 0 ? 0 : (yy10 > 255 ? 255 : yy10));
            y_plane[y1 * stride_y + x1] = (uint8_t)(yy11 < 0 ? 0 : (yy11 > 255 ? 255 : yy11));

            // Average 4 RGB pixels for U/V
            int avg_r = (r00 + r01 + r10 + r11) / 4;
            int avg_g = (g00 + g01 + g10 + g11) / 4;
            int avg_b = (b00 + b01 + b10 + b11) / 4;

            int uu = 128 + ((U_R * avg_r + U_G * avg_g + U_B * avg_b) >> 8);
            int vv = 128 + ((V_R * avg_r + V_G * avg_g + V_B * avg_b) >> 8);

            // UV coordinates
            int ux = x / 2;
            int uy = y / 2;
            u_plane[uy * stride_u + ux] = (uint8_t)(uu < 0 ? 0 : (uu > 255 ? 255 : uu));
            v_plane[uy * stride_v + ux] = (uint8_t)(vv < 0 ? 0 : (vv > 255 ? 255 : vv));
        }
    }

    return yuv;
}

