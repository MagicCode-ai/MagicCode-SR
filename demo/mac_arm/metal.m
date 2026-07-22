

#include <Foundation/Foundation.h>
#include <MetalKit/MetalKit.h>
#include <MetalPerformanceShaders/MetalPerformanceShaders.h>
#include <Metal/Metal.h>
#include <stdint.h>
#include "metal.h"

typedef struct {
    int width;
    int height;
} ImageSize;

#pragma mark - C format conversion function declarations
// 1. YUV420 to YUV444 (C implementation)
static unsigned char* yuv420_to_yuv444_cpu(const unsigned char* yuv420_data, ImageSize size);

// YUV444 to RGB24 (C implementation)
static unsigned char* yuv444_to_rgb24_cpu(const unsigned char* yuv444_data, ImageSize size);

// RGB24 to YUV444 (C implementation)
static unsigned char* rgb24_to_yuv444_cpu(const unsigned char* rgb24_data, ImageSize size);

// YUV444 to YUV420 (C implementation)
static unsigned char* yuv444_to_yuv420_cpu(const unsigned char* yuv444_data, ImageSize size);

// SR placeholder (2x upsample on Y channel)
void sr_process(unsigned char* y_data, ImageSize input_size, ImageSize* output_size);

// Bilinear upsample (2x on UV components)
unsigned char* bilinear_upsample(unsigned char* uv_data, ImageSize input_size, ImageSize* output_size);

#pragma mark - OC+Metal processor class
@interface YUVMetalProcessor : NSObject

// Initialize Metal device and command queue
- (instancetype)init;

// 2. Upload RGB24 data as Metal texture (GPU)
- (id<MTLTexture>)createRGBATextureWithData:(const unsigned char*)data pix_format:(MTLPixelFormat)pix_format size:(ImageSize)size;

// 3. RGB24 texture to YUV444 texture (Metal shader)
- (id<MTLTexture>)convertRGBAToYUV444Texture:(id<MTLTexture>)rgbTexture size:(ImageSize)size;

// 4. Process YUV444 texture (Y SR, UV bilinear upsample, placeholder)
- (id<MTLTexture>)processYUV444Texture:(id<MTLTexture>)yuv444Texture
                             inputSize:(ImageSize)inputSize
                            outputSize:(ImageSize*)outputSize;

// 5. YUV444 texture to RGBA texture (Metal shader)
- (id<MTLTexture>)convertYUV444ToRGBATexture:(id<MTLTexture>)yuv444Texture size:(ImageSize)size;

// 6. Download GPU RGB24 texture to CPU
- (unsigned char*)downloadRGBATexture:(id<MTLTexture>)rgbTexture size:(ImageSize)size;

@end


// Helper macro: clamp values to 0~255
#ifndef CLAMP
#define CLAMP(x, min_val, max_val) ((x) < (min_val) ? (min_val) : ((x) > (max_val) ? (max_val) : (x)))
#endif

// Utility: compute image buffer sizes
NS_INLINE NSUInteger yuv420_data_size(ImageSize size) {
    return size.width * size.height * 3 / 2;
}

NS_INLINE NSUInteger yuv444_data_size(ImageSize size) {
    return size.width * size.height * 3;
}

NS_INLINE NSUInteger rgb32_data_size(ImageSize size) {
    return size.width * size.height * 4;
}

#pragma mark - C format conversion implementation
// 1. YUV420 to YUV444 (C: interpolate UV to every pixel)
unsigned char* yuv420_to_yuv444_cpu(const unsigned char* yuv420_data, ImageSize size) {
    if (!yuv420_data || size.width <= 0 || size.height <= 0) return NULL;
    
    NSUInteger y_size = size.width * size.height;
    NSUInteger uv_size = y_size / 4;
    NSUInteger yuv444_size = yuv444_data_size(size);
    unsigned char* yuv444_data = (unsigned char*)malloc(yuv444_size);
    if (!yuv444_data) return NULL;
    
    // Copy Y plane (no interpolation)
    memcpy(yuv444_data, yuv420_data, y_size);
    
    // UV interpolation: YUV420 has one UV per 2x2 block; YUV444 needs one per pixel
    const unsigned char* u_src = yuv420_data + y_size;
    const unsigned char* v_src = u_src + uv_size;
    unsigned char* u_dst = yuv444_data + y_size;
    unsigned char* v_dst = u_dst + y_size;
    
    for (NSInteger y = 0; y < size.height; y++) {
        for (NSInteger x = 0; x < size.width; x++) {
            // UV index in YUV420
            NSInteger uv_x = x / 2;
            NSInteger uv_y = y / 2;
            NSInteger uv_idx = uv_y * (size.width / 2) + uv_x;
            
            u_dst[y * size.width + x] = u_src[uv_idx];
            v_dst[y * size.width + x] = v_src[uv_idx];
        }
    }
    
    return yuv444_data;
}

// RGB24 to YUV444 (C: standard conversion)
/**
 * Fixed version: RGBA to YUV444 (BT.601 standard, discard Alpha channel)
 * @param rgba_data Input RGBA data (4 bytes/pixel: R G B A)
 * @param size Image dimensions
 * @return YUV444 data (planar: Y(w*h) + U(w*h) + V(w*h), caller must free)
 */
static unsigned char* rgba_to_yuv444_cpu(const unsigned char* rgba_data, ImageSize size) {
    // 1. Validate input data and dimensions
    if (!rgba_data || size.width <= 0 || size.height <= 0) {
        printf("rgba_to_yuv444: invalid input parameters\n");
        return NULL;
    }

    // 2. Compute size and allocate
    NSUInteger pixel_count = size.width * size.height;
    NSUInteger yuv444_size = pixel_count * 3; // Y + U + V, one plane each
    unsigned char* yuv444_data = (unsigned char*)malloc(yuv444_size);
    if (!yuv444_data) {
        printf("rgba_to_yuv444: memory allocation failed\n");
        return NULL;
    }

    // 3. Split Y/U/V buffers
    unsigned char* Y = yuv444_data;
    unsigned char* U = Y + pixel_count;
    unsigned char* V = U + pixel_count;

    // 4. Per-pixel convert (discard A, use R/G/B only)
    for (NSUInteger i = 0; i < pixel_count; i++) {
        
        if (i == 120)
            i = i;
        // Read R/G/B, discard A (rgba_data[i*4+3])
        int R = rgba_data[i*4];
        int G = rgba_data[i*4+1];
        int B = rgba_data[i*4+2];

        // BT.601 formula (integer math, avoid float precision loss)
        int Y_val = (299 * R + 587 * G + 114 * B) / 1000;       // Y = 0.299R + 0.587G + 0.114B
        int U_val = (-147 * R - 289 * G + 436 * B) / 1000 + 128; // U = -0.147R -0.289G +0.436B +128
        int V_val = (615 * R - 515 * G - 100 * B) / 1000 + 128; // V = 0.615R -0.515G -0.100B +128

        // Clamp to 0-255 to prevent color corruption from overflow
        Y_val = (Y_val < 0) ? 0 : (Y_val > 255 ? 255 : Y_val);
        U_val = (U_val < 0) ? 0 : (U_val > 255 ? 255 : U_val);
        V_val = (V_val < 0) ? 0 : (V_val > 255 ? 255 : V_val);

        // Write to YUV buffers
        Y[i] = (unsigned char)Y_val;
        U[i] = (unsigned char)U_val;
        V[i] = (unsigned char)V_val;
    }
    
    printf("=== YUV values for first 10 pixels ===\n");
    for (int i = 100; i < 200 && i < pixel_count; i++) {
        printf("pixel %d: Y=%d, U=%d, V=%d\n",
               i,
               yuv444_data[i],
               yuv444_data[pixel_count + i],
               yuv444_data[2*pixel_count + i]);
    }

    return yuv444_data;
}

