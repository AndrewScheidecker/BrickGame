// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once
#include "CoreUObject.h"
#include "Engine.h"
#include "SuperLoopLibrary.generated.h"

// Contains a static function that allows BluePrint to reset the runaway loop counter.
UCLASS()
class USuperLoopLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	// Resets the runaway loop counter to avoid BluePrint "runaway loop" errors.
	UFUNCTION(BlueprintCallable,Category="Control flow")
	static void ResetRunawayLoopCounter();
};
