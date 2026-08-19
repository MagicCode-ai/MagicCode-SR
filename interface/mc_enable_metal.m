#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <CoreFoundation/CoreFoundation.h>
#include <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Set by mc_enable.c so a stolen release can clear the cached pointer before abort. */
void mc_enable_note_output_stolen(void* output);

static int g_metal_legit_release = 0;
static char kMCEnableOutputGuardKey;

@interface MCEnableOutputGuard : NSObject
@property (nonatomic, assign) void* tracked;
@end

@implementation MCEnableOutputGuard
- (void)dealloc
{
    if (!g_metal_legit_release)
    {
        /*
         * Caller CFRelease'd / over-released the texture returned by MC_Enable.
         * Abort here so the fault is at the bad release site, not the next MC_Enable.
         */
        fprintf(stderr,
                "[MagicSR] FATAL: MC_Enable output texture was released by the caller. "
                "Do not free/CFRelease the pointer; call MC_Disable() instead.\n");
        mc_enable_note_output_stolen(self.tracked);
        abort();
    }
}
@end

int mc_enable_query_texture_size(void* texture, unsigned int* width, unsigned int* height)
{
    id<MTLTexture> tex = (__bridge id<MTLTexture>)texture;
    if (tex == nil || width == NULL || height == NULL)
    {
        return -1;
    }
    if (tex.width == 0 || tex.height == 0)
    {
        return -1;
    }
    *width = (unsigned int)tex.width;
    *height = (unsigned int)tex.height;
    return 0;
}

int mc_enable_resolve_bundle_model(char* out_path, size_t out_size)
{
    static const char* kNames[] = {
        "magic_veryfast_gpu_params.bin",
        "magic_metal_speed_gpu_params.bin",
        "magic_speed_gpu_params.bin",
    };
    NSBundle* bundle = [NSBundle mainBundle];
    size_t i;

    if (out_path == NULL || out_size == 0)
    {
        return -1;
    }
    out_path[0] = '\0';

    for (i = 0; i < sizeof(kNames) / sizeof(kNames[0]); ++i)
    {
        NSString* name = [NSString stringWithUTF8String:kNames[i]];
        NSString* path = [bundle pathForResource:[name stringByDeletingPathExtension]
                                          ofType:[name pathExtension]];
        if (path == nil)
        {
            path = [bundle pathForResource:name ofType:nil];
        }
        if (path == nil)
        {
            continue;
        }
        if ((size_t)path.length + 1 > out_size)
        {
            continue;
        }
        memcpy(out_path, path.UTF8String, (size_t)path.length + 1);
        return 0;
    }
    return -1;
}

void* mc_enable_metal_acquire_output(void* input_texture, unsigned int out_w, unsigned int out_h, int prefer_r8)
{
    id<MTLTexture> input;
    id<MTLDevice> device;
    MTLPixelFormat format;
    MTLTextureDescriptor* desc;
    id<MTLTexture> tex;
    MCEnableOutputGuard* guard;
    void* out;

    if (input_texture == NULL || out_w == 0 || out_h == 0)
    {
        return NULL;
    }

    /* Same pattern as GLES: create output on the caller's GPU device (from input). */
    input = (__bridge id<MTLTexture>)input_texture;
    device = input.device;
    if (device == nil)
    {
        return NULL;
    }

    format = prefer_r8 ? MTLPixelFormatR8Unorm : MTLPixelFormatRGBA8Unorm;
    desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                              width:out_w
                                                             height:out_h
                                                          mipmapped:NO];
    /* Shared so callers (e.g. camera demo UIImage readback) can getBytes safely. */
    desc.storageMode = MTLStorageModeShared;
    desc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
    tex = [device newTextureWithDescriptor:desc];
    if (tex == nil)
    {
        return NULL;
    }

    /*
     * Library owns the texture (CFRetain). Return a borrowed pointer.
     * Caller must NOT CFRelease/free it; doing so triggers abort in the guard.
     */
    out = (__bridge void*)tex;
    CFRetain(out);

    guard = [MCEnableOutputGuard new];
    guard.tracked = out;
    objc_setAssociatedObject(tex, &kMCEnableOutputGuardKey, guard, OBJC_ASSOCIATION_RETAIN_NONATOMIC);

    return out;
}

void mc_enable_metal_release_output(void* output)
{
    id<MTLTexture> tex;

    if (output == NULL)
    {
        return;
    }

    tex = (__bridge id<MTLTexture>)output;
    g_metal_legit_release = 1;
    objc_setAssociatedObject(tex, &kMCEnableOutputGuardKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    CFRelease(output);
    g_metal_legit_release = 0;
}