// YUV444 to YUV420 (C: UV downsampling)
/**
 * YUV444 to YUV420 (I420 planar format, BT.601 standard)
 * @param yuv444_data Input YUV444 data (planar: Y+U+V)
 * @param size Image dimensions (width/height must be even for downsampling)
 * @return YUV420 data (I420: Y(w*h) + U(w/2*h/2) + V(w/2*h/2), caller must free)
 */
static unsigned char* yuv444_to_yuv420_cpu(const unsigned char* yuv444_data, ImageSize size) {
    // 1. Validate args (even width/height required for YUV420 downsample)
    if (!yuv444_data || size.width <= 0 || size.height <= 0 ||
        (size.width % 2 != 0) || (size.height % 2 != 0)) {
        printf("yuv444_to_yuv420: invalid parameters (width/height must be even)\n");
        return NULL;
    }

    // 2. Split YUV444 input buffers
    NSUInteger pixel_count = size.width * size.height;
    const unsigned char* Y444 = yuv444_data;
    const unsigned char* U444 = Y444 + pixel_count;
    const unsigned char* V444 = U444 + pixel_count;

    // 3. Compute YUV420 size (I420)
    NSUInteger y_size = pixel_count;
    NSUInteger uv_size = (size.width / 2) * (size.height / 2);
    NSUInteger yuv420_size = y_size + uv_size * 2;
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
    NSInteger uv_idx = 0;
    for (NSInteger y = 0; y < size.height; y += 2) {
        for (NSInteger x = 0; x < size.width; x += 2) {
            // Sample 4 UV pixels from 2x2 block in YUV444
            NSInteger idx00 = y * size.width + x;
            NSInteger idx01 = y * size.width + (x + 1);
            NSInteger idx10 = (y + 1) * size.width + x;
            NSInteger idx11 = (y + 1) * size.width + (x + 1);

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

// Generic clamp macro (ensure defined)
#ifndef CLAMP
#define CLAMP(x, min_val, max_val) ((x) < (min_val) ? (min_val) : ((x) > (max_val) ? (max_val) : (x)))
#endif

// SR placeholder (resize only, no real processing)
void sr_process(unsigned char* y_data, ImageSize input_size, ImageSize* output_size) {
    if (!y_data || !output_size) return;
    
    // Output size is 2x input
    output_size->width = input_size.width * 2;
    output_size->height = input_size.height * 2;
    
    // Placeholder: real SR logic goes here
    NSLog(@"Y-channel 2x SR processing done (placeholder)");
}

// Compute Metal-aligned bytesPerRow (256-byte alignment)
NS_INLINE NSUInteger metal_aligned_bytes_per_row(NSUInteger width, NSUInteger bytes_per_pixel) {
    NSUInteger base_bytes = width * bytes_per_pixel;
    // Metal requires 256-byte row alignment, round up
    NSUInteger align = 256;
    return ((base_bytes + align - 1) / align) * align;
}

/**
 * @brief 2x bilinear upsample of YUV444 UV components (CPU-side)
 * @param uv_data Input UV data (U then V contiguous, length=width*height each)
 * @param input_size Input image size (width/height)
 * @param output_size Output image size (2x input, filled by this function)
 * @return Upsampled UV data (length=2*width*2*height, caller must free)
 */
unsigned char* bilinear_upsample(unsigned char* uv_data, ImageSize input_size, ImageSize* output_size) {
    // 1. Validate parameters
    if (!uv_data || input_size.width <= 0 || input_size.height <= 0 || !output_size) {
        printf("bilinear_upsample: invalid parameters\n");
        return NULL;
    }

    // 2. Set output size (2x upscale)
    output_size->width = input_size.width * 2;
    output_size->height = input_size.height * 2;
    int out_w = output_size->width;
    int out_h = output_size->height;
    int in_w = input_size.width;
    int in_h = input_size.height;

    // 3. Allocate output (U/V each out_w*out_h, total out_w*out_h*2)
    unsigned char* upsampled_uv = (unsigned char*)malloc(out_w * out_h * 2);
    if (!upsampled_uv) {
        printf("bilinear_upsample: memory allocation failed\n");
        return NULL;
    }

    // 4. Bilinear interpolation core
    for (int y_out = 0; y_out < out_h; y_out++) {
        for (int x_out = 0; x_out < out_w; x_out++) {
            // 4.1 Map output pixel to input float coordinates
            float x_in = (float)x_out / 2.0f;  // Output x maps to input x (half scale)
            float y_in = (float)y_out / 2.0f;  // Output y maps to input y (half scale)

            // 4.2 Four neighbor integer coords (floor/ceil)
            int x0 = (int)floor(x_in);
            int y0 = (int)floor(y_in);
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            // 4.3 Clamp coords to bounds
            x0 = (x0 < 0) ? 0 : (x0 >= in_w ? in_w - 1 : x0);
            y0 = (y0 < 0) ? 0 : (y0 >= in_h ? in_h - 1 : y0);
            x1 = (x1 < 0) ? 0 : (x1 >= in_w ? in_w - 1 : x1);
            y1 = (y1 < 0) ? 0 : (y1 >= in_h ? in_h - 1 : y1);

            // 4.4 Interpolation weights (fractional offset)
            float dx = x_in - x0;  // X fractional offset (0~1)
            float dy = y_in - y0;  // Y fractional offset (0~1)

            // 4.5 Read four neighbors (UV stored U then V)
            // Input UV layout: U[0..in_w*in_h-1], V[in_w*in_h..2*in_w*in_h-1]
            unsigned char u00 = uv_data[y0 * in_w + x0];          // U at (x0,y0)
            unsigned char u01 = uv_data[y1 * in_w + x0];          // U at (x0,y1)
            unsigned char u10 = uv_data[y0 * in_w + x1];          // U at (x1,y0)
            unsigned char u11 = uv_data[y1 * in_w + x1];          // U at (x1,y1)

            unsigned char v00 = uv_data[in_w * in_h + y0 * in_w + x0];  // V at (x0,y0)
            unsigned char v01 = uv_data[in_w * in_h + y1 * in_w + x0];  // V at (x0,y1)
            unsigned char v10 = uv_data[in_w * in_h + y0 * in_w + x1];  // V at (x1,y0)
            unsigned char v11 = uv_data[in_w * in_h + y1 * in_w + x1];  // V at (x1,y1)

            // 4.6 Bilinear interpolate U
            float u_interp = (1 - dx) * (1 - dy) * u00 + dx * (1 - dy) * u10 +
                             (1 - dx) * dy * u01 + dx * dy * u11;

            // 4.7 Bilinear interpolate V
            float v_interp = (1 - dx) * (1 - dy) * v00 + dx * (1 - dy) * v10 +
                             (1 - dx) * dy * v01 + dx * dy * v11;

            // 4.8 Clamp (0~255) and write output
            int out_idx = y_out * out_w + x_out;
            upsampled_uv[out_idx] = (unsigned char)CLAMP(u_interp, 0.0f, 255.0f);          // Output U
            upsampled_uv[out_w * out_h + out_idx] = (unsigned char)CLAMP(v_interp, 0.0f, 255.0f);  // Output V
        }
    }

    return upsampled_uv;
}

#define MTL_STRINGIFY_S(s) @ #s
#pragma mark - Metal Shader strings (inline)
static NSString *const kRGB2YUV444Shader = MTL_STRINGIFY_S(

using namespace metal;

struct ShaderDebugData {
   float R;
   float G;
   float B;
   float Y;
   float U_raw;
   float U;
   float V_raw;
   float V;
};
                                                           
kernel void rgb2yuv444_test(
    texture2d<half, access::read> inputTexture [[texture(0)]],
    texture2d<half, access::write> outputTexture [[texture(1)]],
    device ShaderDebugData* debugData [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]]) {
        
    if (gid.x >= inputTexture.get_width() || gid.y >= inputTexture.get_height()) {
      //  return;
    }
        
    half3 rgb = inputTexture.read(gid).rgb;
    half R = rgb.r;
    half G = rgb.g;
    half B = rgb.b;
    
    // RGB to YUV (normalized 0-1)
    half Y = 0.299 * R + 0.587 * G + 0.114 * B;
    half U = (-0.14713 * R - 0.28886 * G + 0.436 * B) + 0.5;
    half V = (0.615 * R - 0.51499 * G - 0.10001 * B) + 0.5;
        
        half U_raw = -0.14713f * R - 0.28886f * G + 0.436f * B;
        half V_raw = 0.615f * R - 0.51499f * G - 0.10001f * B;
    
    // Explicit clamp (optional, avoid overflow)
    Y = clamp(Y, half(0.0), half(1.0));
    U = clamp(U, half(0.0), half(1.0));
    V = clamp(V, half(0.0), half(1.0));
        
        if(gid.x == 64 && gid.y == 0){
            // Write intermediate values to buffer
            debugData->R = R;
            debugData->G = G;
            debugData->B = B;
            debugData->Y = Y;
            debugData->U_raw = U_raw;
            debugData->U = U;
            debugData->V_raw = V_raw;
            debugData->V = V;
        }
    
    outputTexture.write(half4(Y, U, V, 1.0), gid);
}
);

static NSString *const kYUV444ToRGBAShader = MTL_STRINGIFY_S(

using namespace metal;

kernel void yuv4442rgba(
   texture2d<half, access::read> inputTexture [[texture(0)]],
   texture2d<half, access::write> outputTexture [[texture(1)]],
   uint2 gid [[thread_position_in_grid]]) {
   
   half4 yuv = inputTexture.read(gid);
       half Y = yuv.r;
       half U = yuv.g - 0.5;  // Denormalize U (0-1 -> -0.5~0.5)
       half V = yuv.b - 0.5;  // Denormalize V (0-1 -> -0.5~0.5)
   
   // YUV to RGB (BT.601)
   half R = Y + 1.402 * V;
   half G = Y - 0.34414 * U - 0.71414 * V;
   half B = Y + 1.772 * U;
   
   // Clamp to 0-1
   R = clamp(R, half(0.0), half(1.0));
   G = clamp(G, half(0.0), half(1.0));
   B = clamp(B, half(0.0), half(1.0));
   
   // Output RGBA (Alpha fixed at 1.0)
   outputTexture.write(half4(R, G, B, half(1.0)), gid);
}
);

// Bilinear upsample shader (Y/UV, 2x)
static NSString *const kBilinearUpsampleShader = MTL_STRINGIFY_S(
using namespace metal;

// Bilinear interpolation helper
half4 bilinear_sample(texture2d<half, access::read> tex, float2 uv, float2 tex_size) {
    float2 texel_uv = uv * tex_size - float2(0.5);
    float2 texel_floor = floor(texel_uv);
    float2 texel_frac = texel_uv - texel_floor;
    
    uint2 coord00 = uint2(texel_floor);
    uint2 coord01 = uint2(texel_floor.x, texel_floor.y + 1);
    uint2 coord10 = uint2(texel_floor.x + 1, texel_floor.y);
    uint2 coord11 = uint2(texel_floor.x + 1, texel_floor.y + 1);
    
    // Clamp to texture bounds
    coord00 = clamp(coord00, uint2(0), uint2(tex_size) - uint2(1));
    coord01 = clamp(coord01, uint2(0), uint2(tex_size) - uint2(1));
    coord10 = clamp(coord10, uint2(0), uint2(tex_size) - uint2(1));
    coord11 = clamp(coord11, uint2(0), uint2(tex_size) - uint2(1));
    
    // Sample four neighbor pixels
    half4 c00 = tex.read(coord00);
    half4 c01 = tex.read(coord01);
    half4 c10 = tex.read(coord10);
    half4 c11 = tex.read(coord11);
    
    // Bilinear interpolation
    half4 c0 = mix(c00, c01, texel_frac.y);
    half4 c1 = mix(c10, c11, texel_frac.y);
    return mix(c0, c1, texel_frac.x);
}

kernel void bilinear_upsample_2x(
    texture2d<half, access::read> inputTexture [[texture(0)]],
    texture2d<half, access::write> outputTexture [[texture(1)]],
    uint2 gid [[thread_position_in_grid]]) {
    
    // Output is 2x input; compute input UV coords
    float2 input_size = float2(inputTexture.get_width(), inputTexture.get_height());
    float2 output_size = float2(outputTexture.get_width(), outputTexture.get_height());
    float2 uv = float2(gid) / output_size;
    
    // Bilinear sample
    half4 color = bilinear_sample(inputTexture, uv, input_size);
    
    // Write output texture
    outputTexture.write(color, gid);
}
);


// Helper macro: float clamp
#ifndef CLAMP_FLOAT
#define CLAMP_FLOAT(x, min_val, max_val) ((x) < (min_val) ? (min_val) : ((x) > (max_val) ? (max_val) : (x)))
#endif

/**
 * YUV444 to RGBA in RGBA texture layout (CPU-side, for Metal texture data)
 * @param yuv444_rgba_data Input YUV444 as RGBA (R=Y, G=U, B=V, A=255, [0,255] bytes)
 * @param size Image dimensions
 * @return Output RGBA data (4 bytes/pixel), caller must free; NULL on failure
 */
unsigned char* yuv444_to_rgba_cpu(const unsigned char* yuv444_rgba_data, ImageSize size) {
    if (!yuv444_rgba_data || size.width <= 0 || size.height <= 0) {
        printf("yuv444_rgba_to_rgba: invalid input parameters\n");
        return NULL;
    }

    NSUInteger pixel_count = size.width * size.height;
    unsigned char* rgba_data = (unsigned char*)malloc(pixel_count * 4);
    if (!rgba_data) return NULL;

    for (NSInteger i = 0; i < pixel_count; i++) {
        
        if(i == 128)
            i = 128;
        // Read Y/U/V from RGBA (R=Y, G=U, B=V)
        float Y_float = (float)yuv444_rgba_data[i*3] / 255.0f;
        float U_float = (float)yuv444_rgba_data[i*3+1] / 255.0f - 0.5f;
        float V_float = (float)yuv444_rgba_data[i*3+2] / 255.0f - 0.5f;

        // Same BT.601 formula as above
        float R = Y_float + 1.402f * V_float;
        float G = Y_float - 0.34414f * U_float - 0.71414f * V_float;
        float B = Y_float + 1.772f * U_float;

        // Clamp and convert to bytes
        R = CLAMP_FLOAT(R, 0.0f, 1.0f);
        G = CLAMP_FLOAT(G, 0.0f, 1.0f);
        B = CLAMP_FLOAT(B, 0.0f, 1.0f);

        rgba_data[i*4]   = (unsigned char)(R * 255.0f);
        rgba_data[i*4+1] = (unsigned char)(G * 255.0f);
        rgba_data[i*4+2] = (unsigned char)(B * 255.0f);
        rgba_data[i*4+3] = 255;
    }

    return rgba_data;
}

void* gen_rgba_texture(unsigned char *rgba_data, int texture_type, int width, int height)
{
    ImageSize input_size;
    input_size.width = width;
    input_size.height = height;
    
    MTLPixelFormat pixel_format = MTLPixelFormatRGBA8Unorm;
    if(texture_type == 1)
        pixel_format = MTLPixelFormatRGBA8Unorm;
    else if(texture_type == 2)
        pixel_format = MTLPixelFormatR8Unorm;
    else if(texture_type == 20)
        pixel_format = MTLPixelFormatRGBA8Snorm;
    else if(texture_type == 21)
        pixel_format = MTLPixelFormatRGBA8Uint;
    else
    {
        NSLog(@"creat_texture2d: texture_type is wrong.\n");
        return nil;
    }
    
    YUVMetalProcessor *processor = [[YUVMetalProcessor alloc] init];
    id<MTLTexture> rgbTexture = [processor createRGBATextureWithData:rgba_data pix_format:pixel_format size:input_size];
    
    CFRetain((__bridge CFTypeRef)rgbTexture);
    
    return (__bridge void *)rgbTexture;
}

void* creat_texture2d(int width, int height, int texture_type, int usage)
{
    int pixel_format = MTLPixelFormatRGBA8Unorm;
    if(texture_type == 1)
        pixel_format = MTLPixelFormatRGBA8Unorm;
    else if(texture_type == 2)
        pixel_format = MTLPixelFormatR8Unorm;
    else if(texture_type == 20)
        pixel_format = MTLPixelFormatRGBA8Snorm;
    else if(texture_type == 21)
        pixel_format = MTLPixelFormatRGBA8Uint;
    else
    {
        NSLog(@"creat_texture2d: texture_type is wrong.\n");
        return nil;
    }
      
    // 2. Create RGBA texture descriptor (RGBA8Unorm, 4 bytes/pixel)
    MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixel_format
                                                                                  width:width
                                                                                 height:height
                                                                                mipmapped:NO];
    if(usage == 0)
        desc.usage = MTLTextureUsageShaderRead;
    else if(usage == 1)
        desc.usage = MTLTextureUsageShaderWrite;
    else if(usage == 2)
        desc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
    else if(usage == 3)
        desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite| MTLTextureUsageRenderTarget;
    else
    {
        NSLog(@"creat_texture2d: desc.usage is wrong.\n");
        return nil;
    }
    
   // desc.storageMode = MTLStorageModePrivate;

    id<MTLDevice> _device = MTLCreateSystemDefaultDevice();
    // 3. Create texture object
    id<MTLTexture> texture = [_device newTextureWithDescriptor:desc];
    if (!texture) {
        NSLog(@"creat_texture2d: failed to create texture");
        return nil;
    }
    
    CFRetain((__bridge CFTypeRef)texture);
    
    return (__bridge void *)texture;
}

unsigned char* download_rgba_texture(void* in_texture, int width, int height)
{
    ImageSize size;
    size.width = width;
    size.height = height;
    // 1. Validate parameters
    if (!in_texture || width <= 0 || height <= 0) {
        NSLog(@"downloadRGBATexture: invalid parameters");
        return NULL;
    }
    id<MTLTexture> rgbaTexture = (__bridge id<MTLTexture>)in_texture;

    // 2. Get alignment requirements (legacy compat)
    NSUInteger bytes_per_pixel = 4; // RGBA8Unorm
    NSUInteger valid_bpr = size.width * bytes_per_pixel;
    NSUInteger alignment = 256; // Default min alignment is 256 bytes on most devices
    if ([rgbaTexture.device respondsToSelector:@selector(minimumLinearTextureAlignmentForPixelFormat:)]) {
        alignment = [rgbaTexture.device minimumLinearTextureAlignmentForPixelFormat:rgbaTexture.pixelFormat];
    }
    NSUInteger aligned_bpr = ((valid_bpr + alignment - 1) / alignment) * alignment;

   // NSLog(@"aligned_bpr = %lu, valid_bpr = %lu", aligned_bpr, valid_bpr);

    // 3. Allocate aligned temp buffer
    NSUInteger aligned_total_size = aligned_bpr * size.height;
    unsigned char* aligned_rgba_data = (unsigned char*)malloc(aligned_total_size);
    if (!aligned_rgba_data) {
        NSLog(@"downloadRGBATexture: aligned buffer allocation failed");
        return NULL;
    }
    memset(aligned_rgba_data, 0, aligned_total_size);

    // 4. Read from Metal texture (must use aligned_bpr)
    MTLRegion region = MTLRegionMake2D(0, 0, size.width, size.height);
    [rgbaTexture getBytes:aligned_rgba_data
             bytesPerRow:aligned_bpr
               fromRegion:region
              mipmapLevel:0];

    // 5. Extract valid data (strip row padding)
    NSUInteger valid_total_size = valid_bpr * size.height;
    unsigned char* valid_rgba_data = (unsigned char*)malloc(valid_total_size);
    if (!valid_rgba_data) {
        NSLog(@"downloadRGBATexture: valid buffer allocation failed");
        free(aligned_rgba_data);
        return NULL;
    }
    for (NSUInteger y = 0; y < size.height; y++) {
        const unsigned char* src_row = aligned_rgba_data + y * aligned_bpr;
        unsigned char* dst_row = valid_rgba_data + y * valid_bpr;
        memcpy(dst_row, src_row, valid_bpr);
    }

    // 6. Free temp buffer
    free(aligned_rgba_data);
    return valid_rgba_data;
}

void *down_private_texture_to_cpu(void* in_texture)
{
    
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    
    id<MTLTexture> privateTexture = (__bridge id<MTLTexture>)in_texture;
    
    id<MTLCommandQueue> commandQueue = [device newCommandQueue];
    
    // 2. Create CPU-accessible temp texture (Shared mode)
    MTLTextureDescriptor *tempDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:privateTexture.pixelFormat
                                                                                              width:privateTexture.width
                                                                                             height:privateTexture.height
                                                                                          mipmapped:NO];
    tempDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    tempDescriptor.storageMode = MTLStorageModeShared; // Key: MTLStorageModeShared for CPU access
    id<MTLTexture> tempTexture = [device newTextureWithDescriptor:tempDescriptor];
    if (!tempTexture) {
        NSLog(@"Failed to create temporary texture");
        return nil;
    }
    
    // 3. Command buffer + Blit encoder for GPU texture copy
    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
    id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
    if (!blitEncoder) {
        NSLog(@"Failed to create Blit encoder");
        return nil;
    }
    
    // Copy full texture region
    MTLOrigin sourceOrigin = MTLOriginMake(0, 0, 0);
    MTLSize sourceSize = MTLSizeMake(privateTexture.width, privateTexture.height, 1);
    [blitEncoder copyFromTexture:privateTexture
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:sourceOrigin
                      sourceSize:sourceSize
                        toTexture:tempTexture
                destinationSlice:0
                destinationLevel:0
               destinationOrigin:sourceOrigin];
    
    [blitEncoder endEncoding]; // End encoder
    [commandBuffer commit];    // Submit command buffer
    [commandBuffer waitUntilCompleted]; // Wait for GPU copy (required before CPU read)
    
    // 4. Read temp texture into CPU memory
    NSUInteger bytesPerPixel = 4; // e.g. RGBA8Unorm is 4 bytes/pixel; adjust for other formats
    NSUInteger bytesPerRow = privateTexture.width * bytesPerPixel;
    NSUInteger totalBytes = bytesPerRow * privateTexture.height;
    
    // Allocate and read texture data
    void *pixelBuffer = malloc(totalBytes);
    if (!pixelBuffer) {
        NSLog(@"Memory allocation failed");
        return nil;
    }
    
    MTLRegion region = MTLRegionMake2D(0, 0, privateTexture.width, privateTexture.height);
    [tempTexture getBytes:pixelBuffer
               bytesPerRow:bytesPerRow
                  fromRegion:region
                 mipmapLevel:0];
    
    // Wrap in NSData (automatic memory)
    NSData *pixelData = [NSData dataWithBytesNoCopy:pixelBuffer length:totalBytes freeWhenDone:YES];
    
    // Validate data (optional)
    if (pixelData.length != totalBytes) {
        NSLog(@"Texture data length mismatch");
        return nil;
    }
    
   // NSLog(@"Successfully read %lu bytes of texture data", (unsigned long)pixelData.length);
    return pixelData.bytes;
}

