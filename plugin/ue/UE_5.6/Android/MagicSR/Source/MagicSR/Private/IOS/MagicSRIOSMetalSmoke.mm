#if MAGIC_SR_IOS

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "MagicSRSmokePng.h"

extern "C" {
#include "mc_interface.h"
#include "mc_enable.h"
void* get_device(void* handle);
}

namespace
{
constexpr int SmokeWidth = 64;
constexpr int SmokeHeight = 64;
constexpr int SmokeScale = 2;

id<MTLTexture> CreateRgbaTexture(id<MTLDevice> Device, int Width, int Height, const uint8_t* RgbaOrNull)
{
    MTLTextureDescriptor* Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                    width:Width
                                                                                   height:Height
                                                                                mipmapped:NO];
    Desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
    id<MTLTexture> Texture = [Device newTextureWithDescriptor:Desc];
    if (Texture != nil && RgbaOrNull != nullptr)
    {
        [Texture replaceRegion:MTLRegionMake2D(0, 0, Width, Height)
                   mipmapLevel:0
                     withBytes:RgbaOrNull
                   bytesPerRow:Width * 4];
    }
    return Texture;
}

void FillSyntheticRgba(uint8_t* Rgba, int Width, int Height)
{
    for (int Y = 0; Y < Height; ++Y)
    {
        for (int X = 0; X < Width; ++X)
        {
            const size_t Index = (static_cast<size_t>(Y) * Width + X) * 4;
            Rgba[Index] = static_cast<uint8_t>((X * 3 + Y * 5) & 0xff);
            Rgba[Index + 1] = static_cast<uint8_t>((X * 7) & 0xff);
            Rgba[Index + 2] = static_cast<uint8_t>((Y * 9) & 0xff);
            Rgba[Index + 3] = 255;
        }
    }
}

bool ReadRgbaTexture(id<MTLTexture> Texture, uint8_t* Rgba, int Width, int Height)
{
    if (Texture == nil || Rgba == nullptr || Width <= 0 || Height <= 0)
    {
        return false;
    }
    [Texture getBytes:Rgba bytesPerRow:Width * 4 fromRegion:MTLRegionMake2D(0, 0, Width, Height) mipmapLevel:0];
    return true;
}

unsigned int CountNonZeroRgba(const uint8_t* Rgba, int Width, int Height)
{
    unsigned int NonZero = 0;
    const size_t Bytes = static_cast<size_t>(Width) * Height * 4;
    for (size_t I = 0; I < Bytes; ++I)
    {
        if (Rgba[I] != 0)
        {
            ++NonZero;
        }
    }
    return NonZero;
}

}  // namespace

