#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types.generated.h"

UENUM(BlueprintType)
enum class EMovementState : uint8{
	AimState UMETA(DisplayName = "AimState"),
	WalkState UMETA(DisplayName = "WalkState"),
	RunState UMETA(DisplayName = "RunState"),
	SprintState UMETA(DisplayName = "SprintState")
};

USTRUCT(BlueprintType)
struct FCharacterSpeed {
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float AimSpeed = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float WalkSpeed = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float RunSpeed = 550.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SprintSpeed = 850.0f;
};

UCLASS()
class UTypes : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
};