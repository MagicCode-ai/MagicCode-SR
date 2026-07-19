#if MAGIC_SR_ANDROID

#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif

#include "/Users/joey/Desktop/work/01.prj/magic/project/demo/android_demo/camera/app/src/main/cpp/native-lib.cpp"
#include "MagicSRBlueprintLibrary.h"
#include "MagicSRSmokePng.h"

#include <cstdlib>

namespace
{
constexpr int SmokeWidth = 64;
constexpr int SmokeHeight = 64;
constexpr int SmokeScale = 2;

GLuint CreateGlesR8Texture(int Width, int Height, const uint8_t* Data)
{
    GLuint Texture = 0;
    glGenTextures(1, &Texture);
    glBindTexture(GL_TEXTURE_2D, Texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, Width, Height, 0, GL_RED, GL_UNSIGNED_BYTE, Data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return glGetError() == GL_NO_ERROR ? Texture : 0;
}

uint8_t* ReadGlesR8Texture(GLuint Texture, int Width, int Height)
{
    if (Texture == 0 || Width <= 0 || Height <= 0)
    {
        return nullptr;
    }

    GLuint Fbo = 0;
    glGenFramebuffers(1, &Fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, Fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &Fbo);
        return nullptr;
    }

    uint8_t* Data = static_cast<uint8_t*>(malloc(static_cast<size_t>(Width) * Height));
    if (Data != nullptr)
    {
        glFinish();
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, Width, Height, GL_RED, GL_UNSIGNED_BYTE, Data);
        if (glGetError() != GL_NO_ERROR)
        {
            free(Data);
            Data = nullptr;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &Fbo);
    return Data;
}

int RunTextureBackendSmoke(const char* ModelPath,
                           magic_backend_e Backend,
                           const char* BackendName,
                           const char* OutputDir,
                           char* OutLine,
                           size_t OutLineSize)
{
    const int OutW = SmokeWidth * SmokeScale;
    const int OutH = SmokeHeight * SmokeScale;
    const size_t InputBytes = static_cast<size_t>(SmokeWidth) * SmokeHeight * 4;
    const size_t OutputBytes = static_cast<size_t>(OutW) * OutH * 4;
    uint8_t* Input = static_cast<uint8_t*>(malloc(InputBytes));
    uint8_t* Output = static_cast<uint8_t*>(calloc(OutputBytes, 1));
    if (Input == nullptr || Output == nullptr)
    {
        free(Input);
        free(Output);
        snprintf(OutLine, OutLineSize, "backend=%s ret=-100 output=0x0 nonZero=0", BackendName);
        return -100;
    }

    for (int Y = 0; Y < SmokeHeight; ++Y)
    {
        for (int X = 0; X < SmokeWidth; ++X)
        {
            const size_t Index = (static_cast<size_t>(Y) * SmokeWidth + X) * 4;
            Input[Index] = static_cast<uint8_t>((X * 3 + Y * 5) & 0xff);
            Input[Index + 1] = static_cast<uint8_t>((X * 7) & 0xff);
            Input[Index + 2] = static_cast<uint8_t>((Y * 9) & 0xff);
            Input[Index + 3] = 255;
        }
    }

    std::string SelectedModel = ModelPath ? ModelPath : "";
    if (Backend == MAGIC_BACKEND_OPENGLES)
    {
        const size_t Pos = SelectedModel.find("magic_veryfast_gpu_params.bin");
        if (Pos != std::string::npos)
        {
            SelectedModel.replace(Pos, strlen("magic_veryfast_gpu_params.bin"), "magic_veryfast_gles_params.bin");
        }
    }

    input_param_t Params = {};
    Params.input_type = Backend == MAGIC_BACKEND_OPENGLES ? INPUT_TEXTURE_R8Unorm : INPUT_TEXTURE_RGB8Unorm;
    strncpy(Params.model_path, SelectedModel.c_str(), sizeof(Params.model_path) - 1);
    Params.width = SmokeWidth;
    Params.height = SmokeHeight;
    Params.scaler_factor = SmokeScale;
    Params.alg_mode = HIGH_SPEED_MODE;
    Params.num_threads = 1;
    Params.log_level = MAGIC_LOG_INFO;
    Params.backend = Backend;

    int Ret = 0;
    output_status_params_t Status = {};
    void* Handle = nullptr;
    if (Backend == MAGIC_BACKEND_OPENGLES && !ensureGlesTestContext())
    {
        Ret = -102;
    }
    else
    {
        Handle = MC_Init(&Params);
        Ret = Handle ? 0 : -101;
    }

    if (Ret == 0 && Backend == MAGIC_BACKEND_OPENGLES)
    {
        uint8_t* InputR8 = static_cast<uint8_t*>(malloc(static_cast<size_t>(SmokeWidth) * SmokeHeight));
        if (InputR8 == nullptr)
        {
            Ret = -105;
        }
        else
        {
            for (int I = 0; I < SmokeWidth * SmokeHeight; ++I)
            {
                InputR8[I] = Input[I * 4];
            }
        }
        while (glGetError() != GL_NO_ERROR) {}
        GLuint InTex = Ret == 0 ? CreateGlesR8Texture(SmokeWidth, SmokeHeight, InputR8) : 0;
        GLuint OutTex = Ret == 0 ? CreateGlesR8Texture(OutW, OutH, nullptr) : 0;
        if (Ret == 0 && (InTex == 0 || OutTex == 0))
        {
            Ret = -106;
        }
        if (Ret == 0)
        {
            Ret = MC_Process(Handle, reinterpret_cast<void*>(static_cast<uintptr_t>(InTex)),
                             reinterpret_cast<void*>(static_cast<uintptr_t>(OutTex)));
        }
        if (Ret == 0)
        {
            MC_Control(Handle, QUERY_STATUS, nullptr, &Status);
            uint8_t* Readback = ReadGlesR8Texture(OutTex, Status.output_width, Status.output_height);
            if (Readback != nullptr)
            {
                for (unsigned int I = 0; I < Status.output_width * Status.output_height; ++I)
                {
                    Output[I * 4] = Readback[I];
                    Output[I * 4 + 1] = Readback[I];
                    Output[I * 4 + 2] = Readback[I];
                    Output[I * 4 + 3] = 255;
                }
                free(Readback);
            }
            else
            {
                Ret = -103;
            }
        }
        gles_destroyTexture(InTex);
        gles_destroyTexture(OutTex);
        free(InputR8);
    }
    else if (Ret == 0 && Backend == MAGIC_BACKEND_VULKAN)
    {
        VulkanTexture InTex = vulkan_upload_rgb_to_texture(SmokeWidth, SmokeHeight, Input);
        VulkanTexture OutTex = vulkan_create_texture(OutW, OutH);
        transitionImageLayout(OutTex.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_GENERAL);
        Ret = MC_Process(Handle, &InTex, &OutTex);
        if (Ret == 0)
        {
            MC_Control(Handle, QUERY_STATUS, nullptr, &Status);
            uint8_t* Readback = vulkan_rgb_texture_to_cpu(OutTex, Status.output_width, Status.output_height);
            if (Readback != nullptr)
            {
                memcpy(Output, Readback, OutputBytes);
                free(Readback);
            }
            else
            {
                Ret = -104;
            }
        }
        vulkan_destroyTexture(InTex);
        vulkan_destroyTexture(OutTex);
    }

    if (Handle != nullptr)
    {
        MC_Control(Handle, QUERY_STATUS, nullptr, &Status);
        MC_Uninit(Handle);
    }

    unsigned int NonZero = 0;
    for (size_t I = 0; I < OutputBytes; ++I)
    {
        if (Output[I] != 0)
        {
            ++NonZero;
        }
    }

    char InputPath[512] = {};
    char OutputPath[512] = {};
    snprintf(InputPath, sizeof(InputPath), "%s/input_%s_64x64.png", OutputDir, BackendName);
    snprintf(OutputPath, sizeof(OutputPath), "%s/output_%s_128x128.png", OutputDir, BackendName);
    const bool WroteInput = MagicSRSmokePng::WriteRgbaPng(InputPath, Input, SmokeWidth, SmokeHeight);
    const bool WroteOutput =
        MagicSRSmokePng::WriteRgbaPng(OutputPath, Output, Status.output_width, Status.output_height);
    const bool Pass = Ret == 0 && Status.output_width == OutW && Status.output_height == OutH && NonZero > 0 && WroteInput && WroteOutput;

    snprintf(OutLine,
             OutLineSize,
             "backend=%s ret=%d output=%ux%u nonZero=%u wrote_input=%d wrote_output=%d output_path=%s",
             BackendName,
             Ret,
             Status.output_width,
             Status.output_height,
             NonZero,
             WroteInput ? 1 : 0,
             WroteOutput ? 1 : 0,
             OutputPath);

    free(Input);
    free(Output);
    return Pass ? 0 : -1;
}

// Plugin public API: Enable / Enable_3params / Enable_4params / Disable (Vulkan).
int RunEnableApiSmoke(const char* ModelPath, char* OutLine, size_t OutLineSize)
{
    constexpr int EnableW = 720;
    constexpr int EnableH = 1280;
    constexpr float EnableScale = 2.0f;
    constexpr int32 AlgModeHighSpeed = 0;       // HIGH_SPEED_MODE
    constexpr int32 BackendDefault = 0;         // MAGIC_BACKEND_DEFAULT
    constexpr int32 BackendVulkan = 6;          // MAGIC_BACKEND_VULKAN

    if (ModelPath == nullptr || ModelPath[0] == '\0')
    {
        snprintf(OutLine, OutLineSize, "api=Enable ret=-1 step=model_missing");
        return -1;
    }

    setenv("MAGIC_SR_MODEL", ModelPath, 1);

    const size_t InputBytes = static_cast<size_t>(EnableW) * EnableH * 4;
    uint8_t* Input = static_cast<uint8_t*>(malloc(InputBytes));
    if (Input == nullptr)
    {
        snprintf(OutLine, OutLineSize, "api=Enable ret=-100 step=oom");
        return -1;
    }

    for (int Y = 0; Y < EnableH; ++Y)
    {
        for (int X = 0; X < EnableW; ++X)
        {
            const size_t Index = (static_cast<size_t>(Y) * EnableW + X) * 4;
            Input[Index] = static_cast<uint8_t>((X * 3 + Y * 5) & 0xff);
            Input[Index + 1] = static_cast<uint8_t>((X * 7) & 0xff);
            Input[Index + 2] = static_cast<uint8_t>((Y * 9) & 0xff);
            Input[Index + 3] = 255;
        }
    }

    VulkanTexture InTex = vulkan_upload_rgb_to_texture(EnableW, EnableH, Input);
    free(Input);
    if (InTex.image == VK_NULL_HANDLE)
    {
        snprintf(OutLine, OutLineSize, "api=Enable ret=-106 step=upload");
        return -1;
    }

    const int64 InputHandle = static_cast<int64>(reinterpret_cast<uintptr_t>(&InTex));
    UMagicSRBlueprintLibrary::SetInputSizeHint(EnableW, EnableH);

    const int64 Out1 = UMagicSRBlueprintLibrary::Enable(InputHandle, EnableScale);
    const int64 Out2 = UMagicSRBlueprintLibrary::Enable(InputHandle, EnableScale);
    const bool Enabled = Out1 != 0;
    const bool Reused = Enabled && Out2 == Out1;
    UMagicSRBlueprintLibrary::Disable();
    const int64 Out3 = UMagicSRBlueprintLibrary::Enable(InputHandle, EnableScale);
    const bool Reenabled = Out3 != 0;
    UMagicSRBlueprintLibrary::Disable();

    UMagicSRBlueprintLibrary::SetInputSizeHint(EnableW, EnableH);
    const int64 OutMode = UMagicSRBlueprintLibrary::Enable_3params(InputHandle, EnableScale, AlgModeHighSpeed);
    const bool ModeOk = OutMode != 0;
    UMagicSRBlueprintLibrary::Disable();

    UMagicSRBlueprintLibrary::SetInputSizeHint(EnableW, EnableH);
    const int64 OutExDefault =
        UMagicSRBlueprintLibrary::Enable_4params(InputHandle, EnableScale, AlgModeHighSpeed, BackendDefault);
    const bool ExDefaultOk = OutExDefault != 0;
    UMagicSRBlueprintLibrary::Disable();

    UMagicSRBlueprintLibrary::SetInputSizeHint(EnableW, EnableH);
    const int64 OutExVulkan =
        UMagicSRBlueprintLibrary::Enable_4params(InputHandle, EnableScale, AlgModeHighSpeed, BackendVulkan);
    const bool ExVulkanOk = OutExVulkan != 0;
    UMagicSRBlueprintLibrary::Disable();

    vulkan_destroyTexture(InTex);

    const bool Pass = Enabled && Reused && Reenabled && ModeOk && ExDefaultOk && ExVulkanOk;
    snprintf(OutLine,
             OutLineSize,
             "api=Enable,Enable_3params,Enable_4params enable=%d reused=%d reenabled=%d mode=%d ex_default=%d ex_vulkan=%d "
             "in=%dx%d scale=%.1f",
             Enabled ? 1 : 0,
             Reused ? 1 : 0,
             Reenabled ? 1 : 0,
             ModeOk ? 1 : 0,
             ExDefaultOk ? 1 : 0,
             ExVulkanOk ? 1 : 0,
             EnableW,
             EnableH,
             EnableScale);
    return Pass ? 0 : -1;
}
}  // namespace

