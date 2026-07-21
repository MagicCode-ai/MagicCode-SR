#include "MagicSRBlueprintLibrary.h"

#include "Containers/Map.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/CriticalSection.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/CString.h"
#include "Misc/ScopeLock.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "TextureResource.h"

#ifndef MAGIC_SR_IOS
#define MAGIC_SR_IOS 0
#endif

#define MAGIC_SR_SUPPORTED (MAGIC_SR_ANDROID || MAGIC_SR_IOS)

#if MAGIC_SR_ANDROID
#include <android/log.h>

extern "C" int MagicSR_RunAndroidBackendSmoke(const char* ModelPath, const char* OutputDir, char* OutReport, size_t OutReportSize);
extern "C" int MagicSR_RunAndroidEnableSmoke(const char* ModelPath, char* OutReport, size_t OutReportSize);
#endif

#if MAGIC_SR_SUPPORTED
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#if MAGIC_SR_IOS
#include <mach-o/dyld.h>
#include <string.h>
#endif

extern "C" {
#include "mc_interface.h"
#include "mc_enable.h"
}
#endif

namespace
{
FCriticalSection GSessionMutex;
TMap<int32, void*> GSessions;
int32 GNextSessionId = 1;

#if MAGIC_SR_ANDROID
constexpr int32 SmokeInputWidth = 64;
constexpr int32 SmokeInputHeight = 64;
constexpr const char* SmokeModelPath =
    "/sdcard/Android/data/com.magicsr.uesmoke/files/MagicSRModels/magic_veryfastx2_cpu_params.bin";
constexpr const char* SmokeGpuModelPath =
    "/sdcard/Android/data/com.magicsr.uesmoke/files/MagicSRModels/magic_veryfast_gpu_params.bin";
constexpr const char* SmokeOutputDir = "/sdcard/Android/data/com.magicsr.uesmoke/files/MagicSRSmoke";

bool FileExists(const char* Path)
{
    struct stat Buffer;
    return stat(Path, &Buffer) == 0;
}

void* RunNativeAndroidSmokeTest(void*)
{
    for (int32 Attempt = 0; Attempt < 30 && !FileExists(SmokeGpuModelPath); ++Attempt)
    {
        sleep(1);
    }

    if (!FileExists(SmokeGpuModelPath))
    {
        __android_log_print(ANDROID_LOG_ERROR,
                            "MagicSRUESmoke",
                            "[MagicSRUESmoke] result=FAIL step=gpu_model_missing model=%s",
                            SmokeGpuModelPath);
        __android_log_print(ANDROID_LOG_ERROR,
                            "MagicSREnableSmoke",
                            "[MagicSREnableSmoke] result=FAIL step=gpu_model_missing model=%s",
                            SmokeGpuModelPath);
        return nullptr;
    }

    char Report[1400] = {};
    const int32 SmokeRet = MagicSR_RunAndroidBackendSmoke(SmokeGpuModelPath, SmokeOutputDir, Report, sizeof(Report));
    __android_log_print(SmokeRet == 0 ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
                        "MagicSRUESmoke",
                        "[MagicSRUESmoke] result=%s %s",
                        SmokeRet == 0 ? "PASS" : "FAIL",
                        Report);

    char EnableReport[768] = {};
    const int32 EnableRet = MagicSR_RunAndroidEnableSmoke(SmokeGpuModelPath, EnableReport, sizeof(EnableReport));
    __android_log_print(EnableRet == 0 ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
                        "MagicSREnableSmoke",
                        "[MagicSREnableSmoke] result=%s %s",
                        EnableRet == 0 ? "PASS" : "FAIL",
                        EnableReport);
    return nullptr;
}

__attribute__((constructor)) void StartNativeAndroidSmokeTest()
{
    pthread_t Thread;
    if (pthread_create(&Thread, nullptr, &RunNativeAndroidSmokeTest, nullptr) == 0)
    {
        pthread_detach(Thread);
    }
}
#endif

#if MAGIC_SR_IOS
constexpr int32 IOSSmokeInputWidth = 64;
constexpr int32 IOSSmokeInputHeight = 64;
constexpr int32 IOSSmokeScale = 2;
constexpr const char* IOSSmokeRelativeModelPath = "/Documents/MagicSRModels/magic_veryfastx2_cpu_params.bin";
constexpr const char* IOSSmokeRelativeFlatModelPath = "/Documents/magic_veryfastx2_cpu_params.bin";
constexpr const char* IOSSmokeRelativeOutputDir = "/Documents/MagicSRSmoke";
constexpr const char* IOSSmokeRelativeInputImagePath = "/Documents/MagicSRSmoke/input_64x64.pgm";
constexpr const char* IOSSmokeRelativeOutputImagePath = "/Documents/MagicSRSmoke/output_128x128.pgm";

bool IOSBuildSandboxPath(const char* RelativePath, char* OutPath, size_t OutPathSize)
{
    const char* Home = getenv("HOME");
    if (Home == nullptr || RelativePath == nullptr || OutPath == nullptr || OutPathSize == 0)
    {
        return false;
    }
    return snprintf(OutPath, OutPathSize, "%s%s", Home, RelativePath) > 0;
}

bool IOSFileExists(const char* Path)
{
    struct stat Buffer;
    return stat(Path, &Buffer) == 0;
}

bool IOSBuildBundlePath(const char* FileName, char* OutPath, size_t OutPathSize)
{
    char ExecutablePath[1024] = {};
    uint32_t Size = sizeof(ExecutablePath);
    if (_NSGetExecutablePath(ExecutablePath, &Size) != 0)
    {
        return false;
    }
    char* LastSlash = strrchr(ExecutablePath, '/');
    if (LastSlash == nullptr)
    {
        return false;
    }
    *LastSlash = '\0';
    return snprintf(OutPath, OutPathSize, "%s/%s", ExecutablePath, FileName) > 0;
}

void IOSNormalizePathForMagicSR(const char* Path, char* OutPath, size_t OutPathSize)
{
    constexpr const char* PrivateVarPrefix = "/private/var/";
    if (Path == nullptr || OutPath == nullptr || OutPathSize == 0)
    {
        return;
    }

    if (strncmp(Path, PrivateVarPrefix, strlen(PrivateVarPrefix)) == 0)
    {
        snprintf(OutPath, OutPathSize, "/var/%s", Path + strlen(PrivateVarPrefix));
    }
    else
    {
        snprintf(OutPath, OutPathSize, "%s", Path);
    }
}

bool IOSWriteGrayPgm(const char* Path, const uint8* Data, int32 Width, int32 Height)
{
    FILE* File = fopen(Path, "wb");
    if (File == nullptr)
    {
        return false;
    }

    fprintf(File, "P5\n%d %d\n255\n", Width, Height);
    const size_t BytesWritten = fwrite(Data, 1, static_cast<size_t>(Width * Height), File);
    fclose(File);
    return BytesWritten == static_cast<size_t>(Width * Height);
}

void* RunNativeIOSSmokeTest(void*)
{
    char ModelPath[1024] = {};
    char FlatModelPath[1024] = {};
    char BundleModelPath[1024] = {};
    char OutputDir[1024] = {};
    char InputImagePath[1024] = {};
    char OutputImagePath[1024] = {};
    if (!IOSBuildSandboxPath(IOSSmokeRelativeModelPath, ModelPath, sizeof(ModelPath)) ||
        !IOSBuildSandboxPath(IOSSmokeRelativeFlatModelPath, FlatModelPath, sizeof(FlatModelPath)) ||
        !IOSBuildBundlePath("magic_veryfastx2_cpu_params.bin", BundleModelPath, sizeof(BundleModelPath)) ||
        !IOSBuildSandboxPath(IOSSmokeRelativeOutputDir, OutputDir, sizeof(OutputDir)) ||
        !IOSBuildSandboxPath(IOSSmokeRelativeInputImagePath, InputImagePath, sizeof(InputImagePath)) ||
        !IOSBuildSandboxPath(IOSSmokeRelativeOutputImagePath, OutputImagePath, sizeof(OutputImagePath)))
    {
        return nullptr;
    }

    const char* SelectedModelPath = ModelPath;
    for (int32 Attempt = 0; Attempt < 30; ++Attempt)
    {
        if (IOSFileExists(ModelPath))
        {
            SelectedModelPath = ModelPath;
            break;
        }
        if (IOSFileExists(FlatModelPath))
        {
            SelectedModelPath = FlatModelPath;
            break;
        }
        if (IOSFileExists(BundleModelPath))
        {
            SelectedModelPath = BundleModelPath;
            break;
        }
        sleep(1);
    }

    if (!IOSFileExists(SelectedModelPath))
    {
        fprintf(stderr,
                "[MagicSRUESmoke] result=FAIL step=model_missing docs=%s flat=%s bundle=%s\n",
                ModelPath,
                FlatModelPath,
                BundleModelPath);
        exit(1);
    }

    char InitModelPath[1024] = {};
    IOSNormalizePathForMagicSR(SelectedModelPath, InitModelPath, sizeof(InitModelPath));

    FILE* ProbeFile = fopen(SelectedModelPath, "rb");
    if (ProbeFile != nullptr)
    {
        unsigned char ProbeBytes[4] = {};
        const size_t ProbeRead = fread(ProbeBytes, 1, sizeof(ProbeBytes), ProbeFile);
        fclose(ProbeFile);
        fprintf(stderr,
                "[MagicSRUESmoke] model_probe path=%s read=%zu bytes=%02x%02x%02x%02x\n",
                SelectedModelPath,
                ProbeRead,
                ProbeBytes[0],
                ProbeBytes[1],
                ProbeBytes[2],
                ProbeBytes[3]);
    }
    else
    {
        fprintf(stderr, "[MagicSRUESmoke] model_probe fopen_failed path=%s\n", SelectedModelPath);
    }

    input_param_t Params = {};
    Params.input_type = INPUT_BUFFER;
    FCStringAnsi::Strncpy(Params.model_path, InitModelPath, sizeof(Params.model_path) - 1);
    Params.width = IOSSmokeInputWidth;
    Params.height = IOSSmokeInputHeight;
    Params.scaler_factor = IOSSmokeScale;
    Params.alg_mode = HIGH_SPEED_MODE;
    Params.num_threads = 1;
    Params.log_level = MAGIC_LOG_INFO;
    Params.backend = MAGIC_BACKEND_NEON;

    fprintf(stderr,
            "[MagicSRUESmoke] init_probe selected=%s init=%s init_len=%zu model_path_capacity=%zu backend=%d\n",
            SelectedModelPath,
            InitModelPath,
            strlen(InitModelPath),
            sizeof(Params.model_path),
            static_cast<int32>(Params.backend));

    void* Handle = MC_Init(&Params);
    if (Handle == nullptr)
    {
        fprintf(stderr, "[MagicSRUESmoke] result=FAIL step=create model=%s init_model=%s\n", SelectedModelPath, InitModelPath);
        exit(1);
    }

    output_status_params_t Status = {};
    int32 QueryRet = MC_Control(Handle, QUERY_STATUS, nullptr, &Status);
    TArray<uint8> Input;
    Input.SetNumUninitialized(IOSSmokeInputWidth * IOSSmokeInputHeight);
    for (int32 Y = 0; Y < IOSSmokeInputHeight; ++Y)
    {
        for (int32 X = 0; X < IOSSmokeInputWidth; ++X)
        {
            Input[Y * IOSSmokeInputWidth + X] = static_cast<uint8>((X * 3 + Y * 5) & 0xff);
        }
    }

    TArray<uint8> Output;
    Output.SetNumZeroed(IOSSmokeInputWidth * IOSSmokeScale * IOSSmokeInputHeight * IOSSmokeScale);
    const int32 ProcessRet = MC_Process(Handle, Input.GetData(), Output.GetData());
    mkdir(OutputDir, 0775);
    const bool bWroteInput = IOSWriteGrayPgm(InputImagePath, Input.GetData(), IOSSmokeInputWidth, IOSSmokeInputHeight);
    const bool bWroteOutput =
        IOSWriteGrayPgm(OutputImagePath, Output.GetData(), IOSSmokeInputWidth * IOSSmokeScale, IOSSmokeInputHeight * IOSSmokeScale);
    int32 NonZero = 0;
    for (uint8 Value : Output)
    {
        if (Value != 0)
        {
            ++NonZero;
        }
    }
    const int32 DestroyRet = MC_Uninit(Handle);

    const bool bPass = QueryRet == 0 && ProcessRet == 0 && NonZero > 0 && bWroteInput && bWroteOutput;
    fprintf(stderr,
            "[MagicSRUESmoke] result=%s query_ret=%d process_ret=%d destroy_ret=%d output=%ux%u nonZero=%d wrote_input=%d wrote_output=%d output_path=%s err=0x%llx\n",
            bPass ? "PASS" : "FAIL",
            QueryRet,
            ProcessRet,
            DestroyRet,
            Status.output_width,
            Status.output_height,
            NonZero,
            bWroteInput ? 1 : 0,
            bWroteOutput ? 1 : 0,
            OutputImagePath,
            static_cast<unsigned long long>(Status.error_code));
    exit(bPass ? 0 : 1);
}

__attribute__((constructor)) void StartNativeIOSSmokeTest()
{
    // UE's runtime is not initialized yet during native constructors on iOS.
    // The iOS smoke test is scheduled from FMagicSRModule::StartupModule instead.
}

#endif
}  // namespace

