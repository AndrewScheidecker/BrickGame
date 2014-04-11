// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickTerrainGenerationPluginPrivatePCH.h"
#include "BrickTerrainGenerationLibrary.h"
#include "BrickGridComponent.h"
#include "SimplexNoise.h"

UBrickTerrainGenerationLibrary::UBrickTerrainGenerationLibrary( const FPostConstructInitializeProperties& PCIP )
: Super( PCIP )
{}

class FLocalNoiseFunction : public FNoiseFunction
{
public:

	FLocalNoiseFunction(const FNoiseFunction& NoiseFunction,float LocalToWorldScale)
	: FNoiseFunction(NoiseFunction)
	, LocalToPeriodicScale(LocalToWorldScale / PeriodDistance)
	{
		SimplexNoiseModule = &ISimplexNoise::Get();

		// Compute the total denominator for all octaves of the noise.
		// Sum[l^i,{i,0,n-1}] == (L^n - 1) / (L - 1)
		const float ValueDenominator = (FMath::Pow(Lacunarity,OctaveCount) - 1.0f) / (Lacunarity - 1.0f);

		ValueScale = (MaxValue - MinValue) / (ValueDenominator * 2.0f);
		ValueBias = (MaxValue + MinValue) / 2.0f;
	}

	float Sample2D(float X,float Y) const
	{
		float ScaledX = X * LocalToPeriodicScale;
		float ScaledY = Y * LocalToPeriodicScale;
		float Value = 0.0f;
		for(int32 OctaveIndex = 0;OctaveIndex < OctaveCount;++OctaveIndex)
		{
			const float OctaveValue = SimplexNoiseModule->Sample2D(ScaledX,ScaledY);
			Value *= Lacunarity;
			Value += Ridged ? (-1.0f + 2.0f * (1.0f - FMath::Abs(OctaveValue))) : OctaveValue;
			ScaledX *= Lacunarity;
			ScaledY *= Lacunarity;
		}
		return Value * ValueScale + ValueBias;
	}

	float Sample3D(float X,float Y,float Z) const
	{
		float ScaledX = X * LocalToPeriodicScale;
		float ScaledY = Y * LocalToPeriodicScale;
		float ScaledZ = Z * LocalToPeriodicScale;
		float Value = 0.0f;
		for(int32 OctaveIndex = 0;OctaveIndex < OctaveCount;++OctaveIndex)
		{
			const float OctaveValue = SimplexNoiseModule->Sample3D(ScaledX,ScaledY,ScaledZ);
			Value *= Lacunarity;
			Value += Ridged ? (-1.0f + 2.0f * (1.0f - FMath::Abs(OctaveValue))) : OctaveValue;
			ScaledX *= Lacunarity;
			ScaledY *= Lacunarity;
			ScaledZ *= Lacunarity;
		}
		return Value * ValueScale + ValueBias;
	}

private:

	ISimplexNoise* SimplexNoiseModule;

	float LocalToPeriodicScale;
	float ValueScale;
	float ValueBias;
};

void UBrickTerrainGenerationLibrary::InitRegion(const FBrickTerrainGenerationParameters& Parameters,class UBrickGridComponent* Grid,const FInt3& RegionCoordinates)
{
	const double StartTime = FPlatformTime::Seconds();

	const float LocalToWorldScale = Grid->GetComponentScale().GetAbs().GetMin();
	const FLocalNoiseFunction LocalRockHeightFunction(Parameters.RockHeightFunction,LocalToWorldScale / Parameters.Scale);
	const FLocalNoiseFunction LocalDirtHeightFunction(Parameters.DirtHeightFunction,LocalToWorldScale / Parameters.Scale);
	const FLocalNoiseFunction LocalCavernProbabilityFunction(Parameters.CavernProbabilityFunction,LocalToWorldScale / Parameters.Scale);
	const float NoiseToLocalScale = Parameters.Scale / LocalToWorldScale;

	const FInt3 BricksPerRegion = Grid->BricksPerRegion;
	const FInt3 MinRegionBrickCoordinates = RegionCoordinates * BricksPerRegion;
	const FInt3 MaxRegionBrickCoordinates = MinRegionBrickCoordinates + BricksPerRegion - FInt3::Scalar(1);
	for(int32 Y = MinRegionBrickCoordinates.Y;Y <= MaxRegionBrickCoordinates.Y;++Y)
	{
		for(int32 X = MinRegionBrickCoordinates.X;X <= MaxRegionBrickCoordinates.X;++X)
		{
			const float DirtHeight = LocalDirtHeightFunction.Sample2D(Parameters.Seed * 67 + X,Y) * NoiseToLocalScale;
			const float RockHeight = LocalRockHeightFunction.Sample2D(Parameters.Seed * 71 + X,Y) * NoiseToLocalScale;

			const int32 DirtBrickHeight = FMath::Clamp(FMath::Ceil(DirtHeight),Grid->MinBrickCoordinates.Z,Grid->MaxBrickCoordinates.Z);
			const int32 RockBrickHeight = FMath::Clamp(FMath::Ceil(RockHeight),Grid->MinBrickCoordinates.Z,Grid->MaxBrickCoordinates.Z);
			const int32 MaxBrickHeight = FMath::Min(MaxRegionBrickCoordinates.Z,FMath::Max(RockBrickHeight,DirtBrickHeight));

			for(int32 Z = MinRegionBrickCoordinates.Z;Z <= MaxRegionBrickCoordinates.Z;++Z)
			{
				const FInt3 BrickCoordinates(X,Y,Z);
				int32 MaterialIndex = 0;
				if(Z == Grid->MinBrickCoordinates.Z)
				{
					MaterialIndex = Parameters.RockMaterialIndex;
				}
				else if(Z <= MaxBrickHeight && LocalCavernProbabilityFunction.Sample3D(Parameters.Seed * 73 + X,Y,Z) < (Parameters.CavernThreshold + Parameters.CavernProbabilityHeightBias * Z * LocalToWorldScale))
				{
					MaterialIndex = Z <= RockBrickHeight ? Parameters.RockMaterialIndex : Parameters.DirtMaterialIndex;
				}
				Grid->SetBrickWithoutInvalidatingComponents(BrickCoordinates,MaterialIndex);
			}
		}
	}

	// Invalidate the chunk components for this region.
	const FInt3 MinChunkCoordinates = Grid->BrickToChunkCoordinates(MinRegionBrickCoordinates);
	const FInt3 MaxChunkCoordinates = Grid->BrickToChunkCoordinates(MaxRegionBrickCoordinates);
	Grid->InvalidateChunkComponents(MinChunkCoordinates,MaxChunkCoordinates);

	UE_LOG(LogInit,Log,TEXT("UBrickTerrainGenerationLibrary::InitRegion took %fms"),1000.0f * float(FPlatformTime::Seconds() - StartTime));
}
