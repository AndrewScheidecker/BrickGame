// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickGridPluginPrivatePCH.h"
#include "BrickGridComponent.h"

class FVisibilityBitArray3D
{
public:

	FVisibilityBitArray3D(const FInt3& InNumBits)
	: NumBits(InNumBits.X,InNumBits.Y,(InNumBits.Z + SubElementIndexMask) & ~SubElementIndexMask) // Round NumBits.Z up to the next multiple of BitsPerElement.
	{
		Data.Init(NumBits.X * NumBits.Y * NumBits.Z / BitsPerElement);
	}

	void SetRange(uint32 X,uint32 Y,int32 MinZ,int32 MaxZ,bool Value)
	{
		for(int32 Z = MinZ;Z <= MaxZ;++Z)
		{
			const int32 ElementIndex = ((Y * NumBits.X + X) * NumBits.Z + Z) >> Log2BitsPerElement;
			if(Value)
			{
				Data[ElementIndex] |= 1ull << (Z & SubElementIndexMask);
			}
			else
			{
				Data[ElementIndex] &= ~(1ull << (Z & SubElementIndexMask));
			}
		}
	}

	bool Get(uint32 X,uint32 Y,uint32 Z) const
	{
		const int32 ElementIndex = ((Y * NumBits.X + X) * NumBits.Z + Z) >> Log2BitsPerElement;
		return (Data[ElementIndex] >> (Z & SubElementIndexMask)) & 1;
	}

private:

	typedef uint64 TElement;
	enum { BitsPerElement = 64 };
	enum { Log2BitsPerElement = 6 };
	enum { SubElementIndexMask = BitsPerElement - 1 };

	TArray<TElement> Data;
	const FInt3 NumBits;
};