FString UMagicSRBlueprintLibrary::GetVersion()
{
#if MAGIC_SR_SUPPORTED
    const char* version = MC_GetVersion();
    return version == nullptr ? TEXT("unknown") : UTF8_TO_TCHAR(version);
#else
    return TEXT("unsupported");
#endif
}

bool UMagicSRBlueprintLibrary::CreateSession(const FString& ModelPath,
                                             int32 Width,
                                             int32 Height,
                                             float Scale,
                                             int32 AlgMode,
                                             int32 NumThreads,
                                             int32& OutSessionId)
{
    // Legacy helper: CPU Neon + Buffer/Y8.
    return CreateSessionEx(ModelPath, Width, Height, Scale, AlgMode, NumThreads,
                           /*InputType*/ 0, /*Backend*/ 2, OutSessionId);
}

bool UMagicSRBlueprintLibrary::CreateSessionEx(const FString& ModelPath,
                                               int32 Width,
                                               int32 Height,
                                               float Scale,
                                               int32 AlgMode,
                                               int32 NumThreads,
                                               int32 InputType,
                                               int32 Backend,
                                               int32& OutSessionId)
{
    OutSessionId = -1;

#if MAGIC_SR_SUPPORTED
    if (Width <= 0 || Height <= 0 || Scale < 1.0f || Scale > 8.0f)
    {
        return false;
    }
    if (InputType < 0 || InputType >= static_cast<int32>(MAX_INPUT_TYPE))
    {
        return false;
    }
    if (Backend < 0 || Backend > static_cast<int32>(MAGIC_BACKEND_VULKAN))
    {
        return false;
    }

    FTCHARToUTF8 utf8Path(*ModelPath);

    input_param_t params = {};
    params.input_type = static_cast<input_type_e>(InputType);
    FCStringAnsi::Strncpy(params.model_path, utf8Path.Get(), sizeof(params.model_path) - 1);
    params.width = static_cast<unsigned int>(Width);
    params.height = static_cast<unsigned int>(Height);
    params.scaler_factor = Scale;
    params.alg_mode = static_cast<alg_mode_e>(AlgMode);
    params.num_threads = static_cast<unsigned int>(FMath::Clamp(NumThreads, 1, 8));
    params.log_level = MAGIC_LOG_INFO;
    params.backend = static_cast<magic_backend_e>(Backend);

    void* handle = MC_Init(&params);
    if (handle == nullptr)
    {
        return false;
    }

    FScopeLock lock(&GSessionMutex);
    const int32 sessionId = GNextSessionId++;
    GSessions.Add(sessionId, handle);
    OutSessionId = sessionId;
    return true;
#else
    return false;
#endif
}

