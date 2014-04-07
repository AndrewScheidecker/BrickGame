// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once
#include "CoreUObject.h"
#include "Engine.h"
#include "SaveWorldLibrary.generated.h"

// Contains a static function that allows BluePrint to reset the runaway loop counter.
UCLASS()
class USaveWorldLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	UFUNCTION(BlueprintCallable,Category = "Saved Games")
	static bool SaveGameWorld(const FString& Filename);

	UFUNCTION(BlueprintCallable,Category = "Saved Games")
	static bool LoadGameWorld(const FString& Filename);
};