extern "C" int MagicSR_RunIOSMetalSmoke(const char* ModelPath, const char* OutputDir, char* OutReport, size_t OutReportSize)
{
    if (ModelPath == nullptr || OutputDir == nullptr || OutReport == nullptr || OutReportSize == 0)
    {
        return -1;
    }

    input_param_t Params = {};
    Params.input_type = INPUT_TEXTURE_R8Unorm;
    strncpy(Params.model_path, ModelPath, sizeof(Params.model_path) - 1);
    Params.width = SmokeWidth;
    Params.height = SmokeHeight;
    Params.scaler_factor = SmokeScale;
    Params.alg_mode = HIGH_SPEED_MODE;
    Params.num_threads = 1;
    Params.log_level = MAGIC_LOG_INFO;
    Params.backend = MAGIC_BACKEND_METAL;

    void* Handle = MC_Init(&Params);
    if (Handle == nullptr)
    {
        snprintf(OutReport, OutReportSize, "api=CreateSession/Process backend=metal step=create ret=-101 model=%s", ModelPath);
        return -1;
    }

    output_status_params_t Status = {};
    const int QueryRet = MC_Control(Handle, QUERY_STATUS, nullptr, &Status);
    // Prefer the system device (same as Enable smoke). get_device() has returned a
    // non-nil but unusable pointer on some iOS 26 / UE builds and SIGSEGV'd on
    // newTextureWithDescriptor.
    id<MTLDevice> Device = MTLCreateSystemDefaultDevice();
    if (Device == nil)
    {
        Device = (__bridge id<MTLDevice>)get_device(Handle);
    }
    if (Device == nil)
    {
        MC_Uninit(Handle);
        snprintf(OutReport, OutReportSize, "api=CreateSession/Process backend=metal step=device ret=-102");
        return -1;
    }

    uint8_t Input[SmokeWidth * SmokeHeight] = {};
    for (int Y = 0; Y < SmokeHeight; ++Y)
    {
        for (int X = 0; X < SmokeWidth; ++X)
        {
            Input[Y * SmokeWidth + X] = static_cast<uint8_t>((X * 3 + Y * 5) & 0xff);
        }
    }

    MTLTextureDescriptor* InputDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                         width:SmokeWidth
                                                                                        height:SmokeHeight
                                                                                     mipmapped:NO];
    InputDesc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    id<MTLTexture> InputTexture = [Device newTextureWithDescriptor:InputDesc];
    [InputTexture replaceRegion:MTLRegionMake2D(0, 0, SmokeWidth, SmokeHeight)
                    mipmapLevel:0
                      withBytes:Input
                    bytesPerRow:SmokeWidth];

    MTLTextureDescriptor* OutputDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                          width:SmokeWidth * SmokeScale
                                                                                         height:SmokeHeight * SmokeScale
                                                                                      mipmapped:NO];
    OutputDesc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
    id<MTLTexture> OutputTexture = [Device newTextureWithDescriptor:OutputDesc];

    const int ProcessRet = MC_Process(Handle, (__bridge void*)InputTexture, (__bridge void*)OutputTexture);
    output_status_params_t StatusAfter = {};
    const int QueryAfterRet = MC_Control(Handle, QUERY_STATUS, nullptr, &StatusAfter);

    const int OutW = SmokeWidth * SmokeScale;
    const int OutH = SmokeHeight * SmokeScale;
    uint8_t* Output = static_cast<uint8_t*>(calloc(static_cast<size_t>(OutW * OutH), 1));
    if (Output != nullptr)
    {
        [OutputTexture getBytes:Output bytesPerRow:OutW fromRegion:MTLRegionMake2D(0, 0, OutW, OutH) mipmapLevel:0];
    }

    mkdir(OutputDir, 0775);
    char InputPath[512] = {};
    char OutputPath[512] = {};
    snprintf(InputPath, sizeof(InputPath), "%s/input_metal_64x64.png", OutputDir);
    snprintf(OutputPath, sizeof(OutputPath), "%s/output_metal_128x128.png", OutputDir);
    const bool WroteInput = MagicSRSmokePng::WriteR8AsRgbaPng(InputPath, Input, SmokeWidth, SmokeHeight);
    const bool WroteOutput = Output != nullptr && MagicSRSmokePng::WriteR8AsRgbaPng(OutputPath, Output, OutW, OutH);

    unsigned int NonZero = 0;
    if (Output != nullptr)
    {
        for (int I = 0; I < OutW * OutH; ++I)
        {
            if (Output[I] != 0)
            {
                ++NonZero;
            }
        }
    }

    const int DestroyRet = MC_Uninit(Handle);
    const bool Pass = QueryRet == 0 && ProcessRet == 0 && QueryAfterRet == 0 && StatusAfter.error_code == 0 &&
                      Status.output_width == OutW && Status.output_height == OutH && NonZero > 0 && WroteInput && WroteOutput;

    snprintf(OutReport,
             OutReportSize,
             "api=CreateSession/Process backend=metal ret=%d query_ret=%d query_after=%d destroy_ret=%d output=%ux%u nonZero=%u wrote_input=%d wrote_output=%d output_path=%s err=0x%llx gpu_time=%f",
             ProcessRet,
             QueryRet,
             QueryAfterRet,
             DestroyRet,
             Status.output_width,
             Status.output_height,
             NonZero,
             WroteInput ? 1 : 0,
             WroteOutput ? 1 : 0,
             OutputPath,
             static_cast<unsigned long long>(StatusAfter.error_code),
             StatusAfter.gpu_time);

    free(Output);
    return Pass ? 0 : -1;
}

