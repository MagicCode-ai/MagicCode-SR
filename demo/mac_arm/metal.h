#ifndef TEST_METAL_H
#define TEST_METAL_H
unsigned char*  test_yuv_metal_process(const unsigned char* yuv420_data, int width, int height);

void* gen_rgba_texture(unsigned char *rgba_data, int texture_type, int width, int height);

unsigned char* download_rgba_texture(void* in_texture, int width, int height);

void *down_private_texture_to_cpu(void *privateTexture);

void* creat_texture2d(int width, int height, int texture_type, int usage);

void release_texture(void *p_texture);
#endif
