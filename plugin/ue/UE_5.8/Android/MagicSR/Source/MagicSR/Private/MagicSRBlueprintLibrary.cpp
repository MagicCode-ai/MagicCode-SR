#include "MagicSRBlueprintLibrary.h"

#include "Containers/Map.h"
#include "HAL/CriticalSection.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/CString.h"
#include "Misc/ScopeLock.h"

#ifndef MAGIC_SR_IOS
#define MAGIC_SR_IOS 0
#endif

#define MAGIC_SR_SUPPORTED (MAGIC_SR_ANDROID || MAGIC_SR_IOS)

#if MAGIC_SR_ANDROID
#include <android/log.h>

extern "C" int MagicSR_RunAndroidBackendSmoke(const char* ModelPath, const char* OutputDir, char* OutReport, size_t OutReportSize);
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
constexpr int32 SmokeScale = 2;
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
    for (int32 Attempt = 0; Attempt < 30 && (!FileExists(SmokeGpuModelPath) || !FileExists(SmokeModelPath)); ++Attempt)
    {
        sleep(1);
    }

    if (!FileExists(SmokeGpuModelPath))
    {
        __android_log_print(ANDROID_LOG_ERROR,
                            "MagicSRUESmoke",
                            "[MagicSRUESmoke] result=FAIL step=gpu_model_missing model=%s",
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
                                             int32 Scale,
                                             int32 AlgMode,
                                             int32 NumThreads,
                                             int32& OutSessionId)
{
    OutSessionId = -1;

#if MAGIC_SR_SUPPORTED
    if (Width <= 0 || Height <= 0 || Scale < 2 || Scale > 4)
    {
        return false;
    }

    FTCHARToUTF8 utf8Path(*ModelPath);

    input_param_t params = {};
    params.input_type = INPUT_BUFFER;
    FCStringAnsi::Strncpy(params.model_path, utf8Path.Get(), sizeof(params.model_path) - 1);
    params.width = static_cast<unsigned int>(Width);
    params.height = static_cast<unsigned int>(Height);
    params.scaler_factor = static_cast<unsigned int>(Scale);
    params.alg_mode = static_cast<alg_mode_e>(AlgMode);
    params.num_threads = static_cast<unsigned int>(FMath::Clamp(NumThreads, 1, 8));
    params.log_level = MAGIC_LOG_INFO;
    params.backend = MAGIC_BACKEND_NEON;

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

int32 UMagicSRBlueprintLibrary::SetParam(int32 SessionId,
                                         const FString& ModelPath,
                                         int32 Width,
                                         int32 Height,
                                         int32 Scale,
                                         int32 AlgMode)
{
#if MAGIC_SR_SUPPORTED
    if (Width <= 0 || Height <= 0 || Scale < 2 || Scale > 4)
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
    control.scaler_factor = static_cast<unsigned int>(Scale);
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
    OutStatus.ScalerFactor = static_cast<int32>(status.scaler_factor);
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
