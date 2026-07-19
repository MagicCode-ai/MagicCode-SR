#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MagicSRBlueprintLibrary.generated.h"

USTRUCT(BlueprintType)
struct FMagicSRStatus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 Width = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 Height = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 OutputWidth = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 OutputHeight = 0;

    UPROPERTY(BlueprintReadOnly)
    float ScalerFactor = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    int32 AlgMode = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 Backend = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 NumThreads = 0;

    UPROPERTY(BlueprintReadOnly)
    float GpuTimeMs = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    int64 ErrorCode = 0;
};

UCLASS()
class MAGICSR_API UMagicSRBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "MagicSR")
    static FString GetVersion();

    /** Buffer/Y8 path (CPU backends). Prefer CreateSessionEx + ProcessNativeTexture for GPU game pipelines. */
    UFUNCTION(BlueprintCallable, Category = "MagicSR")
    static bool CreateSession(const FString& ModelPath,
                              int32 Width,
                              int32 Height,
                              float Scale,
                              int32 AlgMode,
                              int32 NumThreads,
                              int32& OutSessionId);

    /**
     * Preferred session create for GPU texture pipelines.
     * InputType: 0=Buffer, 1=TextureRgb8Unorm, 2=TextureR8Unorm
     * Backend: 2=Neon, 3=Metal, 5=OpenGLES, 6=Vulkan
     */
    UFUNCTION(BlueprintCallable, Category = "MagicSR")
    static bool CreateSessionEx(const FString& ModelPath,
                                int32 Width,
                                int32 Height,
                                float Scale,
                                int32 AlgMode,
                                int32 NumThreads,
                                int32 InputType,
                                int32 Backend,
                                int32& OutSessionId);

    /**
     * Preferred two-call GPU path wrapping MC_Enable / MC_Disable.
     * Input must be RGBA8Unorm (Metal / GLES) or RGB8Unorm (Vulkan) native texture.
     * Works with any native GPU texture (not tied to camera / viewport).
     * Returns a borrowed output native texture as int64 (0 on failure).
     * Session is reused across calls when scale/size/mode/backend are unchanged.
     * Do not free/delete the returned pointer — call Disable() only.
     * Scale in [1,8]; <=0 => 2.0f.
     * AlgMode: 0=HighSpeed, 1=Speed. Backend: 0=Default, 3=Metal, 5=OpenGLES, 6=Vulkan.
     */
    UFUNCTION(BlueprintCallable, Category = "MagicSR|Easy")
    static int64 Enable(int64 InputNativeTexture, float Scale);

    UFUNCTION(BlueprintCallable, Category = "MagicSR|Easy")
    static int64 Enable_3params(int64 InputNativeTexture, float Scale, int32 AlgMode);

    UFUNCTION(BlueprintCallable, Category = "MagicSR|Easy")
    static int64 Enable_4params(int64 InputNativeTexture, float Scale, int32 AlgMode, int32 Backend);

    /** Optional: hint input WxH for backends that cannot query texture size (Android Vulkan). */
    UFUNCTION(BlueprintCallable, Category = "MagicSR|Easy")
    static void SetInputSizeHint(int32 Width, int32 Height);

    /** Tear down the MC_Enable singleton (only valid release for Enable output). */
    UFUNCTION(BlueprintCallable, Category = "MagicSR|Easy")
    static void Disable();

    /**
     * @deprecated Prefer Enable(InputNativeTexture, Scale).
     */
    UFUNCTION(BlueprintCallable, Category = "MagicSR", meta = (DeprecatedFunction, DeprecationMessage = "Use Enable(InputNativeTexture, Scale)"))
    static int64 EnableNative(float Scale, int64 InputNativeTexture);

    /** @deprecated Prefer Disable(). */
    UFUNCTION(BlueprintCallable, Category = "MagicSR", meta = (DeprecatedFunction, DeprecationMessage = "Use Disable()"))
    static void DisableNative();

    /** CPU buffer path. Use only with InputType=Buffer sessions. */
    UFUNCTION(BlueprintCallable, Category = "MagicSR")
    static int32 ProcessY8(int32 SessionId, const TArray<uint8>& InputY, TArray<uint8>& OutputY);

    /**
     * Preferred GPU path. Pass native GPU texture handles:
     * - OpenGLES: GLuint texture id cast to int64
     * - Vulkan/Metal: native texture object pointer cast to int64
     * Must match CreateSessionEx InputType/Backend.
     */
    UFUNCTION(BlueprintCallable, Category = "MagicSR")
    static int32 ProcessNativeTexture(int32 SessionId, int64 InputNativeTexture, int64 OutputNativeTexture);

    /**
     * High-level GPU path for game content.
     * Resolves UTexture/UTextureRenderTarget2D RHI native handles on the render thread
     * and calls MC_Process (Metal / OpenGLES / Vulkan).
     */
    UFUNCTION(BlueprintCallable, Category = "MagicSR")
    static int32 ProcessUTexture(int32 SessionId, UTexture* InputTexture, UTexture* OutputTexture);

    UFUNCTION(BlueprintCallable, Category = "MagicSR")
    static int32 SetParam(int32 SessionId,
                          const FString& ModelPath,
                          int32 Width,
                          int32 Height,
                          float Scale,
                          int32 AlgMode);

    UFUNCTION(BlueprintCallable, Category = "MagicSR")
    static bool QueryStatus(int32 SessionId, FMagicSRStatus& OutStatus);

    UFUNCTION(BlueprintCallable, Category = "MagicSR")
    static int32 DestroySession(int32 SessionId);
};
