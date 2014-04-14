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

	const float BiasedSeed = Parameters.Seed + 3.14159265359f;
	const float LocalToWorldScale = Grid->GetComponentScale().GetAbs().GetMin();
	const FLocalNoiseFunction LocalUnerodedHeightFunction(Parameters.UnerodedHeightFunction,LocalToWorldScale / Parameters.Scale);
	const FLocalNoiseFunction LocalErodedHeightFunction(Parameters.ErodedHeightFunction,LocalToWorldScale / Parameters.Scale);
	const FLocalNoiseFunction LocalErosionFunction(Parameters.ErosionFunction,LocalToWorldScale / Parameters.Scale);
	const FLocalNoiseFunction LocalMoistureFunction(Parameters.MoistureFunction,LocalToWorldScale / Parameters.Scale);
	const FLocalNoiseFunction LocalDirtProbabilityFunction(Parameters.DirtProbabilityFunction,LocalToWorldScale / Parameters.Scale);
	const FLocalNoiseFunction LocalCavernProbabilityFunction(Parameters.CavernProbabilityFunction,LocalToWorldScale / Parameters.Scale);
	const float NoiseToLocalScale = Parameters.Scale / LocalToWorldScale;

	const FInt3 BricksPerRegion = Grid->BricksPerRegion;
	const FInt3 MinRegionBrickCoordinates = RegionCoordinates * BricksPerRegion;
	const FInt3 MaxRegionBrickCoordinates = MinRegionBrickCoordinates + BricksPerRegion - FInt3::Scalar(1);
	for(int32 Y = MinRegionBrickCoordinates.Y;Y <= MaxRegionBrickCoordinates.Y;++Y)
	{
		for(int32 X = MinRegionBrickCoordinates.X;X <= MaxRegionBrickCoordinates.X;++X)
		{
			const float Erosion = LocalErosionFunction.Sample2D(BiasedSeed * 59 + X,Y);
			const float UnerodedGroundHeight = LocalUnerodedHeightFunction.Sample2D(BiasedSeed * 67 + X,Y) * NoiseToLocalScale;
			const float ErodedGroundHeight = LocalErodedHeightFunction.Sample2D(BiasedSeed * 71 + X,Y) * NoiseToLocalScale;
			const float GroundHeight = FMath::Lerp(UnerodedGroundHeight,ErodedGroundHeight,Erosion);

			const int32 BrickGroundHeight = FMath::Min(MaxRegionBrickCoordinates.Z,FMath::Ceil(GroundHeight));

			for(int32 Z = MinRegionBrickCoordinates.Z;Z <= MaxRegionBrickCoordinates.Z;++Z)
			{
				const FInt3 BrickCoordinates(X,Y,Z);
				int32 MaterialIndex = 0;
				if(Z == Grid->MinBrickCoordinates.Z)
				{
					MaterialIndex = Parameters.BottomMaterialIndex;
				}
				else if(Z <= BrickGroundHeight)
				{
					const float DirtProbability = LocalDirtProbabilityFunction.Sample3D(BiasedSeed * 79 + X,Y,Z);
					const float CavernProbability = LocalCavernProbabilityFunction.Sample3D(BiasedSeed * 73 + X,Y,Z);
					const float RockCavernThreshold = Parameters.CavernThresholdByHeight->FloatCurve.Eval(Z * LocalToWorldScale);
					const float DirtThreshold = Parameters.DirtThresholdByHeight->FloatCurve.Eval(Z * LocalToWorldScale);
					if(DirtProbability < DirtThreshold)
					{
						if(CavernProbability > RockCavernThreshold + Parameters.DirtCavernThresholdBias)
						{
							MaterialIndex = Z == BrickGroundHeight && LocalMoistureFunction.Sample2D(BiasedSeed * 61 + X,Y) > Parameters.GrassMoistureThreshold
								? Parameters.GrassMaterialIndex
								: Parameters.DirtMaterialIndex;
						}
					}
					else
					{
						if(CavernProbability > RockCavernThreshold)
						{
							MaterialIndex = Parameters.RockMaterialIndex;
						}
					}
				}
				Grid->SetBrickWithoutInvalidatingComponents(BrickCoordinates,MaterialIndex);
			}
		}
	}

	// Invalidate the chunk components for this region.
	Grid->InvalidateChunkComponents(MinRegionBrickCoordinates,MaxRegionBrickCoordinates);

	UE_LOG(LogStats,Log,TEXT("UBrickTerrainGenerationLibrary::InitRegion took %fms"),1000.0f * float(FPlatformTime::Seconds() - StartTime));
}