int64 UMagicSRBlueprintLibrary::Enable(int64 InputNativeTexture, float Scale)
{
    return Enable_3params(InputNativeTexture, Scale, 0 /* HIGH_SPEED_MODE */);
}

int64 UMagicSRBlueprintLibrary::Enable_3params(int64 InputNativeTexture, float Scale, int32 AlgMode)
{
    return Enable_4params(InputNativeTexture, Scale, AlgMode, 0 /* MAGIC_BACKEND_DEFAULT */);
}

int64 UMagicSRBlueprintLibrary::Enable_4params(int64 InputNativeTexture, float Scale, int32 AlgMode, int32 Backend)
{
#if MAGIC_SR_SUPPORTED
    if (InputNativeTexture == 0)
    {
        return 0;
    }

    const float resolvedScale = Scale <= 0.0f ? 2.0f : Scale;
    void* output = MC_Enable_4params(
        reinterpret_cast<void*>(static_cast<uintptr_t>(InputNativeTexture)),
        resolvedScale,
        static_cast<alg_mode_e>(AlgMode),
        static_cast<magic_backend_e>(Backend));
    return reinterpret_cast<int64>(output);
#else
    return 0;
#endif
}




void UMagicSRBlueprintLibrary::SetModelPath(const FString& ModelPath)
{
#if MAGIC_SR_SUPPORTED
    if (ModelPath.IsEmpty())
    {
        MC_Enable_SetModelPath(nullptr);
    }
    else
    {
        MC_Enable_SetModelPath(TCHAR_TO_UTF8(*ModelPath));
    }
#else
    (void)ModelPath;
#endif
}

