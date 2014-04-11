// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once
#include "CoreUObject.h"
#include "Engine.h"
#include "BrickGridComponent.h"
#include "BrickTerrainGenerationLibrary.generated.h"

USTRUCT(BlueprintType)
struct FNoiseFunction
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Noise)
	bool Ridged;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Noise)
	float PeriodDistance;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Noise)
	float MinValue;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Noise)
	float MaxValue;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Noise)
	int32 OctaveCount;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Noise)
	float Lacunarity;

	FNoiseFunction()
	: Ridged(true)
	, PeriodDistance(1.0f)
	, MinValue(0.0f)
	, MaxValue(1.0f)
	, OctaveCount(1)
	, Lacunarity(2.0f)
	{}
};

USTRUCT(BlueprintType)
struct FBrickTerrainGenerationParameters
{
	GENERATED_USTRUCT_BODY()

	// A random seed for the terrain.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	int32 Seed;

	// An overall scale applied to the generated terrain.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	float Scale;

	// The number of world-units per noise parameter unit for the height-map.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	FNoiseFunction RockHeightFunction;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	FNoiseFunction DirtHeightFunction;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	FNoiseFunction CavernProbabilityFunction;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	float CavernThreshold;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	float CavernProbabilityHeightBias;

	// The material index for rock.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	int32 RockMaterialIndex;

	// The material index for dirt.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	int32 DirtMaterialIndex;

	FBrickTerrainGenerationParameters()
	: Seed()
	, Scale(10000.0f)
	, CavernThreshold(0.95f)
	, RockMaterialIndex()
	, DirtMaterialIndex()
	{}
};

/** A 3D grid of bricks */
UCLASS()
class UBrickTerrainGenerationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
public:

	UFUNCTION(BlueprintCallable,Category="Terrain Generation")
	static BRICKTERRAINGENERATION_API void InitRegion(const FBrickTerrainGenerationParameters& Parameters,class UBrickGridComponent* Grid,const struct FInt3& RegionCoordinates);
};
