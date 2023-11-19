// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSSBCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/Material.h"
#include "Engine/World.h"

ATPSSBCharacter::ATPSSBCharacter()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Rotate character to moving direction
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// Create a camera boom...
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true); // Don't want arm to rotate when character does
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false; // Don't want to pull camera in when it collides with level

	// Create a camera...
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Create a decal in the world to show the cursor's location
	CursorToWorld = CreateDefaultSubobject<UDecalComponent>("CursorToWorld");
	CursorToWorld->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UMaterial> DecalMaterialAsset(TEXT("Material'/Game/Blueprint/Character/M_Cursor_Decal.M_Cursor_Decal'"));
	if (DecalMaterialAsset.Succeeded())
	{
		CursorToWorld->SetDecalMaterial(DecalMaterialAsset.Object);
	}
	CursorToWorld->DecalSize = FVector(16.0f, 32.0f, 32.0f);
	CursorToWorld->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f).Quaternion());

	// Activate ticking in order to update the cursor every frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SprintButtonPressed = 0;
	WalkButtonPressed = 0;
	AimButtonPressed = 0;
}

void ATPSSBCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

	if (CursorToWorld != nullptr)
	{
		if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
		{
			if (UWorld* World = GetWorld())
			{
				FHitResult HitResult;
				FCollisionQueryParams Params(NAME_None, FCollisionQueryParams::GetUnknownStatId());
				FVector StartLocation = TopDownCameraComponent->GetComponentLocation();
				FVector EndLocation = TopDownCameraComponent->GetComponentRotation().Vector() * 2000.0f;
				Params.AddIgnoredActor(this);
				World->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, Params);
				FQuat SurfaceRotation = HitResult.ImpactNormal.ToOrientationRotator().Quaternion();
				CursorToWorld->SetWorldLocationAndRotation(HitResult.Location, SurfaceRotation);
			}
		}
		else if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			FHitResult TraceHitResult;
			PC->GetHitResultUnderCursor(ECC_Visibility, true, TraceHitResult);
			FVector CursorFV = TraceHitResult.ImpactNormal;
			FRotator CursorR = CursorFV.Rotation();
			CursorToWorld->SetWorldLocation(TraceHitResult.Location);
			CursorToWorld->SetWorldRotation(CursorR);
		}
	}

	MovementTick(DeltaSeconds);
}

void ATPSSBCharacter::SetupPlayerInputComponent(class UInputComponent* InputComponento)
{
	Super::SetupPlayerInputComponent(InputComponento);


	InputComponento->BindAxis(TEXT("MoveForward"), this, &ATPSSBCharacter::InputAxisX);
	InputComponento->BindAxis(TEXT("MoveRight"), this, &ATPSSBCharacter::InputAxisY);

	InputComponento->BindAction(TEXT("SwitchWalk"), IE_Pressed, this, &ATPSSBCharacter::SetWalkState);
	InputComponento->BindAction(TEXT("SwitchWalk"), IE_Released, this, &ATPSSBCharacter::SetRunState);

	InputComponento->BindAction(TEXT("SprintSwitch"), IE_Pressed, this, &ATPSSBCharacter::SetSprintState);
	InputComponento->BindAction(TEXT("SprintSwitch"), IE_Released, this, &ATPSSBCharacter::SetRunState);

	InputComponento->BindAction(TEXT("AimAction"), IE_Pressed, this, &ATPSSBCharacter::SetAimState);
	InputComponento->BindAction(TEXT("AimAction"), IE_Released, this, &ATPSSBCharacter::SetRunState);
}

void ATPSSBCharacter::InputAxisX(float Value)
{
	AxisX = Value;
}

void ATPSSBCharacter::InputAxisY(float Value)
{
	AxisY = Value;
}

void ATPSSBCharacter::SetRunState()
{
}

void ATPSSBCharacter::SetSprintState()
{
	SprintButtonPressed = 1;
	MovementState = EMovementState::SprintState;
	CharacterUpdate();
}

void ATPSSBCharacter::SetWalkState()
{
	WalkButtonPressed = 1;
	if (!SprintButtonPressed) {
		MovementState = EMovementState::WalkState;
		CharacterUpdate();
	}
}

void ATPSSBCharacter::SetAimState()
{
	AimButtonPressed = 1;
	if (!SprintButtonPressed) {
		MovementState = EMovementState::AimState;
		CharacterUpdate();
	}
}

void ATPSSBCharacter::MovementTick(float DeltaTime)
{
	AddMovementInput(FVector(1.0f, 0.0f, 0.0f), AxisX);
	AddMovementInput(FVector(0.0f, 1.0f, 0.0f), AxisY);

	FHitResult HitResult;
	APlayerController* myController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	myController->GetHitResultUnderCursorByChannel(ETraceTypeQuery::TraceTypeQuery6, false, HitResult);
	SetActorRotation(FQuat(FRotator(0.0f, UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), HitResult.Location).Yaw, 0.0f)));

}

void ATPSSBCharacter::CharacterUpdate()
{
	float RegSpeed = 300.0f;
	switch (MovementState) {
	case EMovementState::AimState: RegSpeed = MovementInfo.AimSpeed; break;
	case EMovementState::WalkState: RegSpeed = MovementInfo.WalkSpeed; break;
	case EMovementState::RunState: RegSpeed = MovementInfo.RunSpeed; break;
	case EMovementState::SprintState: RegSpeed = MovementInfo.SprintSpeed; break;
	default: break;
	}
	GetCharacterMovement()->MaxWalkSpeed = RegSpeed;
}