void release_texture(void *p_texture)
{
    id<MTLTexture> tex = (__bridge id<MTLTexture>)p_texture;
    
    BOOL ret = [tex conformsToProtocol:@protocol(MTLTexture)];
    if(ret == false)
        return;
    
    CFRelease((__bridge CFTypeRef)tex);
    
    tex = nil;
}


#pragma mark - YUVMetalProcessor implementation
@interface YUVMetalProcessor ()
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) id<MTLLibrary> library; // Added
@property (nonatomic, strong) id<MTLComputePipelineState> rgb2yuvPipeline;
@property (nonatomic, strong) id<MTLComputePipelineState> yuv2rgbaPipeline;
@property (nonatomic, strong) id<MTLComputePipelineState> bilinearUpsamplePipeline;
@end

@implementation YUVMetalProcessor

- (instancetype)init {
    self = [super init];
    if (self) {
        self.device = MTLCreateSystemDefaultDevice();
        if (!self.device) {
            NSLog(@"Metal device initialization failed");
            return nil;
        }

        self.commandQueue = [self.device newCommandQueue];

        // Concatenate inline shader sources
        NSString *allShaderSources = [NSString stringWithFormat:@"%@%@%@",
                                     kRGB2YUV444Shader,
                                     kYUV444ToRGBAShader,
                                     kBilinearUpsampleShader];
                                     
        NSError *error = nil;
        // Create library from source
        self.library = [self.device newLibraryWithSource:allShaderSources
                                                options:nil
                                                  error:&error];
                                                  
        if (!self.library || error) {
            NSLog(@"Failed to create Metal library from source: %@", error);
            return nil;
        }

        // Pipeline creation follows...
        self.rgb2yuvPipeline = [self createPipelineStateWithFunctionName:@"rgb2yuv444_test"];
        self.yuv2rgbaPipeline = [self createPipelineStateWithFunctionName:@"yuv4442rgba"];
        self.bilinearUpsamplePipeline = [self createPipelineStateWithFunctionName:@"bilinear_upsample_2x"];
        
        if (!self.rgb2yuvPipeline || !self.yuv2rgbaPipeline || !self.bilinearUpsamplePipeline) {
            return nil;
        }
    }
    return self;
}