void UMagicSRBlueprintLibrary::SetModelDir(const FString& ModelDir)
{
#if MAGIC_SR_SUPPORTED
    if (ModelDir.IsEmpty())
    {
        MC_Enable_SetModelDir(nullptr);
    }
    else
    {
        MC_Enable_SetModelDir(TCHAR_TO_UTF8(*ModelDir));
    }
#else
    (void)ModelDir;
#endif
}

void UMagicSRBlueprintLibrary::SetInputSizeHint(int32 Width, int32 Height)
{
#if MAGIC_SR_SUPPORTED
    MC_Enable_SetInputSizeHint(
        Width > 0 ? static_cast<unsigned int>(Width) : 0u,
        Height > 0 ? static_cast<unsigned int>(Height) : 0u);
#else
    (void)Width;
    (void)Height;
#endif
}

void UMagicSRBlueprintLibrary::Disable()
{
#if MAGIC_SR_SUPPORTED
    MC_Disable(nullptr);
#endif
}

int64 UMagicSRBlueprintLibrary::EnableNative(float Scale, int64 InputNativeTexture)
{
    return Enable(InputNativeTexture, Scale);
}

void UMagicSRBlueprintLibrary::DisableNative()
{
    Disable();
}

int32 UMagicSRBlueprintLibrary::ProcessY8(int32 SessionId, const TArray<uint8>& InputY, TArray<uint8>& OutputY)
{
#if MAGIC_SR_SUPPORTED
    void* handle = nullptr;
    {
        FScopeLock lock(&GSessionMutex);
        void** found = GSessions.Find(SessionId);
        if (found == nullptr || *found == nullptr)
        {
            return -3001;
        }
        handle = *found;
    }

    output_status_params_t status = {};
    const int32 queryRet = MC_Control(handle, QUERY_STATUS, nullptr, &status);
    if (queryRet != 0)
    {
        return -3002;
    }

    if (status.input_type != INPUT_BUFFER)
    {
        // Session was created for textures; refuse silent CPU-buffer misuse.
        return -3004;
    }

    const int32 expectedInput = static_cast<int32>(status.width * status.height);
    const int32 expectedOutput = static_cast<int32>(status.output_width * status.output_height);
    if (InputY.Num() < expectedInput)
    {
        return -3003;
    }

    OutputY.SetNumUninitialized(expectedOutput);
    return MC_Process(handle, (void*)InputY.GetData(), OutputY.GetData());
#else
    return -3999;
#endif
}

