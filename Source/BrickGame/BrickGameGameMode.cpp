// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BrickGame.h"
#include "BrickGameGameMode.h"
#include "BrickGameHUD.h"

ABrickGameGameMode::ABrickGameGameMode(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FObjectFinder<UBlueprint> PlayerPawnObject(TEXT("/Game/Blueprints/MyCharacter"));
	if (PlayerPawnObject.Object != NULL)
	{
		DefaultPawnClass = (UClass*)PlayerPawnObject.Object->GeneratedClass;
	}

	// use our custom HUD class
	HUDClass = ABrickGameHUD::StaticClass();
}