- (id<MTLComputePipelineState>)createPipelineStateWithSource:(NSString *)source functionName:(NSString *)name {
    NSError *error = nil;
    // Compile single shader source
    id<MTLLibrary> library = [self.device newLibraryWithSource:source options:nil error:&error];
    if (!library || error) {
        NSLog(@"Shader compile failed: %@, error: %@", name, error);
        return nil;
    }
    
    id<MTLFunction> function = [library newFunctionWithName:name];
    if (!function) {
        NSLog(@"Failed to get shader function %@", name);
        return nil;
    }
    
    id<MTLComputePipelineState> pipeline = [self.device newComputePipelineStateWithFunction:function error:&error];
    if (!pipeline || error) {
        NSLog(@"Failed to create pipeline state: %@, error: %@", name, error);
        return nil;
    }
    return pipeline;
}

- (id<MTLComputePipelineState>)createPipelineStateWithFunctionName:(NSString *)name {
    id<MTLFunction> function = [self.library newFunctionWithName:name];
    if (!function) {
        NSLog(@"Failed to get shader function %@", name);
        return nil;
    }
    
    NSError *error = nil;
    id<MTLComputePipelineState> pipeline = [self.device newComputePipelineStateWithFunction:function error:&error];
    if (!pipeline || error) {
        NSLog(@"Failed to create pipeline state: %@", error);
        return nil;
    }
    return pipeline;
}