int32 UMagicSRBlueprintLibrary::ProcessNativeTexture(int32 SessionId, int64 InputNativeTexture, int64 OutputNativeTexture)
{
#if MAGIC_SR_SUPPORTED
    if (InputNativeTexture == 0 || OutputNativeTexture == 0)
    {
        return -3005;
    }

    void* handle = nullptr;
    {
        FScopeLock lock(&GSessionMutex);
        void** found = GSessions.Find(SessionId);
        if (found == nullptr || *found == nullptr)
        {
            return -3001;
        }
        handle = *found;
    }

    output_status_params_t status = {};
    const int32 queryRet = MC_Control(handle, QUERY_STATUS, nullptr, &status);
    if (queryRet != 0)
    {
        return -3002;
    }

    if (status.input_type == INPUT_BUFFER)
    {
        // Session was created for CPU buffers; use ProcessY8 instead.
        return -3006;
    }

    void* inputPtr = reinterpret_cast<void*>(static_cast<uintptr_t>(InputNativeTexture));
    void* outputPtr = reinterpret_cast<void*>(static_cast<uintptr_t>(OutputNativeTexture));
    return MC_Process(handle, inputPtr, outputPtr);
#else
    return -3999;
#endif
}

namespace
{
#if MAGIC_SR_SUPPORTED
bool IsVulkanRHI()
{
    return GDynamicRHI != nullptr && FCString::Stristr(GDynamicRHI->GetName(), TEXT("Vulkan")) != nullptr;
}

bool IsOpenGLRHI()
{
    if (GDynamicRHI == nullptr)
    {
        return false;
    }
    const TCHAR* Name = GDynamicRHI->GetName();
    return FCString::Stristr(Name, TEXT("OpenGL")) != nullptr || FCString::Stristr(Name, TEXT("OpenGLES")) != nullptr;
}

bool IsMetalRHI()
{
    return GDynamicRHI != nullptr && FCString::Stristr(GDynamicRHI->GetName(), TEXT("Metal")) != nullptr;
}

/**
 * Resolve RHI native handle into the pointer shape expected by MC_Process:
 * - OpenGLES: GLuint as uintptr_t
 * - Metal: id<MTLTexture> pointer
 * - Vulkan: pointer to VkImage (UE GetNativeResource returns VkImage-as-void*, so wrap locally)
 */
int32 ResolveNativeProcessPtr(FRHITexture* RHITexture, uint64& OutVkImageStorage, void*& OutProcessPtr)
{
    OutProcessPtr = nullptr;
    OutVkImageStorage = 0;

    if (RHITexture == nullptr)
    {
        return -3008;
    }

    void* Native = RHITexture->GetNativeResource();
    if (Native == nullptr)
    {
        return -3008;
    }

    if (IsVulkanRHI())
    {
        // UE Vulkan returns VkImage cast to void*. MagicSR dereferences once as VkImage*.
        OutVkImageStorage = reinterpret_cast<uint64>(Native);
        OutProcessPtr = &OutVkImageStorage;
        return 0;
    }

    if (IsOpenGLRHI() || IsMetalRHI())
    {
        OutProcessPtr = Native;
        return 0;
    }

    return -3009;
}

FRHITexture* ResolveRHIFromGameThreadResources(FTextureRenderTargetResource* RTResource, FTextureResource* TextureResource)
{
    if (RTResource != nullptr)
    {
        return RTResource->GetRenderTargetTexture();
    }
    if (TextureResource != nullptr)
    {
        return TextureResource->TextureRHI;
    }
    return nullptr;
}
#endif
}  // namespace

