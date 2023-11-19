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
#include "EnhancedInputSubsystems.h"
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

void ATPSSBCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ATPSSBCharacter::SetupPlayerInputComponent(class UInputComponent* InputComponento)
{
	Super::SetupPlayerInputComponent(InputComponento);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponento)) {
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATPSSBCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ATPSSBCharacter::Move);

		EnhancedInputComponent->BindAction(WalkAction, ETriggerEvent::Triggered, this, &ATPSSBCharacter::Walk);
		EnhancedInputComponent->BindAction(WalkAction, ETriggerEvent::Completed, this, &ATPSSBCharacter::Walk);

		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Triggered, this, &ATPSSBCharacter::Aim);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &ATPSSBCharacter::Aim);

		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Triggered, this, &ATPSSBCharacter::Sprint);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ATPSSBCharacter::Sprint);
	}
}

void ATPSSBCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr){
		AddMovementInput(FVector(1.0f, 0.0f, 0.0f), MovementVector.Y);
		AddMovementInput(FVector(0.0f, 1.0f, 0.0f), MovementVector.X);
	}
}

void ATPSSBCharacter::Walk(const FInputActionValue& Value)
{
	if (Value.IsNonZero()) {
		MovementState = EMovementState::WalkState;
		WalkButtonPressed = 1;
	}
	else {
		MovementState = EMovementState::RunState;
		WalkButtonPressed = 0;
	}

	CharacterUpdate();
}

void ATPSSBCharacter::Aim(const FInputActionValue& Value)
{
	if (Value.IsNonZero()) {
		MovementState = EMovementState::AimState;
		AimButtonPressed = 1;
	}
	else {
		MovementState = EMovementState::RunState;
		AimButtonPressed = 0;
	}

	CharacterUpdate();
}

void ATPSSBCharacter::Sprint(const FInputActionValue& Value)
{
	if (Value.IsNonZero()) {
		MovementState = EMovementState::SprintState;
		SprintButtonPressed = 1;
	}
	else {
		MovementState = EMovementState::RunState;
		SprintButtonPressed = 0;
	}

	CharacterUpdate();
}


void ATPSSBCharacter::MovementTick(float DeltaTime)
{

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