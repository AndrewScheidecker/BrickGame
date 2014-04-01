// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once
#include "CoreUObject.h"
#include "Engine.h"
#include "SimplexNoiseLibrary.generated.h"

// A class used to simply access the static noise function. */
UCLASS()
class USimplexNoiseLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	// Samples a 1D simplex noise function, returning a value between 0-1.
	UFUNCTION(BlueprintCallable,Category=Noise)
	static float Sample(float X);

	// Samples a 2D simplex noise function, returning a value between 0-1.
	UFUNCTION(BlueprintCallable,Category=Noise)
	static float Sample2D(float X,float Y);

	// Samples a 3D simplex noise function, returning a value between 0-1.
	UFUNCTION(BlueprintCallable,Category=Noise)
	static float Sample3D(float X,float Y,float Z);
};
