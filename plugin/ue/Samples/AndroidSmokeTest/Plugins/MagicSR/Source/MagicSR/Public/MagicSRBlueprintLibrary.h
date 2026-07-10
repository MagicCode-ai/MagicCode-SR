#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
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
    int32 ScalerFactor = 0;

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

    UFUNCTION(BlueprintCallable, Category = "MagicSR")
    static bool CreateSession(const FString& ModelPath,
                              int32 Width,
                              int32 Height,
                              int32 Scale,
                              int32 AlgMode,
                              int32 NumThreads,
                              int32& OutSessionId);

    UFUNCTION(BlueprintCallable, Category = "MagicSR")
    static int32 ProcessY8(int32 SessionId, const TArray<uint8>& InputY, TArray<uint8>& OutputY);

    UFUNCTION(BlueprintCallable, Category = "MagicSR")
    static int32 SetParam(int32 SessionId,
                          const FString& ModelPath,
                          int32 Width,
                          int32 Height,
                          int32 Scale,
                          int32 AlgMode);

    UFUNCTION(BlueprintCallable, Category = "MagicSR")
    static bool QueryStatus(int32 SessionId, FMagicSRStatus& OutStatus);

    UFUNCTION(BlueprintCallable, Category = "MagicSR")
    static int32 DestroySession(int32 SessionId);
};
