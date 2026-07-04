#include "MagicSRModule.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformFileManager.h"
#include "MagicSRBlueprintLibrary.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/CString.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#if MAGIC_SR_IOS
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <mach-o/dyld.h>

extern "C" {
#include "mc_interface.h"
}
#endif

IMPLEMENT_MODULE(FMagicSRModule, MagicSR)

namespace
{
#if MAGIC_SR_ANDROID || MAGIC_SR_IOS
constexpr int32 InputWidth = 64;
constexpr int32 InputHeight = 64;
constexpr int32 Scale = 2;
constexpr int32 AlgModeHighSpeed = 0;
constexpr int32 NumThreads = 1;
#endif

#if MAGIC_SR_ANDROID
FString ResolveSmokeModelPath()
{
    const TArray<FString> Candidates = {
        FPaths::Combine(FPaths::ProjectContentDir(), TEXT("MagicSRModels/magic_veryfastx2_cpu_params.bin")),
        FPaths::Combine(FPaths::ProjectDir(), TEXT("Content/MagicSRModels/magic_veryfastx2_cpu_params.bin")),
        FPaths::Combine(FPaths::ProjectPersistentDownloadDir(), TEXT("MagicSRModels/magic_veryfastx2_cpu_params.bin"))
    };

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    for (const FString& Candidate : Candidates)
    {
        if (PlatformFile.FileExists(*Candidate))
        {
            return Candidate;
        }
    }

    return Candidates[0];
}

#endif

#if MAGIC_SR_IOS
FString BuildIOSDocumentsPath(const TCHAR* RelativePath)
{
    const char* Home = getenv("HOME");
    if (Home == nullptr)
    {
        return FString();
    }
    return FString(UTF8_TO_TCHAR(Home)) / RelativePath;
}

bool WriteGrayPgm(const FString& Path, const TArray<uint8>& Data, int32 Width, int32 Height)
{
    FTCHARToUTF8 Utf8Path(*Path);
    FILE* File = fopen(Utf8Path.Get(), "wb");
    if (File == nullptr)
    {
        return false;
    }

    fprintf(File, "P5\n%d %d\n255\n", Width, Height);
    const size_t BytesWritten = fwrite(Data.GetData(), 1, static_cast<size_t>(Data.Num()), File);
    fclose(File);
    return BytesWritten == static_cast<size_t>(Data.Num());
}

bool IOSPathExists(const FString& Path)
{
    FTCHARToUTF8 Utf8Path(*Path);
    struct stat Buffer;
    return stat(Utf8Path.Get(), &Buffer) == 0;
}

FString NormalizeIOSPathForMagicSR(const FString& Path)
{
    constexpr const TCHAR* PrivateVarPrefix = TEXT("/private/var/");
    if (Path.StartsWith(PrivateVarPrefix))
    {
        return FString(TEXT("/var/")) + Path.RightChop(FCString::Strlen(PrivateVarPrefix));
    }
    return Path;
}

FString BuildIOSBundlePath(const TCHAR* FileName)
{
    char ExecutablePath[1024] = {};
    uint32 Size = sizeof(ExecutablePath);
    if (_NSGetExecutablePath(ExecutablePath, &Size) != 0)
    {
        return FString();
    }
    FString Executable = UTF8_TO_TCHAR(ExecutablePath);
    return FPaths::Combine(FPaths::GetPath(Executable), FileName);
}

void RunIOSSmokeTestIfRequested()
{
    static bool bHasRun = false;
    if (bHasRun)
    {
        return;
    }

    const bool bIsSmokeProject =
        FCString::Strcmp(FApp::GetProjectName(), TEXT("MagicSRUESmoke")) == 0 ||
        FCString::Strstr(FCommandLine::Get(), TEXT("MagicSRUESmoke")) != nullptr;
    if (!bIsSmokeProject)
    {
        return;
    }
    bHasRun = true;

    FString ModelPath = FPaths::Combine(FPaths::ProjectContentDir(),
                                        TEXT("MagicSRModels/magic_veryfastx2_cpu_params.bin"));
    const FString BundleModelPath = BuildIOSBundlePath(TEXT("magic_veryfastx2_cpu_params.bin"));
    const FString DocumentsModelPath =
        BuildIOSDocumentsPath(TEXT("Documents/MagicSRModels/magic_veryfastx2_cpu_params.bin"));
    const FString FlatModelPath = BuildIOSDocumentsPath(TEXT("Documents/magic_veryfastx2_cpu_params.bin"));
    const FString OutputDir = BuildIOSDocumentsPath(TEXT("Documents/MagicSRSmoke"));
    const FString InputPath = OutputDir / TEXT("input_64x64.pgm");
    const FString OutputPath = OutputDir / TEXT("output_128x128.pgm");
    for (int32 Attempt = 0; Attempt < 30; ++Attempt)
    {
        if (IOSPathExists(DocumentsModelPath))
        {
            ModelPath = DocumentsModelPath;
            break;
        }
        if (IOSPathExists(FlatModelPath))
        {
            ModelPath = FlatModelPath;
            break;
        }
        if (IOSPathExists(BundleModelPath))
        {
            ModelPath = BundleModelPath;
            break;
        }
        if (IOSPathExists(ModelPath))
        {
            break;
        }
        FPlatformProcess::Sleep(1.0f);
    }

    UE_LOG(LogTemp,
           Display,
           TEXT("[MagicSRUESmoke] starting project=%s model=%s commandline=%s"),
           FApp::GetProjectName(),
           *ModelPath,
           FCommandLine::Get());

    if (!IOSPathExists(ModelPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[MagicSRUESmoke] result=FAIL step=model_missing model=%s"), *ModelPath);
        FPlatformMisc::RequestExitWithStatus(false, 1, TEXT("MagicSR iOS smoke missing model"));
        return;
    }
    ModelPath = NormalizeIOSPathForMagicSR(ModelPath);

    const FString Version = UMagicSRBlueprintLibrary::GetVersion();
    FTCHARToUTF8 ModelPathUtf8(*ModelPath);
    const magic_backend_e BackendsToTry[] = {MAGIC_BACKEND_NEON, MAGIC_BACKEND_DEFAULT, MAGIC_BACKEND_METAL};
    void* Handle = nullptr;
    magic_backend_e SelectedBackend = MAGIC_BACKEND_NEON;
    for (magic_backend_e Backend : BackendsToTry)
    {
        input_param_t Params;
        memset(&Params, 0, sizeof(Params));
        Params.input_type = INPUT_BUFFER;
        strncpy(Params.model_path, ModelPathUtf8.Get(), sizeof(Params.model_path) - 1);
        Params.width = static_cast<unsigned int>(InputWidth);
        Params.height = static_cast<unsigned int>(InputHeight);
        Params.scaler_factor = static_cast<unsigned int>(Scale);
        Params.alg_mode = HIGH_SPEED_MODE;
        Params.num_threads = static_cast<unsigned int>(NumThreads);
        Params.log_level = MAGIC_LOG_INFO;
        Params.backend = Backend;

        UE_LOG(LogTemp,
               Display,
               TEXT("[MagicSRUESmoke] trying backend=%d model_utf8_len=%d"),
               static_cast<int32>(Backend),
               ModelPathUtf8.Length());
        Handle = MC_Init(&Params);
        if (Handle != nullptr)
        {
            SelectedBackend = Backend;
            break;
        }
    }
    if (Handle == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("[MagicSRUESmoke] result=FAIL step=create version=%s model=%s"), *Version, *ModelPath);
        FPlatformMisc::RequestExitWithStatus(false, 1, TEXT("MagicSR iOS smoke create failed"));
        return;
    }

    output_status_params_t Status = {};
    const int32 QueryRet = MC_Control(Handle, QUERY_STATUS, nullptr, &Status);

    TArray<uint8> Input;
    Input.SetNumUninitialized(InputWidth * InputHeight);
    for (int32 Y = 0; Y < InputHeight; ++Y)
    {
        for (int32 X = 0; X < InputWidth; ++X)
        {
            Input[Y * InputWidth + X] = static_cast<uint8>((X * 3 + Y * 5) & 0xff);
        }
    }

    TArray<uint8> Output;
    Output.SetNumZeroed(InputWidth * Scale * InputHeight * Scale);
    const int32 ProcessRet = MC_Process(Handle, Input.GetData(), Output.GetData());
    output_status_params_t StatusAfter = {};
    const int32 QueryAfterRet = MC_Control(Handle, QUERY_STATUS, nullptr, &StatusAfter);
    const int32 DestroyRet = MC_Uninit(Handle);

    FTCHARToUTF8 OutputDirUtf8(*OutputDir);
    mkdir(OutputDirUtf8.Get(), 0775);
    const bool bWroteInput = WriteGrayPgm(InputPath, Input, InputWidth, InputHeight);
    const bool bWroteOutput = WriteGrayPgm(OutputPath, Output, InputWidth * Scale, InputHeight * Scale);

    int32 NonZero = 0;
    for (uint8 Value : Output)
    {
        if (Value != 0)
        {
            ++NonZero;
        }
    }

    const bool bPass = QueryRet == 0 && ProcessRet == 0 && QueryAfterRet == 0 && NonZero > 0 && bWroteInput &&
                       bWroteOutput && StatusAfter.error_code == 0;
    if (bPass)
    {
        UE_LOG(LogTemp,
               Display,
               TEXT("[MagicSRUESmoke] result=PASS version=%s model=%s backend=%d query_ret=%d process_ret=%d query_after=%d destroy_ret=%d output=%dx%d nonZero=%d wrote_input=%d wrote_output=%d output_path=%s err=0x%llx"),
               *Version,
               *ModelPath,
               static_cast<int32>(SelectedBackend),
               QueryRet,
               ProcessRet,
               QueryAfterRet,
               DestroyRet,
               Status.output_width,
               Status.output_height,
               NonZero,
               bWroteInput ? 1 : 0,
               bWroteOutput ? 1 : 0,
               *OutputPath,
               static_cast<unsigned long long>(StatusAfter.error_code));
    }
    else
    {
        UE_LOG(LogTemp,
               Error,
               TEXT("[MagicSRUESmoke] result=FAIL version=%s model=%s backend=%d query_ret=%d process_ret=%d query_after=%d destroy_ret=%d output=%dx%d nonZero=%d wrote_input=%d wrote_output=%d output_path=%s err=0x%llx"),
               *Version,
               *ModelPath,
               static_cast<int32>(SelectedBackend),
               QueryRet,
               ProcessRet,
               QueryAfterRet,
               DestroyRet,
               Status.output_width,
               Status.output_height,
               NonZero,
               bWroteInput ? 1 : 0,
               bWroteOutput ? 1 : 0,
               *OutputPath,
               static_cast<unsigned long long>(StatusAfter.error_code));
    }
    FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1, TEXT("MagicSR iOS smoke complete"));
}