void UBrickGridComponent::UpdateRegionAO(const FInt3& RegionCoordinates)
{
	FBrickRegion& Region = Regions[RegionCoordinatesToIndex.FindRef(RegionCoordinates)];
	if(!Region.HasAmbientOcclusionFactors)
	{
		double StartTime = FPlatformTime::Seconds();
				
		// Allocate a binary visibility mask for whether each brick in the region can directly see the sky.
		const uint32 BlurRadius = Parameters.AmbientOcclusionBlurRadius;
		const uint32 BlurDiameter = BlurRadius * 2;
		const FInt3 BlurExtent = FInt3(BlurRadius,BlurRadius,0);
		const uint32 FixedBlurDenominator = (255ul << 24) / FMath::Square(BlurDiameter + 1);
		const FInt3 BricksInDirectVisibilityBuffer = BricksPerRegion + FInt3(BlurDiameter,BlurDiameter,0);
		FVisibilityBitArray3D DirectVisibilityMask(BricksInDirectVisibilityBuffer);

		// For each XY in the region, walk down Z from the top of the grid (note that this may be above the region's top).
		// The ambient occlusion for every brick below the first occupied brick is set to 0, while the ambient occlusion for every brick above is set to 1.
		const FInt3 MinVisibilityBrickCoordinates = RegionCoordinates * BricksPerRegion - BlurExtent;
		const FInt3 MaxVisibilityBrickCoordinates = MinVisibilityBrickCoordinates + BricksInDirectVisibilityBuffer - FInt3::Scalar(1);
		for(int32 Y = MinVisibilityBrickCoordinates.Y;Y <= MaxVisibilityBrickCoordinates.Y;++Y)
		{
			for(int32 X = MinVisibilityBrickCoordinates.X;X <= MaxVisibilityBrickCoordinates.X;++X)
			{
				int32 Z = MaxBrickCoordinates.Z;
				for(;Z >= MinVisibilityBrickCoordinates.Z;--Z)
				{
					const FInt3 BrickCoordinates(X,Y,Z);
					const int32 BrickMaterialIndex = GetBrick(BrickCoordinates).MaterialIndex;
					if(BrickMaterialIndex != Parameters.EmptyMaterialIndex)
					{
						break;
					}
				}
				const FInt3 DirectVisibilityCoordinates = FInt3(X,Y,Z) - MinVisibilityBrickCoordinates;
				DirectVisibilityMask.SetRange(DirectVisibilityCoordinates.X,DirectVisibilityCoordinates.Y,0,DirectVisibilityCoordinates.Z,false);
				DirectVisibilityMask.SetRange(DirectVisibilityCoordinates.X,DirectVisibilityCoordinates.Y,DirectVisibilityCoordinates.Z + 1,BricksInDirectVisibilityBuffer.Z - 1,true);
			}
		}
		
		// Allocate an initialize the region's ambient occlusion.
		const uint32 NumRegionBricks = 1 << (Parameters.BricksPerRegionLog2.X + Parameters.BricksPerRegionLog2.Y + Parameters.BricksPerRegionLog2.Z);
		Region.BrickAmbientOcclusion.Init(NumRegionBricks);
		
		const uint32 BricksInHalfFilteredBufferX = BricksPerRegion.X;
		const uint32 BricksInHalfFilteredBufferY = BricksPerRegion.Y + BlurDiameter;
		TArray<uint8> HalfFilteredVisibility;
		HalfFilteredVisibility.Init(BricksInHalfFilteredBufferX * BricksInHalfFilteredBufferY);

		for(int32 Z = 0;Z < BricksPerRegion.Z;++Z)
		{
			for(uint32 X = 0;X < BricksInHalfFilteredBufferX;++X)
			{
				for(uint32 Y = 0;Y < BricksInHalfFilteredBufferY;++Y)
				{
					uint32 SummedVisibility = 0;
					for(uint32 FilterX = 0;FilterX < BlurDiameter + 1;++FilterX)
					{
						SummedVisibility += DirectVisibilityMask.Get(X + FilterX,Y,Z) ? 1 : 0;
					}

					const uint32 HalfFilteredBrickIndex = X * BricksInHalfFilteredBufferY + Y;
					HalfFilteredVisibility[HalfFilteredBrickIndex] = (uint8)SummedVisibility;
				}
			}
		
			for(int32 Y = 0;Y < BricksPerRegion.Y;++Y)
			{
				for(int32 X = 0;X < BricksPerRegion.X;++X)
				{
					uint32 FilteredVisibility = 0;
					for(uint32 FilterY = 0;FilterY < BlurDiameter + 1;++FilterY)
					{
						FilteredVisibility += HalfFilteredVisibility[X * BricksInHalfFilteredBufferY + Y + FilterY];
					}
					FilteredVisibility *= FixedBlurDenominator;
					FilteredVisibility >>= 24;

					const uint32 RegionBrickIndex = SubregionBrickCoordinatesToRegionBrickIndex(FInt3(X,Y,Z));
					Region.BrickAmbientOcclusion[RegionBrickIndex] = (uint8)FilteredVisibility;
				}
			}
		}

		Region.HasAmbientOcclusionFactors = true;

		UE_LOG(LogStats,Log,TEXT("UBrickGridComponent::UpdateRegionAO took %fms"),1000.0f * float(FPlatformTime::Seconds() - StartTime));
	}
}

void UBrickGridComponent::UpdateAO(const FInt3& MinBrickCoordinates,const FInt3& MaxBrickCoordinates)
{
	const FInt3 MinRegionCoordinates = BrickToRegionCoordinates(MinBrickCoordinates);
	const FInt3 MaxRegionCoordinates = BrickToRegionCoordinates(MaxBrickCoordinates);
	for(int32 RegionX = MinRegionCoordinates.X;RegionX <= MaxRegionCoordinates.X;++RegionX)
	{
		for(int32 RegionY = MinRegionCoordinates.Y;RegionY <= MaxRegionCoordinates.Y;++RegionY)
		{
			for(int32 RegionZ = MinRegionCoordinates.Z;RegionZ <= MaxRegionCoordinates.Z;++RegionZ)
			{
				const FInt3 RegionCoordinates(RegionX,RegionY,RegionZ);
				if(RegionCoordinatesToIndex.Find(RegionCoordinates))
				{
					UpdateRegionAO(RegionCoordinates);
				}
			}
		}
	}
}