// 2. Create RGB24 Metal texture (upload to GPU)
// Renamed: convertYUV444ToRGBATexture -> convertYUV444ToRGBATexture
- (id<MTLTexture>)convertYUV444ToRGBATexture:(id<MTLTexture>)yuv444Texture size:(ImageSize)size {
    if (!yuv444Texture || !self.yuv2rgbaPipeline) {
        NSLog(@"Invalid args or YUV->RGBA pipeline not initialized");
        return nil;
    }
    
    // 1. Create RGBA output texture (RGBA8Unorm)
    MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                  width:size.width
                                                                                 height:size.height
                                                                              mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    id<MTLTexture> rgbaTexture = [self.device newTextureWithDescriptor:desc];
    if (!rgbaTexture) {
        NSLog(@"Failed to create RGBA texture");
        return nil;
    }
    
    // 2. Create command buffer and encoder
    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];
    id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
    if (!encoder) {
        NSLog(@"Failed to create command encoder");
        return nil;
    }
    
    // 3. Set pipeline and textures
    [encoder setComputePipelineState:self.yuv2rgbaPipeline];
    [encoder setTexture:yuv444Texture atIndex:0];  // Input YUV444 texture
    [encoder setTexture:rgbaTexture atIndex:1];    // Output RGBA texture
    
    // 4. Threadgroup size (16x16 is optimal on Metal)
    int unit = 16;
    MTLSize threadGroupSize = MTLSizeMake(unit, unit, 1);
    MTLSize threadGridSize = MTLSizeMake((size.width + unit-1) / unit,
                                         (size.height + unit-1) / unit,
                                         1);
    
    // 5. Dispatch GPU threads
    [encoder dispatchThreads:threadGridSize threadsPerThreadgroup:threadGroupSize];
    
    // 6. Commit and wait
    [encoder endEncoding];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    
    NSLog(@"YUV444->RGBA texture conversion done");
    return rgbaTexture;
}