#endif

#if MAGIC_SR_IOS
bool TickIOSSmokeTest(float DeltaTime)
{
    RunIOSSmokeTestIfRequested();
    return false;
}
#endif

#if MAGIC_SR_ANDROID
void RunAndroidSmokeTestIfRequested()
{
    static bool bHasRun = false;
    if (bHasRun)
    {
        return;
    }

    const bool bIsSmokeProject =
        FCString::Strcmp(FApp::GetProjectName(), TEXT("MagicSRUESmoke")) == 0 ||
        FCString::Strstr(FCommandLine::Get(), TEXT("MagicSRUESmoke")) != nullptr;
    if (!bIsSmokeProject)
    {
        return;
    }
    bHasRun = true;

    UE_LOG(LogTemp,
           Display,
           TEXT("[MagicSRUESmoke] starting project=%s commandline=%s"),
           FApp::GetProjectName(),
           FCommandLine::Get());

    const FString ModelPath = ResolveSmokeModelPath();
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.FileExists(*ModelPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[MagicSRUESmoke] result=FAIL step=model_missing model=%s"), *ModelPath);
        return;
    }

    const FString Version = UMagicSRBlueprintLibrary::GetVersion();
    int32 SessionId = -1;
    const bool Created = UMagicSRBlueprintLibrary::CreateSession(
        ModelPath,
        InputWidth,
        InputHeight,
        Scale,
        AlgModeHighSpeed,
        NumThreads,
        SessionId);
    if (!Created)
    {
        UE_LOG(LogTemp, Error, TEXT("[MagicSRUESmoke] result=FAIL step=create version=%s model=%s"), *Version, *ModelPath);
        return;
    }

    FMagicSRStatus Status;
    const bool QueryOk = UMagicSRBlueprintLibrary::QueryStatus(SessionId, Status);
    if (!QueryOk || Status.OutputWidth != InputWidth * Scale || Status.OutputHeight != InputHeight * Scale)
    {
        UMagicSRBlueprintLibrary::DestroySession(SessionId);
        UE_LOG(LogTemp,
               Error,
               TEXT("[MagicSRUESmoke] result=FAIL step=query version=%s query=%d output=%dx%d err=0x%llx"),
               *Version,
               QueryOk ? 1 : 0,
               Status.OutputWidth,
               Status.OutputHeight,
               Status.ErrorCode);
        return;
    }

    TArray<uint8> Input;
    Input.SetNumUninitialized(InputWidth * InputHeight);
    for (int32 Y = 0; Y < InputHeight; ++Y)
    {
        for (int32 X = 0; X < InputWidth; ++X)
        {
            Input[Y * InputWidth + X] = static_cast<uint8>((X * 3 + Y * 5) & 0xff);
        }
    }

    TArray<uint8> Output;
    const int32 ProcessRet = UMagicSRBlueprintLibrary::ProcessY8(SessionId, Input, Output);
    FMagicSRStatus StatusAfter;
    const bool QueryAfterOk = UMagicSRBlueprintLibrary::QueryStatus(SessionId, StatusAfter);
    const int32 DestroyRet = UMagicSRBlueprintLibrary::DestroySession(SessionId);

    int32 NonZero = 0;
    for (uint8 Value : Output)
    {
        if (Value != 0)
        {
            ++NonZero;
        }
    }

    const bool Pass = ProcessRet == 0 && QueryAfterOk && NonZero > 0 && StatusAfter.ErrorCode == 0;
    UE_LOG(LogTemp,
           Display,
           TEXT("[MagicSRUESmoke] result=%s version=%s model=%s process_ret=%d query_after=%d destroy_ret=%d output=%dx%d nonZero=%d err=0x%llx"),
           Pass ? TEXT("PASS") : TEXT("FAIL"),
           *Version,
           *ModelPath,
           ProcessRet,
           QueryAfterOk ? 1 : 0,
           DestroyRet,
           Status.OutputWidth,
           Status.OutputHeight,
           NonZero,
           StatusAfter.ErrorCode);
}

bool TickAndroidSmokeTest(float DeltaTime)
{
    RunAndroidSmokeTestIfRequested();
    return false;
}
#endif
}  // namespace

void FMagicSRModule::StartupModule()
{
    UE_LOG(LogTemp,
           Display,
           TEXT("[MagicSR] StartupModule project=%s commandline=%s"),
           FApp::GetProjectName(),
           FCommandLine::Get());
#if MAGIC_SR_ANDROID
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickAndroidSmokeTest));
#endif
#if MAGIC_SR_IOS
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickIOSSmokeTest));
#endif
}

void FMagicSRModule::ShutdownModule() {}