extern "C" int MagicSR_RunIOSEnableSmoke(const char* ModelPath, char* OutReport, size_t OutReportSize)
{
    constexpr int EnableW = 64;
    constexpr int EnableH = 64;
    constexpr float EnableScale = 2.0f;
    constexpr int OutW = EnableW * 2;
    constexpr int OutH = EnableH * 2;

    if (OutReport == nullptr || OutReportSize == 0)
    {
        return -1;
    }
    if (ModelPath == nullptr || ModelPath[0] == '\0')
    {
        snprintf(OutReport, OutReportSize, "api=Enable ret=-1 step=model_missing");
        return -1;
    }

    MC_Enable_SetModelPath(ModelPath);

    id<MTLDevice> Device = MTLCreateSystemDefaultDevice();
    if (Device == nil)
    {
        snprintf(OutReport, OutReportSize, "api=Enable ret=-102 step=device");
        return -1;
    }

    uint8_t Input[EnableW * EnableH * 4] = {};
    FillSyntheticRgba(Input, EnableW, EnableH);

    id<MTLTexture> InputTexture = CreateRgbaTexture(Device, EnableW, EnableH, Input);
    if (InputTexture == nil)
    {
        snprintf(OutReport, OutReportSize, "api=Enable ret=-106 step=texture");
        return -1;
    }

    void* InputPtr = (__bridge void*)InputTexture;

    // MC_Enable
    void* OutEnable1 = MC_Enable(InputPtr, EnableScale);
    void* OutEnable2 = MC_Enable(InputPtr, EnableScale);
    const bool EnableOk = OutEnable1 != nullptr;
    const bool EnableReused = EnableOk && OutEnable2 == OutEnable1;
    MC_Disable(nullptr);
    void* OutEnable3 = MC_Enable(InputPtr, EnableScale);
    const bool EnableReenabled = OutEnable3 != nullptr;

    uint8_t* EnableOutRgba = static_cast<uint8_t*>(calloc(static_cast<size_t>(OutW * OutH * 4), 1));
    bool EnableReadback = false;
    unsigned int EnableNonZero = 0;
    if (EnableReenabled && EnableOutRgba != nullptr)
    {
        EnableReadback = ReadRgbaTexture((__bridge id<MTLTexture>)OutEnable3, EnableOutRgba, OutW, OutH);
        EnableNonZero = CountNonZeroRgba(EnableOutRgba, OutW, OutH);
    }
    MC_Disable(nullptr);

    // MC_Enable_3params
    void* OutMode = MC_Enable_3params(InputPtr, EnableScale, HIGH_SPEED_MODE);
    const bool ModeOk = OutMode != nullptr;
    uint8_t* ModeOutRgba = static_cast<uint8_t*>(calloc(static_cast<size_t>(OutW * OutH * 4), 1));
    bool ModeReadback = false;
    unsigned int ModeNonZero = 0;
    if (ModeOk && ModeOutRgba != nullptr)
    {
        ModeReadback = ReadRgbaTexture((__bridge id<MTLTexture>)OutMode, ModeOutRgba, OutW, OutH);
        ModeNonZero = CountNonZeroRgba(ModeOutRgba, OutW, OutH);
    }
    MC_Disable(nullptr);

    // MC_Enable_4params (explicit Metal backend)
    void* OutEx = MC_Enable_4params(InputPtr, EnableScale, HIGH_SPEED_MODE, MAGIC_BACKEND_METAL);
    const bool ExOk = OutEx != nullptr;
    uint8_t* ExOutRgba = static_cast<uint8_t*>(calloc(static_cast<size_t>(OutW * OutH * 4), 1));
    bool ExReadback = false;
    unsigned int ExNonZero = 0;
    if (ExOk && ExOutRgba != nullptr)
    {
        ExReadback = ReadRgbaTexture((__bridge id<MTLTexture>)OutEx, ExOutRgba, OutW, OutH);
        ExNonZero = CountNonZeroRgba(ExOutRgba, OutW, OutH);
    }
    MC_Disable(nullptr);

    NSString* Docs = [NSHomeDirectory() stringByAppendingPathComponent:@"Documents/MagicSRSmoke"];
    [[NSFileManager defaultManager] createDirectoryAtPath:Docs withIntermediateDirectories:YES attributes:nil error:nil];
    const char* OutputDir = Docs.fileSystemRepresentation;

    char InputPath[512] = {};
    char EnableOutPath[512] = {};
    char ModeOutPath[512] = {};
    char ExOutPath[512] = {};
    snprintf(InputPath, sizeof(InputPath), "%s/input_enable_64x64.png", OutputDir);
    snprintf(EnableOutPath, sizeof(EnableOutPath), "%s/output_enable_128x128.png", OutputDir);
    snprintf(ModeOutPath, sizeof(ModeOutPath), "%s/output_enable_3params_128x128.png", OutputDir);
    snprintf(ExOutPath, sizeof(ExOutPath), "%s/output_enable_4params_128x128.png", OutputDir);

    const bool WroteInput = MagicSRSmokePng::WriteRgbaPng(InputPath, Input, EnableW, EnableH);
    const bool WroteEnable =
        EnableReadback && MagicSRSmokePng::WriteRgbaPng(EnableOutPath, EnableOutRgba, OutW, OutH);
    const bool WroteMode = ModeReadback && MagicSRSmokePng::WriteRgbaPng(ModeOutPath, ModeOutRgba, OutW, OutH);
    const bool WroteEx = ExReadback && MagicSRSmokePng::WriteRgbaPng(ExOutPath, ExOutRgba, OutW, OutH);

    free(EnableOutRgba);
    free(ModeOutRgba);
    free(ExOutRgba);

    const bool Pass = EnableOk && EnableReused && EnableReenabled && EnableReadback && EnableNonZero > 0 &&
                      ModeOk && ModeReadback && ModeNonZero > 0 && ExOk && ExReadback && ExNonZero > 0 && WroteInput &&
                      WroteEnable && WroteMode && WroteEx;

    snprintf(OutReport,
             OutReportSize,
             "api=Enable,Enable_3params,Enable_4params enable=%d reused=%d reenabled=%d mode=%d ex=%d "
             "nonZero=%u/%u/%u wrote=%d/%d/%d/%d in=%dx%d scale=%.1f format=rgba8",
             EnableOk ? 1 : 0,
             EnableReused ? 1 : 0,
             EnableReenabled ? 1 : 0,
             ModeOk ? 1 : 0,
             ExOk ? 1 : 0,
             EnableNonZero,
             ModeNonZero,
             ExNonZero,
             WroteInput ? 1 : 0,
             WroteEnable ? 1 : 0,
             WroteMode ? 1 : 0,
             WroteEx ? 1 : 0,
             EnableW,
             EnableH,
             EnableScale);
    return Pass ? 0 : -1;
}

// Auto-run is scheduled from FMagicSRModule::StartupModule (not a native constructor).
// Early constructors race with UE Metal init and previously SIGSEGV'd on device.

#endif  // MAGIC_SR_IOS