- (id<MTLTexture>)createRGBATextureWithData:(const unsigned char*)data pix_format:(MTLPixelFormat)pix_format size:(ImageSize)size  {
    // 1. Validate parameters
    if (!data || !self.device || size.width <= 0 || size.height <= 0) {
        NSLog(@"createRGBATextureWithData: invalid parameters");
        return nil;
    }

    // 2. Create RGBA texture descriptor (RGBA8Unorm, 4 bytes/pixel)
    MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pix_format
                                                                                  width:size.width
                                                                                 height:size.height
                                                                              mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;

    // 3. Create texture object
    id<MTLTexture> texture = [self.device newTextureWithDescriptor:desc];
    if (!texture) {
        NSLog(@"createRGB24TextureWithData: failed to create texture");
        return nil;
    }

    // 4. Metal minimum aligned bytesPerRow (iOS 12+ compat)
    NSUInteger bytes_per_pixel = 4; // RGBA8Unorm 4 bytes/pixel
    NSUInteger alignment = 256;
    NSUInteger base_bpr = size.width * bytes_per_pixel;
    NSUInteger aligned_bpr = ((base_bpr + alignment - 1) / alignment) * alignment;

    // 5. Allocate aligned buffer and copy data
    NSUInteger total_aligned_size = aligned_bpr * size.height;
    unsigned char* aligned_data = (unsigned char*)malloc(total_aligned_size);
    if (!aligned_data) {
        NSLog(@"createRGB24TextureWithData: memory allocation failed");
        return nil;
    }
    memset(aligned_data, 0, total_aligned_size); // Zero-init buffer

    // Copy RGBA row-by-row (avoid misalignment)
    for (NSInteger y = 0; y < size.height; y++) {
        memcpy(aligned_data + y * aligned_bpr,
               data + y * size.width * bytes_per_pixel,
               size.width * bytes_per_pixel);
    }

    // 6. Write data to texture
    MTLRegion region = MTLRegionMake2D(0, 0, size.width, size.height);
    [texture replaceRegion:region
              mipmapLevel:0
                withBytes:aligned_data
              bytesPerRow:aligned_bpr];

    // 7. Free temp memory
    free(aligned_data);

    //NSLog(@"createRGBATextureWithData: texture created (RGBA8Unorm, %lux%lu)", size.width, size.height);
    return texture;
}

typedef struct {
    float R;
    float G;
    float B;
    float Y;
    float U_raw;
    float U;
    float V_raw;
    float V;
} ShaderDebugData;

// 3. RGB24 texture to YUV444 texture
- (id<MTLTexture>)convertRGBAToYUV444Texture:(id<MTLTexture>)rgbTexture size:(ImageSize)size {
    if (!rgbTexture || !self.rgb2yuvPipeline) return nil;
    
    // Create YUV444 output texture (RGBA8: R=Y, G=U, B=V, A=1)
    MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                  width:size.width
                                                                                 height:size.height
                                                                              mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    id<MTLTexture> yuvTexture = [self.device newTextureWithDescriptor:desc];
    
    // Create command buffer
    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];
    id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
    
    // 2. Shared buffer (1 pixel debug data, expandable)
    NSUInteger bufferSize = sizeof(ShaderDebugData);
    id<MTLBuffer> debugBuffer = [self.device newBufferWithLength:bufferSize
                                                       options:MTLResourceStorageModeShared];
    // Initialize buffer
    memset(debugBuffer.contents, 0, bufferSize);
    
    // Set pipeline and textures
    [encoder setComputePipelineState:self.rgb2yuvPipeline];
    [encoder setTexture:rgbTexture atIndex:0];
    [encoder setTexture:yuvTexture atIndex:1];
    [encoder setBuffer:debugBuffer offset:0 atIndex:0];
    
    // Set threadgroup size
    int unit = 2;
   // MTLSize threadGroupSize = MTLSizeMake(16, 16, 1);
   // MTLSize threadGridSize = MTLSizeMake((size.width + 15)/16, (size.height + 15)/16, 1);
    // Fixed threadgroup size (16x16 Metal standard)
    MTLSize threadGroupSize = MTLSizeMake(unit, unit, 1);
    // Compute grid size (ceil to cover full image)
    MTLSize threadGridSize = MTLSizeMake(
        (size.width + unit - 1) / unit,  // Threadgroups in X
        (size.height + unit - 1) / unit, // Threadgroups in Y
        1
    );

    [encoder dispatchThreads:threadGridSize threadsPerThreadgroup:threadGroupSize];

    
    [encoder endEncoding];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    
    ShaderDebugData* debugData = (ShaderDebugData*)debugBuffer.contents;
    NSLog(@"Shader intermediate debug (pixel (120,0)):");
    NSLog(@"R = %.6f", debugData->R);
    NSLog(@"G = %.6f", debugData->G);
    NSLog(@"B = %.6f", debugData->B);
    NSLog(@"Y = %.6f", debugData->Y);
    NSLog(@"U_raw = %.6f", debugData->U_raw);
    NSLog(@"U = %.6f", debugData->U);
    NSLog(@"V_raw = %.6f", debugData->V_raw);
    NSLog(@"V = %.6f", debugData->V);
    
    return yuvTexture;
}

