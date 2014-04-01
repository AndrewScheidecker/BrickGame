// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once
#include "CoreUObject.h"
#include "Engine.h"
#include "SuperLoopLibrary.generated.h"

// A class used to simply access the static noise function. */
UCLASS()
class USuperLoopLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	UFUNCTION(BlueprintCallable,Category="Control flow")
	static void ResetRunawayLoopCounter();
};
