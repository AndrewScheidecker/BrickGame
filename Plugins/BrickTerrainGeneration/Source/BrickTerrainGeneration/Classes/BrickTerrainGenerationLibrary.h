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

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	FNoiseFunction UnerodedHeightFunction;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	FNoiseFunction ErodedHeightFunction;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	FNoiseFunction UnerodedRockHeightFunction;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	FNoiseFunction ErodedRockHeightFunction;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	FNoiseFunction MoistureFunction;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	FNoiseFunction ErosionFunction;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	FNoiseFunction DirtThicknessFunction;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	UCurveFloat* DirtThicknessFactorByHeight;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	FNoiseFunction CavernProbabilityFunction;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	UCurveFloat* CavernThresholdByHeight;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	float DirtCavernThresholdBias;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	float GrassMoistureThreshold;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	float SandstoneMoistureThreshold;

	// The material for sandstone
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	int32 SandstoneMaterialIndex;

	// The material for eroded rock
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	int32 ErodedRockMaterialIndex;

	// The material for uneroded rock
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	int32 UnerodedRockMaterialIndex;

	// The material index for dirt.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	int32 DirtMaterialIndex;

	// The material index for grass.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	int32 GrassMaterialIndex;

	// The material index to use for the bottom of the map.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	int32 BottomMaterialIndex;

	FBrickTerrainGenerationParameters()
	: Scale(1.0f)
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