// 4. Process YUV444 texture (Y SR, UV upsample)
- (id<MTLTexture>)processYUV444Texture:(id<MTLTexture>)yuv444Texture
                             inputSize:(ImageSize)inputSize
                            outputSize:(ImageSize*)outputSize {
    // 1. Validate parameters
    if (!yuv444Texture || !outputSize || inputSize.width <= 0 || inputSize.height <= 0) {
        NSLog(@"processYUV444Texture: invalid parameters");
        return nil;
    }
    
    // 2. Download YUV444 texture from GPU (RGBA stores Y/U/V/A)
    unsigned char* yuvData = [self downloadYUV444Texture:yuv444Texture size:inputSize];
    if (!yuvData) {
        NSLog(@"processYUV444Texture: failed to download YUV444 data");
        return nil;
    }
    
    // 3. Split Y/U/V (discard A; YUV444 has no Alpha)
    NSUInteger inputPixelCount = inputSize.width * inputSize.height;
    unsigned char* y_data = (unsigned char*)malloc(inputPixelCount);
    unsigned char* u_data = (unsigned char*)malloc(inputPixelCount);
    unsigned char* v_data = (unsigned char*)malloc(inputPixelCount);
    if (!y_data || !u_data || !v_data) {
        NSLog(@"processYUV444Texture: failed to allocate Y/U/V memory");
        free(y_data); free(u_data); free(v_data); free(yuvData);
        return nil;
    }
    
    for (NSInteger i = 0; i < inputPixelCount; i++) {
        y_data[i] = yuvData[i*4];    // R channel -> Y
        u_data[i] = yuvData[i*4+1];  // G channel -> U
        v_data[i] = yuvData[i*4+2];  // B channel -> V
        // A channel (yuvData[i*4+3]) discarded
    }
    
    // 4. Bilinear upsample (Y/U/V each 2x)
    ImageSize yOutSize, uOutSize, vOutSize;
    unsigned char* y_out_data = bilinear_upsample(y_data, inputSize, &yOutSize);
    unsigned char* u_out_data = bilinear_upsample(u_data, inputSize, &uOutSize);
    unsigned char* v_out_data = bilinear_upsample(v_data, inputSize, &vOutSize);
    if (!y_out_data || !u_out_data || !v_out_data) {
        NSLog(@"processYUV444Texture: bilinear upsample failed");
        free(y_data); free(u_data); free(v_data);
        free(y_out_data); free(u_out_data); free(v_out_data);
        free(yuvData);
        return nil;
    }
    
    // 5. Verify upsample size (must be 2x input)
    BOOL sizeValid = (yOutSize.width == inputSize.width*2 && yOutSize.height == inputSize.height*2) &&
                     (uOutSize.width == inputSize.width*2 && uOutSize.height == inputSize.height*2) &&
                     (vOutSize.width == inputSize.width*2 && vOutSize.height == inputSize.height*2);
    if (!sizeValid) {
        NSLog(@"processYUV444Texture: upsample size mismatch");
        free(y_data); free(u_data); free(v_data);
        free(y_out_data); free(u_out_data); free(v_out_data);
        free(yuvData);
        return nil;
    }
    
    // 6. Pack upsampled Y/U/V as RGBA for Metal texture
    *outputSize = yOutSize;
    NSUInteger outputPixelCount = outputSize->width * outputSize->height;
    unsigned char* processedYUVData = (unsigned char*)malloc(outputPixelCount * 4);
    if (!processedYUVData) {
        NSLog(@"processYUV444Texture: failed to allocate output memory");
        free(y_data); free(u_data); free(v_data);
        free(y_out_data); free(u_out_data); free(v_out_data);
        free(yuvData);
        return nil;
    }
    
    // Fix: use full upsampled data directly, no bounds check
    for (NSInteger i = 0; i < outputPixelCount; i++) {
        processedYUVData[i*4]   = y_out_data[i];  // Y -> R channel
        processedYUVData[i*4+1] = u_out_data[i];  // U -> G channel
        processedYUVData[i*4+2] = v_out_data[i];  // V -> B channel
        processedYUVData[i*4+3] = 255;            // Pad A channel (unused)
    }
    
    // 7. Create Metal texture (RGBA8Unorm)
    id<MTLTexture> processedTexture = [self createRGBATextureWithData:processedYUVData pix_format:MTLPixelFormatRGBA8Unorm size:*outputSize];
    
    // 8. Free all temp memory (prevent leaks)
    free(y_data);
    free(u_data);
    free(v_data);
    free(y_out_data);
    free(u_out_data);
    free(v_out_data);
    free(yuvData);
    free(processedYUVData);
    
    NSLog(@"processYUV444Texture: done, input %lux%lu -> output %lux%lu",
          inputSize.width, inputSize.height,
          outputSize->width, outputSize->height);
    return processedTexture;
}

// Download RGBA texture to CPU (4 bytes/pixel)
/**
 * Fixed: download CPU-side RGBA from Metal RGBA texture (fixes misalignment/black output)
 * @param rgbaTexture Metal RGBA texture (MTLPixelFormatRGBA8Unorm)
 * @param size Texture dimensions
 * @return Linear RGBA data (4 bytes/pixel, no row padding, length=width*height*4), caller must free; NULL on failure
 */
- (unsigned char*)downloadRGBATexture:(id<MTLTexture>)rgbaTexture size:(ImageSize)size {
    // 1. Validate parameters
    if (!rgbaTexture || size.width <= 0 || size.height <= 0) {
        NSLog(@"downloadRGBATexture: invalid parameters");
        return NULL;
    }

    // 2. Get alignment requirements (legacy compat)
    NSUInteger bytes_per_pixel = 4; // RGBA8Unorm
    NSUInteger valid_bpr = size.width * bytes_per_pixel;
    NSUInteger alignment = 256; // Default min alignment is 256 bytes on most devices
    if ([rgbaTexture.device respondsToSelector:@selector(minimumLinearTextureAlignmentForPixelFormat:)]) {
        alignment = [rgbaTexture.device minimumLinearTextureAlignmentForPixelFormat:rgbaTexture.pixelFormat];
    }
    NSUInteger aligned_bpr = ((valid_bpr + alignment - 1) / alignment) * alignment;

    NSLog(@"aligned_bpr = %lu, valid_bpr = %lu", aligned_bpr, valid_bpr);

    // 3. Allocate aligned temp buffer
    NSUInteger aligned_total_size = aligned_bpr * size.height;
    unsigned char* aligned_rgba_data = (unsigned char*)malloc(aligned_total_size);
    if (!aligned_rgba_data) {
        NSLog(@"downloadRGBATexture: aligned buffer allocation failed");
        return NULL;
    }
    memset(aligned_rgba_data, 0, aligned_total_size);

    // 4. Read from Metal texture (must use aligned_bpr)
    MTLRegion region = MTLRegionMake2D(0, 0, size.width, size.height);
    [rgbaTexture getBytes:aligned_rgba_data
             bytesPerRow:aligned_bpr
               fromRegion:region
              mipmapLevel:0];

    // 5. Extract valid data (strip row padding)
    NSUInteger valid_total_size = valid_bpr * size.height;
    unsigned char* valid_rgba_data = (unsigned char*)malloc(valid_total_size);
    if (!valid_rgba_data) {
        NSLog(@"downloadRGBATexture: valid buffer allocation failed");
        free(aligned_rgba_data);
        return NULL;
    }
    for (NSUInteger y = 0; y < size.height; y++) {
        const unsigned char* src_row = aligned_rgba_data + y * aligned_bpr;
        unsigned char* dst_row = valid_rgba_data + y * valid_bpr;
        memcpy(dst_row, src_row, valid_bpr);
    }

    // 6. Free temp buffer
    free(aligned_rgba_data);
    return valid_rgba_data;
}