extern "C" int MagicSR_RunAndroidBackendSmoke(const char* ModelPath, const char* OutputDir, char* OutReport, size_t OutReportSize)
{
    if (ModelPath == nullptr || OutputDir == nullptr || OutReport == nullptr || OutReportSize == 0)
    {
        return -1;
    }

    mkdir(OutputDir, 0775);
    char VulkanLine[512] = {};
    char GlesLine[512] = {};
    const int VulkanRet = RunTextureBackendSmoke(ModelPath, MAGIC_BACKEND_VULKAN, "vulkan", OutputDir, VulkanLine, sizeof(VulkanLine));
    const int GlesRet = RunTextureBackendSmoke(ModelPath, MAGIC_BACKEND_OPENGLES, "gles", OutputDir, GlesLine, sizeof(GlesLine));
    const int Failures = (VulkanRet == 0 ? 0 : 1) + (GlesRet == 0 ? 0 : 1);
    snprintf(OutReport, OutReportSize, "api=CreateSession/Process %s; %s; summary failures=%d", VulkanLine, GlesLine, Failures);
    return Failures == 0 ? 0 : -1;
}

extern "C" int MagicSR_RunAndroidEnableSmoke(const char* ModelPath, char* OutReport, size_t OutReportSize)
{
    if (OutReport == nullptr || OutReportSize == 0)
    {
        return -1;
    }
    return RunEnableApiSmoke(ModelPath, OutReport, OutReportSize);
}

#endif  // MAGIC_SR_ANDROID