int32 UMagicSRBlueprintLibrary::ProcessUTexture(int32 SessionId, UTexture* InputTexture, UTexture* OutputTexture)
{
#if MAGIC_SR_SUPPORTED
    if (InputTexture == nullptr || OutputTexture == nullptr)
    {
        return -3007;
    }

    void* Handle = nullptr;
    {
        FScopeLock Lock(&GSessionMutex);
        void** Found = GSessions.Find(SessionId);
        if (Found == nullptr || *Found == nullptr)
        {
            return -3001;
        }
        Handle = *Found;
    }

    output_status_params_t Status = {};
    const int32 QueryRet = MC_Control(Handle, QUERY_STATUS, nullptr, &Status);
    if (QueryRet != 0)
    {
        return -3002;
    }
    if (Status.input_type == INPUT_BUFFER)
    {
        return -3006;
    }

    FTextureRenderTargetResource* InputRTResource = nullptr;
    FTextureRenderTargetResource* OutputRTResource = nullptr;
    FTextureResource* InputTextureResource = nullptr;
    FTextureResource* OutputTextureResource = nullptr;

    if (UTextureRenderTarget2D* InputRT = Cast<UTextureRenderTarget2D>(InputTexture))
    {
        InputRT->UpdateResourceImmediate(false);
        InputRTResource = InputRT->GameThread_GetRenderTargetResource();
    }
    else
    {
        InputTextureResource = InputTexture->GetResource();
    }

    if (UTextureRenderTarget2D* OutputRT = Cast<UTextureRenderTarget2D>(OutputTexture))
    {
        OutputRT->UpdateResourceImmediate(false);
        OutputRTResource = OutputRT->GameThread_GetRenderTargetResource();
    }
    else
    {
        OutputTextureResource = OutputTexture->GetResource();
    }

    if ((InputRTResource == nullptr && InputTextureResource == nullptr) ||
        (OutputRTResource == nullptr && OutputTextureResource == nullptr))
    {
        return -3008;
    }

    int32 ProcessRet = -3010;
    ENQUEUE_RENDER_COMMAND(MagicSRProcessUTexture)(
        [Handle, InputRTResource, OutputRTResource, InputTextureResource, OutputTextureResource, &ProcessRet](
            FRHICommandListImmediate& /*RHICmdList*/)
        {
            FRHITexture* InputRHI = ResolveRHIFromGameThreadResources(InputRTResource, InputTextureResource);
            FRHITexture* OutputRHI = ResolveRHIFromGameThreadResources(OutputRTResource, OutputTextureResource);
            if (InputRHI == nullptr || OutputRHI == nullptr)
            {
                ProcessRet = -3008;
                return;
            }

            uint64 InputVkImage = 0;
            uint64 OutputVkImage = 0;
            void* InputPtr = nullptr;
            void* OutputPtr = nullptr;
            const int32 InResolve = ResolveNativeProcessPtr(InputRHI, InputVkImage, InputPtr);
            if (InResolve != 0)
            {
                ProcessRet = InResolve;
                return;
            }
            const int32 OutResolve = ResolveNativeProcessPtr(OutputRHI, OutputVkImage, OutputPtr);
            if (OutResolve != 0)
            {
                ProcessRet = OutResolve;
                return;
            }

            ProcessRet = MC_Process(Handle, InputPtr, OutputPtr);
        });
    FlushRenderingCommands();
    return ProcessRet;
#else
    return -3999;
#endif
}

