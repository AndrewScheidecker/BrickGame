// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickTerrainGenerationPluginPrivatePCH.h"
#include "BrickTerrainGenerationLibrary.h"
#include "BrickChunkComponent.h"
#include "SimplexNoise.h"

ISimplexNoise* SimplexNoiseModule = NULL;

UBrickTerrainGenerationLibrary::UBrickTerrainGenerationLibrary( const FPostConstructInitializeProperties& PCIP )
: Super( PCIP )
{}

void UBrickTerrainGenerationLibrary::InitChunk(const FBrickTerrainGenerationParameters& Parameters,class UBrickChunkComponent* Chunk)
{
	if(SimplexNoiseModule == NULL)
	{
		SimplexNoiseModule = &ISimplexNoise::Get();
	}

	const FMatrix ChunkToWorld = Chunk->GetComponentToWorld().ToMatrixWithScale();
	const FBrickChunkParameters& ChunkParameters = Chunk->GetParameters();
	const float InvHeightNoiseScale = 1.0f / Parameters.HeightNoiseScale;

	for(int32 Y = 0;Y < ChunkParameters.SizeY;++Y)
	{
		for(int32 X = 0;X < ChunkParameters.SizeX;++X)
		{
			const FVector WorldXY = ChunkToWorld.TransformPosition(FVector(X,Y,0));
			const float DirtHeight = 0.3f + 0.3f * SimplexNoiseModule->Sample2D(Parameters.Seed * 71 + WorldXY.X * InvHeightNoiseScale,WorldXY.Y * InvHeightNoiseScale);
			const float RockHeight = 0.01f + 0.99f * SimplexNoiseModule->Sample2D(Parameters.Seed * 67 + WorldXY.X * InvHeightNoiseScale,WorldXY.Y * InvHeightNoiseScale)
									- 0.05f + 0.1f * SimplexNoiseModule->Sample2D(Parameters.Seed * 67 + WorldXY.X * InvHeightNoiseScale * 100,WorldXY.Y * InvHeightNoiseScale * 100);

			const int32 DirtBrickHeight = FMath::Clamp(FMath::Ceil(ChunkParameters.SizeZ * DirtHeight),0,ChunkParameters.SizeZ);
			const int32 RockBrickHeight = FMath::Clamp(FMath::Ceil(ChunkParameters.SizeZ * RockHeight),0,ChunkParameters.SizeZ);
			const int32 MaxBrickHeight = FMath::Max(RockBrickHeight,DirtBrickHeight);

			for(int32 Z = 0;Z < MaxBrickHeight;++Z)
			{
				Chunk->SetBrick(X, Y, Z, Z < RockBrickHeight ? Parameters.RockMaterialIndex : Parameters.DirtMaterialIndex);
			}
		}
	}
}