// Download YUV444 texture data (R=Y, G=U, B=V, A=1)
- (unsigned char*)downloadYUV444Texture:(id<MTLTexture>)yuv444Texture size:(ImageSize)size {
    // 1. Validate parameters
    if (!yuv444Texture || size.width <= 0 || size.height <= 0) {
        NSLog(@"downloadYUV444Texture: invalid parameters");
        return NULL;
    }
    if (yuv444Texture.pixelFormat != MTLPixelFormatRGBA8Unorm) {
        NSLog(@"downloadYUV444Texture: texture format must be RGBA8Unorm");
        return NULL;
    }

    // 2. Get correct aligned bytesPerRow
    NSUInteger aligned_bpr = 0;
    if ([yuv444Texture respondsToSelector:@selector(minimumRequiredBytesPerRow)]) {
        //aligned_bpr = yuv444Texture.minimumRequiredBytesPerRow;
    } else {
        NSUInteger valid_bpr = size.width * 4;
        NSUInteger alignment = [yuv444Texture.device minimumLinearTextureAlignmentForPixelFormat:yuv444Texture.pixelFormat];
        aligned_bpr = ((valid_bpr + alignment - 1) / alignment) * alignment;
    }
    NSLog(@"aligned_bpr = %lu, texture size = %lux%lu", aligned_bpr, yuv444Texture.width, yuv444Texture.height);

    // 3. Allocate aligned buffer (must be large enough)
    NSUInteger aligned_buf_size = aligned_bpr * size.height;
    unsigned char* aligned_yuv_data = (unsigned char*)malloc(aligned_buf_size);
    if (!aligned_yuv_data) {
        NSLog(@"downloadYUV444Texture: buffer allocation failed");
        return NULL;
    }
    memset(aligned_yuv_data, 0, aligned_buf_size); // Zero-init to avoid garbage

    // 4. Read texture (params must be exact)
    MTLRegion region = MTLRegionMake2D(0, 0, size.width, size.height);
    NSError *readError = nil;
    // Debug: print texture info
    NSLog(@"Read region: %lux%lu, mipmapLevel=0", region.size.width, region.size.height);
    [yuv444Texture getBytes:aligned_yuv_data
             bytesPerRow:aligned_bpr
               fromRegion:region
              mipmapLevel:0];

    // 5. Verify read (print first 16 bytes)
    NSLog(@"aligned_yuv_data first 16 bytes:");
    for (int i=0; i<16; i++) {
        NSLog(@"byte %d: %d", i, aligned_yuv_data[i]);
    }
    // When shader outputs Y=0.299(76), U=0.35287(89), V=0.907(231),
    // first 4 bytes should be 76,89,231,255 (R=Y, G=U, B=V, A=255)

    // 6. Extract YUV (drop A channel)
    NSUInteger pixel_count = size.width * size.height;
    unsigned char* yuv444_data = (unsigned char*)malloc(pixel_count * 3);
    if (!yuv444_data) {
        free(aligned_yuv_data);
        return NULL;
    }
    NSUInteger valid_bpr = size.width * 4;
    for (NSUInteger y=0; y<size.height; y++) {
        const unsigned char* src_row = aligned_yuv_data + y * aligned_bpr;
        for (NSUInteger x=0; x<size.width; x++) {
            NSUInteger idx = y * size.width + x;
            // Channel map: R=Y, G=U, B=V
            yuv444_data[idx] = src_row[x*4];                          // Y
            yuv444_data[pixel_count + idx] = src_row[x*4+1];          // U
            yuv444_data[pixel_count * 2 + idx] = src_row[x*4+2];      // V
        }
    }

    // 7. Free temp buffer
    free(aligned_yuv_data);
    return yuv444_data;
}

@end

#pragma mark - Test entry (example)
unsigned char* test_yuv_metal_process(const unsigned char* yuv420_data, int width, int height) {
    
    ImageSize input_size;
    input_size.width = width;
    input_size.height = height;
    // 1. YUV420 to YUV444
    unsigned char* yuv444_data = yuv420_to_yuv444_cpu(yuv420_data, input_size);
#if 0
    FILE* fp = fopen("yuv444_data.yuv", "wb");
    fwrite(yuv444_data, sizeof(char), width*height*3, fp);
    fflush(fp);
#endif
    // 2. YUV444 to RGB24 + upload to GPU
    unsigned char* rgba_data = yuv444_to_rgba_cpu(yuv444_data, input_size);
#if 0
    FILE* fp = fopen("rgbau8.rgb", "wb");
    fwrite(rgba_data, sizeof(char), width*height*4, fp);
    fflush(fp);
#endif
    //---------------------------------------------------------------------
    
    YUVMetalProcessor *processor = [[YUVMetalProcessor alloc] init];
    id<MTLTexture> rgbTexture = [processor createRGBATextureWithData:rgba_data pix_format:MTLPixelFormatRGBA8Unorm size:input_size];
    
    // 3. RGB24 texture to YUV444 texture
    id<MTLTexture> yuv444Texture = [processor convertRGBAToYUV444Texture:rgbTexture size:input_size];
#if 1
    unsigned char* yuv444Data = [processor downloadYUV444Texture:yuv444Texture size:input_size];
    
    //unsigned char* yuv444Data_2 = rgba_to_yuv444_cpu(rgba_data, input_size);
    FILE* fp = fopen("yuv444_data.yuv", "wb");
    fwrite(yuv444Data, sizeof(char), width*height*3, fp);
    fflush(fp);
    fclose(fp);
    
    unsigned char* rgba = yuv444_to_rgba_cpu(yuv444Data, input_size);
    
    fp = fopen("rgba-cpu.rgb", "wb");
    fwrite(rgba, sizeof(char), width*height*4, fp);
    fflush(fp);
    fclose(fp);
#endif
    // 4. Process YUV444 texture (Y SR, UV upsample)
    ImageSize output_size;
#if 0
    id<MTLTexture> processedYUVTexture = [processor processYUV444Texture:yuv444Texture
                                                             inputSize:input_size
                                                            outputSize:&output_size];
#if 1
    unsigned char* yuv444Data = [processor downloadYUV444Texture:processedYUVTexture size:output_size];
    FILE* fp = fopen("yuv444_data.yuv", "wb");
    fwrite(yuv444_data, sizeof(char), output_size.width*output_size.height*3, fp);
    fflush(fp);
#endif
#else
    output_size.width = input_size.width;
    output_size.height = input_size.height;
    id<MTLTexture> processedYUVTexture = yuv444Texture;
#endif
    
    // 5. Processed YUV444 to RGB24 texture
    id<MTLTexture> processedRGBTexture = [processor convertYUV444ToRGBATexture:yuv444Texture size:output_size];
    
    // 6. Download RGB24 texture to CPU + convert to YUV420
    unsigned char* downloadedRGB = [processor downloadRGBATexture:processedRGBTexture size:output_size];
#if 0
    FILE* fp = fopen("rgbau8-2.rgb", "wb");
    fwrite(downloadedRGB, sizeof(char), output_size.width*output_size.height*4, fp);
    fflush(fp);
#endif
    unsigned char* YUV444 = rgba_to_yuv444_cpu(downloadedRGB, output_size);
#if 0
    FILE* fp = fopen("yuv444_data.yuv", "wb");
    fwrite(YUV444, sizeof(char), output_size.width*output_size.height*3, fp);
    fflush(fp);
#endif
    
    unsigned char* finalYUV420 = yuv444_to_yuv420_cpu(YUV444, output_size);
    
    // Free memory (example)
    free(yuv444_data);
    free(rgba_data);
    free(downloadedRGB);
    
    return finalYUV420;

}
