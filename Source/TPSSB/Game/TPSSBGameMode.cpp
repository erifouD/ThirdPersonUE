// Copyright Epic Games, Inc. All Rights Reserved.

#include "TPSSBGameMode.h"
#include "TPSSBPlayerController.h"
#include "../Character/TPSSBCharacter.h"
#include "UObject/ConstructorHelpers.h"

ATPSSBGameMode::ATPSSBGameMode()
{
	// use our custom PlayerController class
	PlayerControllerClass = ATPSSBPlayerController::StaticClass();

	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/Blueprint/Character/TopDownCharacter"));
	if (PlayerPawnBPClass.Class != nullptr)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}