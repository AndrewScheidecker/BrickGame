// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BrickGame.h"
#include "BrickGameGameMode.h"
#include "BrickGameHUD.h"

ABrickGameGameMode::ABrickGameGameMode(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// use our custom HUD class
	HUDClass = ABrickGameHUD::StaticClass();
}