int32 UMagicSRBlueprintLibrary::SetParam(int32 SessionId,
                                         const FString& ModelPath,
                                         int32 Width,
                                         int32 Height,
                                         float Scale,
                                         int32 AlgMode)
{
#if MAGIC_SR_SUPPORTED
    if (Width <= 0 || Height <= 0 || Scale < 1.0f || Scale > 8.0f)
    {
        return -3101;
    }

    void* handle = nullptr;
    {
        FScopeLock lock(&GSessionMutex);
        void** found = GSessions.Find(SessionId);
        if (found == nullptr || *found == nullptr)
        {
            return -3102;
        }
        handle = *found;
    }

    FTCHARToUTF8 utf8Path(*ModelPath);

    control_param_t control = {};
    control.width = static_cast<unsigned int>(Width);
    control.height = static_cast<unsigned int>(Height);
    control.scaler_factor = Scale;
    control.alg_mode = static_cast<alg_mode_e>(AlgMode);
    FCStringAnsi::Strncpy(control.model_path, utf8Path.Get(), sizeof(control.model_path) - 1);

    return MC_Control(handle, SET_PARAM, &control, nullptr);
#else
    return -3999;
#endif
}

bool UMagicSRBlueprintLibrary::QueryStatus(int32 SessionId, FMagicSRStatus& OutStatus)
{
#if MAGIC_SR_SUPPORTED
    void* handle = nullptr;
    {
        FScopeLock lock(&GSessionMutex);
        void** found = GSessions.Find(SessionId);
        if (found == nullptr || *found == nullptr)
        {
            return false;
        }
        handle = *found;
    }

    output_status_params_t status = {};
    const int32 ret = MC_Control(handle, QUERY_STATUS, nullptr, &status);
    if (ret != 0)
    {
        return false;
    }

    OutStatus.Width = static_cast<int32>(status.width);
    OutStatus.Height = static_cast<int32>(status.height);
    OutStatus.OutputWidth = static_cast<int32>(status.output_width);
    OutStatus.OutputHeight = static_cast<int32>(status.output_height);
    OutStatus.ScalerFactor = status.scaler_factor;
    OutStatus.AlgMode = static_cast<int32>(status.alg_mode);
    OutStatus.Backend = static_cast<int32>(status.backend);
    OutStatus.NumThreads = static_cast<int32>(status.num_threads);
    OutStatus.GpuTimeMs = static_cast<float>(status.gpu_time);
    OutStatus.ErrorCode = static_cast<int64>(status.error_code);
    return true;
#else
    return false;
#endif
}

int32 UMagicSRBlueprintLibrary::DestroySession(int32 SessionId)
{
#if MAGIC_SR_SUPPORTED
    void* handle = nullptr;
    {
        FScopeLock lock(&GSessionMutex);
        void** found = GSessions.Find(SessionId);
        if (found == nullptr || *found == nullptr)
        {
            return 0;
        }
        handle = *found;
        GSessions.Remove(SessionId);
    }

    return MC_Uninit(handle);
#else
    return -3999;
#endif
}
