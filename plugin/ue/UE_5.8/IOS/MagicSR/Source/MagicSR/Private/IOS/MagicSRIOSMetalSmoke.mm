#if MAGIC_SR_IOS

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

extern "C" {
#include "mc_interface.h"
void* get_device(void* handle);
}

namespace
{
constexpr int SmokeWidth = 64;
constexpr int SmokeHeight = 64;
constexpr int SmokeScale = 2;

bool WriteGrayPgm(const char* Path, const uint8_t* Data, int Width, int Height)
{
    FILE* File = fopen(Path, "wb");
    if (File == nullptr)
    {
        return false;
    }

    fprintf(File, "P5\n%d %d\n255\n", Width, Height);
    const size_t Written = fwrite(Data, 1, static_cast<size_t>(Width * Height), File);
    fclose(File);
    return Written == static_cast<size_t>(Width * Height);
}

bool SmokeAutoRunRequested()
{
    NSString* BundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];
    if (![BundleIdentifier isEqualToString:@"com.magicsr.uesmoke"])
    {
        return false;
    }

    NSString* CommandLinePath = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"uecommandline.txt"];
    NSString* CommandLine = [NSString stringWithContentsOfFile:CommandLinePath encoding:NSUTF8StringEncoding error:nil];
    return [CommandLine containsString:@"-MagicSRUESmoke"];
}

NSString* FirstExistingPath(NSArray<NSString*>* Paths)
{
    NSFileManager* FileManager = [NSFileManager defaultManager];
    for (NSString* Path in Paths)
    {
        if ([FileManager fileExistsAtPath:Path])
        {
            return Path;
        }
    }
    return [Paths firstObject];
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
        snprintf(OutReport, OutReportSize, "backend=metal step=create ret=-101 model=%s", ModelPath);
        return -1;
    }

    output_status_params_t Status = {};
    const int QueryRet = MC_Control(Handle, QUERY_STATUS, nullptr, &Status);
    id<MTLDevice> Device = (__bridge id<MTLDevice>)get_device(Handle);
    if (Device == nil)
    {
        Device = MTLCreateSystemDefaultDevice();
    }
    if (Device == nil)
    {
        MC_Uninit(Handle);
        snprintf(OutReport, OutReportSize, "backend=metal step=device ret=-102");
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
    snprintf(InputPath, sizeof(InputPath), "%s/input_metal_64x64.pgm", OutputDir);
    snprintf(OutputPath, sizeof(OutputPath), "%s/output_metal_128x128.pgm", OutputDir);
    const bool WroteInput = WriteGrayPgm(InputPath, Input, SmokeWidth, SmokeHeight);
    const bool WroteOutput = Output != nullptr && WriteGrayPgm(OutputPath, Output, OutW, OutH);

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
             "backend=metal ret=%d query_ret=%d query_after=%d destroy_ret=%d output=%ux%u nonZero=%u wrote_input=%d wrote_output=%d output_path=%s err=0x%llx gpu_time=%f",
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

__attribute__((constructor)) static void MagicSR_RunIOSMetalSmokeConstructor()
{
    @autoreleasepool
    {
        if (!SmokeAutoRunRequested())
        {
            return;
        }

        NSArray<NSString*>* DocumentDirectories =
            NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        NSString* DocumentsPath = [DocumentDirectories firstObject];
        NSString* BundleModelPath =
            [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"magic_veryfast_gpu_params.bin"];
        NSString* DocumentsModelPath =
            [DocumentsPath stringByAppendingPathComponent:@"MagicSRModels/magic_veryfast_gpu_params.bin"];
        NSString* FlatModelPath = [DocumentsPath stringByAppendingPathComponent:@"magic_veryfast_gpu_params.bin"];
        NSString* ModelPath = FirstExistingPath(@[ DocumentsModelPath, FlatModelPath, BundleModelPath ]);
        NSString* OutputDir = [DocumentsPath stringByAppendingPathComponent:@"MagicSRSmoke"];

        [[NSFileManager defaultManager] createDirectoryAtPath:OutputDir
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:nil];

        char Report[1024] = {};
        const int Ret = MagicSR_RunIOSMetalSmoke([ModelPath fileSystemRepresentation],
                                                [OutputDir fileSystemRepresentation],
                                                Report,
                                                sizeof(Report));
        if (Ret == 0)
        {
            NSLog(@"[MagicSRUESmoke] result=PASS %s", Report);
            exit(0);
        }

        NSLog(@"[MagicSRUESmoke] result=FAIL %s", Report);
        exit(1);
    }
}

#endif  // MAGIC_SR_IOS
