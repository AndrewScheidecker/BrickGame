// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickTerrainGenerationPluginPrivatePCH.h"
#include "BrickTerrainGenerationLibrary.h"
#include "BrickGridComponent.h"
#include "SimplexNoise.h"

ISimplexNoise* SimplexNoiseModule = NULL;

UBrickTerrainGenerationLibrary::UBrickTerrainGenerationLibrary( const FPostConstructInitializeProperties& PCIP )
: Super( PCIP )
{}

void UBrickTerrainGenerationLibrary::InitRegion(const FBrickTerrainGenerationParameters& Parameters,class UBrickGridComponent* Grid,const FInt3& RegionCoordinates)
{
	if(SimplexNoiseModule == NULL)
	{
		SimplexNoiseModule = &ISimplexNoise::Get();
	}

	const FVector2D NoiseScale2D = FVector2D(Grid->GetComponentScale()) / Parameters.HeightNoiseScale;
	const int32 MinBrickZ = Grid->MinBrickCoordinates.Z;
	const int32 SizeZ = Grid->MaxBrickCoordinates.Z - MinBrickZ;

	const FInt3 BricksPerRegion = Grid->BricksPerRegion;
	const FInt3 MinBrickCoordinates = RegionCoordinates * BricksPerRegion;
	const FInt3 MaxBrickCoordinatesPlus1 = MinBrickCoordinates + BricksPerRegion;
	for(int32 Y = MinBrickCoordinates.Y;Y < MaxBrickCoordinatesPlus1.Y;++Y)
	{
		for(int32 X = MinBrickCoordinates.X;X < MaxBrickCoordinatesPlus1.X;++X)
		{
			const FVector2D NoiseXY = FVector2D(X,Y) * NoiseScale2D;
			const float DirtHeight = 0.3f + 0.3f * SimplexNoiseModule->Sample2D(Parameters.Seed * 71 + NoiseXY.X,NoiseXY.Y);
			const float RockHeight = 0.01f + 0.99f * SimplexNoiseModule->Sample2D(Parameters.Seed * 67 + NoiseXY.X,NoiseXY.Y)
									- 0.05f + 0.1f * SimplexNoiseModule->Sample2D(Parameters.Seed * 67 + NoiseXY.X * 100,NoiseXY.Y * 100);

			const int32 DirtBrickHeight = MinBrickZ + FMath::Clamp(FMath::Ceil(SizeZ * DirtHeight),0,SizeZ - 1);
			const int32 RockBrickHeight = MinBrickZ + FMath::Clamp(FMath::Ceil(SizeZ * RockHeight),0,SizeZ - 1);
			const int32 MaxBrickHeight = FMath::Max(RockBrickHeight,DirtBrickHeight);

			for(int32 Z = MinBrickCoordinates.Z;Z < MaxBrickHeight;++Z)
			{
				Grid->SetBrick(FInt3(X,Y,Z),Z < RockBrickHeight ? Parameters.RockMaterialIndex : Parameters.DirtMaterialIndex);
			}
		}
	}
}
