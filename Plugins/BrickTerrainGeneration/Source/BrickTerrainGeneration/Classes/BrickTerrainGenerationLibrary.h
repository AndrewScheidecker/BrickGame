// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once
#include "CoreUObject.h"
#include "Engine.h"
#include "BrickTerrainGenerationLibrary.generated.h"

USTRUCT()
struct FBrickTerrainGenerationParameters
{
	GENERATED_USTRUCT_BODY()

	// A random seed for the terrain.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	int32 Seed;

	// The number of world-units per noise parameter unit for the height-map.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	float HeightNoiseScale;

	// The material index for rock.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	int32 RockMaterialIndex;

	// The material index for dirt.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Terrain Generation")
	int32 DirtMaterialIndex;

	FBrickTerrainGenerationParameters()
	: Seed()
	, HeightNoiseScale(10000.0f)
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
	static BRICKTERRAINGENERATION_API void InitChunk(const FBrickTerrainGenerationParameters& Parameters,class UBrickChunkComponent* Chunk);
};
