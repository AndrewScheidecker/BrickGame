// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelLandscapePrivatePCH.h"
#include "ComponentReregisterContext.h"
#include "VoxelRenderComponent.h"
#include "VoxelCollisionComponent.h"
#include "VoxelLandscapeComponent.h"

void UVoxelLandscapeComponent::Init(const FVoxelLandscapeParameters& InParameters)
{
	Parameters = InParameters;

	// Validate the empty material index.
	Parameters.EmptyMaterialIndex = FMath::Clamp<int32>(Parameters.EmptyMaterialIndex, 0, Parameters.Materials.Num() - 1);

	// Limit each region to 128x128x128 Voxels, which is the largest power of 2 size that can be rendered using 8-bit relative vertex positions.
	Parameters.VoxelsPerRegionLog2 = FInt3::Clamp(Parameters.VoxelsPerRegionLog2, FInt3::Scalar(0), FInt3::Scalar(VoxelLandscapeConstants::MaxVoxelsPerRegionAxisLog2));

	// Don't allow fractional chunks/region, or chunks smaller than one Voxel.
	Parameters.RenderChunksPerRegionLog2 = FInt3::Clamp(Parameters.RenderChunksPerRegionLog2, FInt3::Scalar(0), Parameters.VoxelsPerRegionLog2);
	Parameters.CollisionChunksPerRegionLog2 = FInt3::Clamp(Parameters.CollisionChunksPerRegionLog2, FInt3::Scalar(0), Parameters.VoxelsPerRegionLog2);

	// Derive the chunk and region sizes from the log2 inputs.
	VoxelsPerRenderChunkLog2 = Parameters.VoxelsPerRegionLog2 - Parameters.RenderChunksPerRegionLog2;
	VoxelsPerCollisionChunkLog2 = Parameters.VoxelsPerRegionLog2 - Parameters.CollisionChunksPerRegionLog2;
	VoxelsPerRegion = VoxelsPerCollisionChunk = FInt3::Exp2(Parameters.VoxelsPerRegionLog2);
	VoxelsPerRenderChunk = FInt3::Exp2(VoxelsPerRenderChunkLog2);
	VoxelsPerCollisionChunk = FInt3::Exp2(VoxelsPerCollisionChunkLog2);
	RenderChunksPerRegion = FInt3::Exp2(Parameters.RenderChunksPerRegionLog2);
	CollisionChunksPerRegion = FInt3::Exp2(Parameters.CollisionChunksPerRegionLog2);
	VoxelsPerRegion = FInt3::Exp2(Parameters.VoxelsPerRegionLog2);

	// Clamp the min/max region coordinates to keep Voxel coordinates within 32-bit signed integers.
	Parameters.MinRegionCoordinates = FInt3::Max(Parameters.MinRegionCoordinates, FInt3::Scalar(INT_MIN) / VoxelsPerRegion);
	Parameters.MaxRegionCoordinates = FInt3::Min(Parameters.MaxRegionCoordinates, (FInt3::Scalar(INT_MAX) - VoxelsPerRegion + FInt3::Scalar(1)) / VoxelsPerRegion);
	MinVoxelCoordinates = Parameters.MinRegionCoordinates * VoxelsPerRegion;
	MaxVoxelCoordinates = Parameters.MaxRegionCoordinates * VoxelsPerRegion + VoxelsPerRegion - FInt3::Scalar(1);

	// Limit the ambient occlusion blur radius to be a positive value.
	Parameters.AmbientOcclusionBlurRadius = FMath::Max(0, Parameters.AmbientOcclusionBlurRadius);

	// Reset the regions and reregister the component.
	FComponentReregisterContext ReregisterContext(this);
	Regions.Empty();
	RegionCoordinatesToIndex.Empty();
	for (auto ChunkIt = RenderChunkCoordinatesToComponent.CreateConstIterator(); ChunkIt; ++ChunkIt)
	{
		ChunkIt.Value()->DetachFromParent();
		ChunkIt.Value()->DestroyComponent();
	}
	for (auto ChunkIt = CollisionChunkCoordinatesToComponent.CreateConstIterator(); ChunkIt; ++ChunkIt)
	{
		ChunkIt.Value()->DetachFromParent();
		ChunkIt.Value()->DestroyComponent();
	}
	RenderChunkCoordinatesToComponent.Empty();
	CollisionChunkCoordinatesToComponent.Empty();
}

FVoxelLandscapeData UVoxelLandscapeComponent::GetData() const
{
	FVoxelLandscapeData Result;
	Result.Regions = Regions;
	return Result;
}

void UVoxelLandscapeComponent::SetData(const FVoxelLandscapeData& Data)
{
	Init(Parameters);
	Regions = Data.Regions;

	for (auto RegionIt = Regions.CreateIterator(); RegionIt; ++RegionIt)
	{
		// Compute the max non-empty Voxel map for the new regions.
		UpdateMaxNonEmptyVoxelMap(*RegionIt, FInt3::Scalar(0), VoxelsPerRegion - FInt3::Scalar(1));

		// Recreate the region coordinate to index map.
		RegionCoordinatesToIndex.Add(RegionIt->Coordinates, RegionIt.GetIndex());
	}
}

FVoxel UVoxelLandscapeComponent::GetVoxel(const FInt3& VoxelCoordinates) const
{
	if (FInt3::All(VoxelCoordinates >= MinVoxelCoordinates) && FInt3::All(VoxelCoordinates <= MaxVoxelCoordinates))
	{
		const FInt3 RegionCoordinates = VoxelToRegionCoordinates(VoxelCoordinates);
		const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
		if (RegionIndex != NULL)
		{
			const uint32 VoxelIndex = VoxelCoordinatesToRegionVoxelIndex(RegionCoordinates, VoxelCoordinates);
			const FVoxelRegion& Region = Regions[*RegionIndex];
			return FVoxel(Region.VoxelContents[VoxelIndex]);
		}
	}
	return FVoxel(Parameters.EmptyMaterialIndex);
}

void UVoxelLandscapeComponent::GetVoxelMaterialArray(const FInt3& MinVoxelCoordinates, const FInt3& MaxVoxelCoordinates, TArray<uint8>& OutVoxelMaterials) const
{
	const FInt3 OutputSize = MaxVoxelCoordinates - MinVoxelCoordinates + FInt3::Scalar(1);
	const FInt3 MinRegionCoordinates = VoxelToRegionCoordinates(MinVoxelCoordinates);
	const FInt3 MaxRegionCoordinates = VoxelToRegionCoordinates(MaxVoxelCoordinates);
	for (int32 RegionY = MinRegionCoordinates.Y; RegionY <= MaxRegionCoordinates.Y; ++RegionY)
	{
		for (int32 RegionX = MinRegionCoordinates.X; RegionX <= MaxRegionCoordinates.X; ++RegionX)
		{
			for (int32 RegionZ = MinRegionCoordinates.Z; RegionZ <= MaxRegionCoordinates.Z; ++RegionZ)
			{
				const FInt3 RegionCoordinates(RegionX, RegionY, RegionZ);
				const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
				const FInt3 MinRegionVoxelCoordinates = FInt3(RegionX, RegionY, RegionZ) * VoxelsPerRegion;
				const FInt3 MinOutputRegionVoxelCoordinates = FInt3::Max(FInt3::Scalar(0), MinVoxelCoordinates - MinRegionVoxelCoordinates);
				const FInt3 MaxOutputRegionVoxelCoordinates = FInt3::Min(VoxelsPerRegion - FInt3::Scalar(1), MaxVoxelCoordinates - MinRegionVoxelCoordinates);
				for (int32 RegionVoxelY = MinOutputRegionVoxelCoordinates.Y; RegionVoxelY <= MaxOutputRegionVoxelCoordinates.Y; ++RegionVoxelY)
				{
					for (int32 RegionVoxelX = MinOutputRegionVoxelCoordinates.X; RegionVoxelX <= MaxOutputRegionVoxelCoordinates.X; ++RegionVoxelX)
					{
						const int32 OutputX = MinRegionVoxelCoordinates.X + RegionVoxelX - MinVoxelCoordinates.X;
						const int32 OutputY = MinRegionVoxelCoordinates.Y + RegionVoxelY - MinVoxelCoordinates.Y;
						const int32 OutputMinZ = MinRegionVoxelCoordinates.Z + MinOutputRegionVoxelCoordinates.Z - MinVoxelCoordinates.Z;
						const int32 OutputSizeZ = MaxOutputRegionVoxelCoordinates.Z - MinOutputRegionVoxelCoordinates.Z + 1;
						const uint32 OutputBaseVoxelIndex = (OutputY * OutputSize.X + OutputX) * OutputSize.Z + OutputMinZ;
						const uint32 RegionBaseVoxelIndex = (((RegionVoxelY << Parameters.VoxelsPerRegionLog2.X) + RegionVoxelX) << Parameters.VoxelsPerRegionLog2.Z) + MinOutputRegionVoxelCoordinates.Z;
						if (RegionIndex)
						{
							FMemory::Memcpy(&OutVoxelMaterials[OutputBaseVoxelIndex], &Regions[*RegionIndex].VoxelContents[RegionBaseVoxelIndex], OutputSizeZ * sizeof(uint8));
						}
						else
						{
							FMemory::Memset(&OutVoxelMaterials[OutputBaseVoxelIndex], Parameters.EmptyMaterialIndex, OutputSizeZ * sizeof(uint8));
						}
					}
				}
			}
		}
	}
}

void UVoxelLandscapeComponent::SetVoxelMaterialArray(const FInt3& MinVoxelCoordinates, const FInt3& MaxVoxelCoordinates, const TArray<uint8>& VoxelMaterials)
{
	const FInt3 InputSize = MaxVoxelCoordinates - MinVoxelCoordinates + FInt3::Scalar(1);
	const FInt3 MinRegionCoordinates = VoxelToRegionCoordinates(MinVoxelCoordinates);
	const FInt3 MaxRegionCoordinates = VoxelToRegionCoordinates(MaxVoxelCoordinates);
	for (int32 RegionY = MinRegionCoordinates.Y; RegionY <= MaxRegionCoordinates.Y; ++RegionY)
	{
		for (int32 RegionX = MinRegionCoordinates.X; RegionX <= MaxRegionCoordinates.X; ++RegionX)
		{
			for (int32 RegionZ = MinRegionCoordinates.Z; RegionZ <= MaxRegionCoordinates.Z; ++RegionZ)
			{
				const FInt3 RegionCoordinates(RegionX, RegionY, RegionZ);
				const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
				const FInt3 MinRegionVoxelCoordinates = FInt3(RegionX, RegionY, RegionZ) * VoxelsPerRegion;
				const FInt3 MinInputRegionVoxelCoordinates = FInt3::Max(FInt3::Scalar(0), MinVoxelCoordinates - MinRegionVoxelCoordinates);
				const FInt3 MaxInputRegionVoxelCoordinates = FInt3::Min(VoxelsPerRegion - FInt3::Scalar(1), MaxVoxelCoordinates - MinRegionVoxelCoordinates);
				for (int32 RegionVoxelY = MinInputRegionVoxelCoordinates.Y; RegionVoxelY <= MaxInputRegionVoxelCoordinates.Y; ++RegionVoxelY)
				{
					for (int32 RegionVoxelX = MinInputRegionVoxelCoordinates.X; RegionVoxelX <= MaxInputRegionVoxelCoordinates.X; ++RegionVoxelX)
					{
						const int32 InputX = MinRegionVoxelCoordinates.X + RegionVoxelX - MinVoxelCoordinates.X;
						const int32 InputY = MinRegionVoxelCoordinates.Y + RegionVoxelY - MinVoxelCoordinates.Y;
						const int32 InputMinZ = MinRegionVoxelCoordinates.Z + MinInputRegionVoxelCoordinates.Z - MinVoxelCoordinates.Z;
						const int32 InputSizeZ = MaxInputRegionVoxelCoordinates.Z - MinInputRegionVoxelCoordinates.Z + 1;
						const uint32 InputBaseVoxelIndex = (InputY * InputSize.X + InputX) * InputSize.Z + InputMinZ;
						const uint32 RegionBaseVoxelIndex = (((RegionVoxelY << Parameters.VoxelsPerRegionLog2.X) + RegionVoxelX) << Parameters.VoxelsPerRegionLog2.Z) + MinInputRegionVoxelCoordinates.Z;
						if (RegionIndex)
						{
							FMemory::Memcpy(&Regions[*RegionIndex].VoxelContents[RegionBaseVoxelIndex], &VoxelMaterials[InputBaseVoxelIndex], InputSizeZ * sizeof(uint8));
						}
					}
				}
			}
		}
	}

	InvalidateChunkComponents(MinVoxelCoordinates, MaxVoxelCoordinates);
}

bool UVoxelLandscapeComponent::SetVoxel(const FInt3& VoxelCoordinates, int32 MaterialIndex)
{
	if (FInt3::All(VoxelCoordinates >= MinVoxelCoordinates) && FInt3::All(VoxelCoordinates <= MaxVoxelCoordinates) && MaterialIndex < Parameters.Materials.Num())
	{
		const FInt3 RegionCoordinates = VoxelToRegionCoordinates(VoxelCoordinates);
		const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
		if (RegionIndex != NULL)
		{
			const uint32 VoxelIndex = VoxelCoordinatesToRegionVoxelIndex(RegionCoordinates, VoxelCoordinates);
			FVoxelRegion& Region = Regions[*RegionIndex];
			Region.VoxelContents[VoxelIndex] = MaterialIndex;
			InvalidateChunkComponents(VoxelCoordinates, VoxelCoordinates);
			return true;
		}
	}
	return false;
}

void UVoxelLandscapeComponent::UpdateMaxNonEmptyVoxelMap(FVoxelRegion& Region, const FInt3 MinDirtyRegionVoxelCoordinates, const FInt3 MaxDirtyRegionVoxelCoordinates) const
{
	// Allocate the map.
	if (!Region.MaxNonEmptyVoxelRegionZs.Num())
	{
		Region.MaxNonEmptyVoxelRegionZs.SetNumUninitialized(1 << (Parameters.VoxelsPerRegionLog2.X + Parameters.VoxelsPerRegionLog2.Y));
	}

	// For each XY in the chunk, find the highest non-empty Voxel between the bottom of the chunk and the top of the Landscape.
	for (int32 RegionVoxelY = MinDirtyRegionVoxelCoordinates.Y; RegionVoxelY <= MaxDirtyRegionVoxelCoordinates.Y; ++RegionVoxelY)
	{
		for (int32 RegionVoxelX = MinDirtyRegionVoxelCoordinates.X; RegionVoxelX <= MaxDirtyRegionVoxelCoordinates.X; ++RegionVoxelX)
		{
			int32 MaxNonEmptyRegionVoxelZ = VoxelsPerRegion.Z - 1;
			for (; MaxNonEmptyRegionVoxelZ >= 0; --MaxNonEmptyRegionVoxelZ)
			{
				const uint32 RegionVoxelIndex = (((RegionVoxelY << Parameters.VoxelsPerRegionLog2.X) + RegionVoxelX) << Parameters.VoxelsPerRegionLog2.Z) + MaxNonEmptyRegionVoxelZ;
				if (Region.VoxelContents[RegionVoxelIndex] != Parameters.EmptyMaterialIndex)
				{
					break;
				}
			}
			Region.MaxNonEmptyVoxelRegionZs[(RegionVoxelY << Parameters.VoxelsPerRegionLog2.X) + RegionVoxelX] = (int8)MaxNonEmptyRegionVoxelZ;
		}
	}
}
void UVoxelLandscapeComponent::UpdateRegionComplexVoxelsMap(FVoxelRegion& Region, const FInt3 MinDirtyRegionVoxelCoordinates, const FInt3 MaxDirtyRegionVoxelCoordinates) const
{
	// For each XY in the chunk, find the highest non-empty Voxel between the bottom of the chunk and the top of the grid.
	for (int32 RegionVoxelY = MinDirtyRegionVoxelCoordinates.Y; RegionVoxelY <= MaxDirtyRegionVoxelCoordinates.Y; ++RegionVoxelY)
	{
		for (int32 RegionVoxelX = MinDirtyRegionVoxelCoordinates.X; RegionVoxelX <= MaxDirtyRegionVoxelCoordinates.X; ++RegionVoxelX)
		{
			/*
			A VARIATION OF THIS WILL BE THE WORKING VERSION
			for (int32 RegionVoxelZ = MinDirtyRegionVoxelCoordinates.Z; RegionVoxelZ <= MaxDirtyRegionVoxelCoordinates.Z; ++RegionVoxelZ)
			{
			const uint32 RegionVoxelIndex = (((RegionVoxelY << Parameters.VoxelsPerRegionLog2.X) + RegionVoxelX) << Parameters.VoxelsPerRegionLog2.Z) + RegionVoxelZ;
			if (Region.VoxelContents[RegionVoxelIndex] == 1 && RegionVoxelIndex%100==0)
			{
			Region.RegionComplexVoxelsIndexes.Add(FInt3(RegionVoxelX,RegionVoxelY,RegionVoxelZ), Region.VoxelContents[RegionVoxelIndex]);
			}
			}*/

			/*THE VERSION BELOW IS FOR TESTING FEEL FREE TO REPLACE*/
			int32 MaxNonEmptyRegionVoxelZ = VoxelsPerRegion.Z - 1;
			for (; MaxNonEmptyRegionVoxelZ >= 0; --MaxNonEmptyRegionVoxelZ)
			{
				const uint32 RegionVoxelIndex = (((RegionVoxelY << Parameters.VoxelsPerRegionLog2.X) + RegionVoxelX) << Parameters.VoxelsPerRegionLog2.Z) + MaxNonEmptyRegionVoxelZ;
				if (Region.VoxelContents[RegionVoxelIndex] == 1 && RegionVoxelIndex % 10 == 0)
				{
					Region.RegionComplexVoxelsIndexes.Add(FInt3(RegionVoxelX, RegionVoxelY, MaxNonEmptyRegionVoxelZ + 1), Region.VoxelContents[RegionVoxelIndex]);
					break;
				}
			}
			Region.MaxNonEmptyVoxelRegionZs[(RegionVoxelY << Parameters.VoxelsPerRegionLog2.X) + RegionVoxelX] = (int8)MaxNonEmptyRegionVoxelZ;
		}
	}

}

void UVoxelLandscapeComponent::GetRegionComplexVoxelsCoordinates(const FInt3 RegionCoordinates, TArray<FInt3> &outCoordinates, TArray<uint8> &outComplexVoxelIndices)
{
	const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
	if (RegionIndex && RegionCoordinates == FInt3(0, 0, 0))
	{
		UpdateRegionComplexVoxelsMap(Regions[*RegionIndex], FInt3::Scalar(0), VoxelsPerRegion - FInt3::Scalar(1));
		Regions[*RegionIndex].RegionComplexVoxelsIndexes.GenerateKeyArray(outCoordinates);
		Regions[*RegionIndex].RegionComplexVoxelsIndexes.GenerateValueArray(outComplexVoxelIndices);

		UE_LOG(LogStats, Log, TEXT("ORIGENP %d RegionComplexVoxelsIndexes key value num %d %d"), Regions[*RegionIndex].RegionComplexVoxelsIndexes.Num(), outCoordinates.Num(), outComplexVoxelIndices.Num());
	}

}
void UVoxelLandscapeComponent::GetMaxNonEmptyVoxelZ(const FInt3& MinVoxelCoordinates, const FInt3& MaxVoxelCoordinates, TArray<int8>& OutHeightMap) const
{
	const FInt3 OutputSize = MaxVoxelCoordinates - MinVoxelCoordinates + FInt3::Scalar(1);
	const FInt3 MinRegionCoordinates = VoxelToRegionCoordinates(MinVoxelCoordinates);
	const FInt3 MaxRegionCoordinates = VoxelToRegionCoordinates(MaxVoxelCoordinates);
	const FInt3 TopMaxRegionCoordinates = FInt3(MaxRegionCoordinates.X, MaxRegionCoordinates.Y, Parameters.MaxRegionCoordinates.Z);
	for (int32 RegionY = MinRegionCoordinates.Y; RegionY <= TopMaxRegionCoordinates.Y; ++RegionY)
	{
		for (int32 RegionX = MinRegionCoordinates.X; RegionX <= TopMaxRegionCoordinates.X; ++RegionX)
		{
			TArray<const FVoxelRegion*> ZRegions;
			ZRegions.Empty(TopMaxRegionCoordinates.Z - MinRegionCoordinates.Z + 1);
			for (int32 RegionZ = MinRegionCoordinates.Z; RegionZ <= TopMaxRegionCoordinates.Z; ++RegionZ)
			{
				const FInt3 RegionCoordinates(RegionX, RegionY, RegionZ);
				const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
				if (RegionIndex)
				{
					ZRegions.Add(&Regions[*RegionIndex]);
				}
			}
			const FInt3 MinRegionVoxelCoordinates = FInt3(RegionX, RegionY, MinRegionCoordinates.Z) * VoxelsPerRegion;
			const FInt3 MinOutputRegionVoxelCoordinates = FInt3::Max(FInt3::Scalar(0), MinVoxelCoordinates - MinRegionVoxelCoordinates);
			const FInt3 MaxOutputRegionVoxelCoordinates = FInt3::Min(VoxelsPerRegion - FInt3::Scalar(1), MaxVoxelCoordinates - MinRegionVoxelCoordinates);
			for (int32 RegionVoxelY = MinOutputRegionVoxelCoordinates.Y; RegionVoxelY <= MaxOutputRegionVoxelCoordinates.Y; ++RegionVoxelY)
			{
				for (int32 RegionVoxelX = MinOutputRegionVoxelCoordinates.X; RegionVoxelX <= MaxOutputRegionVoxelCoordinates.X; ++RegionVoxelX)
				{
					int32 MaxNonEmptyVoxelZ = UVoxelLandscapeComponent::MinVoxelCoordinates.Z - 1;
					for (int32 RegionZIndex = ZRegions.Num() - 1; RegionZIndex >= 0; --RegionZIndex)
					{
						const FVoxelRegion& Region = *ZRegions[RegionZIndex];
						const int8 RegionMaxNonEmptyZ = Region.MaxNonEmptyVoxelRegionZs[(RegionVoxelY << Parameters.VoxelsPerRegionLog2.X) + RegionVoxelX];
						if (RegionMaxNonEmptyZ != -1)
						{
							MaxNonEmptyVoxelZ = Region.Coordinates.Z * VoxelsPerRegion.Z + (int32)RegionMaxNonEmptyZ;
							break;
						}
					}

					const int32 OutputX = MinRegionVoxelCoordinates.X + RegionVoxelX - MinVoxelCoordinates.X;
					const int32 OutputY = MinRegionVoxelCoordinates.Y + RegionVoxelY - MinVoxelCoordinates.Y;
					OutHeightMap[OutputY * OutputSize.X + OutputX] = (int8)FMath::Clamp(MaxNonEmptyVoxelZ - MinVoxelCoordinates.Z, -1, 127);
				}
			}
		}
	}
}

void UVoxelLandscapeComponent::InvalidateChunkComponents(const FInt3& MinVoxelCoordinates, const FInt3& MaxVoxelCoordinates)
{
	// Expand the Voxel box by 1 Voxel so that Voxels facing the one being invalidated are also updated.
	const FInt3 FacingExpansionExtent = FInt3::Scalar(1);

	// Update the region non-empty Voxel max Z maps.
	const FInt3 MinRegionCoordinates = VoxelToRegionCoordinates(MinVoxelCoordinates);
	const FInt3 MaxRegionCoordinates = VoxelToRegionCoordinates(MaxVoxelCoordinates);
	for (int32 RegionZ = Parameters.MinRegionCoordinates.Z; RegionZ <= MaxRegionCoordinates.Z; ++RegionZ)
	{
		for (int32 RegionY = MinRegionCoordinates.Y; RegionY <= MaxRegionCoordinates.Y; ++RegionY)
		{
			for (int32 RegionX = MinRegionCoordinates.X; RegionX <= MaxRegionCoordinates.X; ++RegionX)
			{
				const FInt3 RegionCoordinates(RegionX, RegionY, RegionZ);
				const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
				if (RegionIndex)
				{
					const FInt3 MinRegionVoxelCoordinates = RegionCoordinates * VoxelsPerRegion;
					const FInt3 MinDirtyRegionVoxelCoordinates = FInt3::Max(FInt3::Scalar(0), MinVoxelCoordinates - MinRegionVoxelCoordinates);
					const FInt3 MaxDirtyRegionVoxelCoordinates = FInt3::Min(VoxelsPerRegion - FInt3::Scalar(1), MaxVoxelCoordinates - MinRegionVoxelCoordinates);
					UpdateMaxNonEmptyVoxelMap(Regions[*RegionIndex], MinDirtyRegionVoxelCoordinates, MaxDirtyRegionVoxelCoordinates);
				}
			}
		}
	}

	// Invalidate render components. Note that because of ambient occlusion, the render chunks need to be invalidated all the way to the bottom of the Landscape!
	const FInt3 AmbientOcclusionExpansionExtent = FInt3::Scalar(Parameters.AmbientOcclusionBlurRadius);
	const FInt3 RenderExpansionExtent = AmbientOcclusionExpansionExtent + FacingExpansionExtent;
	const FInt3 MinRenderChunkCoordinates = VoxelToRenderChunkCoordinates(MinVoxelCoordinates - RenderExpansionExtent);
	const FInt3 MaxRenderChunkCoordinates = VoxelToRenderChunkCoordinates(MaxVoxelCoordinates + RenderExpansionExtent);
	for (int32 ChunkX = MinRenderChunkCoordinates.X; ChunkX <= MaxRenderChunkCoordinates.X; ++ChunkX)
	{
		for (int32 ChunkY = MinRenderChunkCoordinates.Y; ChunkY <= MaxRenderChunkCoordinates.Y; ++ChunkY)
		{
			for (int32 ChunkZ = UVoxelLandscapeComponent::MinVoxelCoordinates.Z; ChunkZ <= MaxRenderChunkCoordinates.Z; ++ChunkZ)
			{
				UVoxelRenderComponent* RenderComponent = RenderChunkCoordinatesToComponent.FindRef(FInt3(ChunkX, ChunkY, ChunkZ));
				if (RenderComponent)
				{
					if (ChunkZ >= MinRenderChunkCoordinates.Z)
					{
						RenderComponent->MarkRenderStateDirty();
					}
					else
					{
						// If the chunk only needs to be invalidate to update its ambient occlusion, defer it as a low priority update.
						RenderComponent->HasLowPriorityUpdatePending = true;
					}
				}
			}
		}
	}

	// Invalidate collision components.
	const FInt3 MinCollisionChunkCoordinates = VoxelToCollisionChunkCoordinates(MinVoxelCoordinates - FacingExpansionExtent);
	const FInt3 MaxCollisionChunkCoordinates = VoxelToCollisionChunkCoordinates(MaxVoxelCoordinates + FacingExpansionExtent);
	for (int32 ChunkX = MinCollisionChunkCoordinates.X; ChunkX <= MaxCollisionChunkCoordinates.X; ++ChunkX)
	{
		for (int32 ChunkY = MinCollisionChunkCoordinates.Y; ChunkY <= MaxCollisionChunkCoordinates.Y; ++ChunkY)
		{
			for (int32 ChunkZ = MinCollisionChunkCoordinates.Z; ChunkZ <= MaxCollisionChunkCoordinates.Z; ++ChunkZ)
			{
				UVoxelCollisionComponent* CollisionComponent = CollisionChunkCoordinatesToComponent.FindRef(FInt3(ChunkX, ChunkY, ChunkZ));
				if (CollisionComponent)
				{
					CollisionComponent->MarkRenderStateDirty();
				}
			}
		}
	}
}

void UVoxelLandscapeComponent::Update(const FVector& WorldViewPosition, float MaxDrawDistance, float MaxCollisionDistance, float MaxDesiredUpdateTime, FVoxelLandscape_InitRegion OnInitRegion)
{
	const FVector LocalViewPosition = GetComponentTransform().InverseTransformPosition(WorldViewPosition);
	const float LocalMaxDrawDistance = FMath::Max(0.0f, MaxDrawDistance / GetComponentTransform().GetScale3D().GetMin());
	const float LocalMaxCollisionDistance = FMath::Max(0.0f, MaxCollisionDistance / GetComponentTransform().GetScale3D().GetMin());
	const float LocalMaxDrawAndCollisionDistance = FMath::Max(LocalMaxDrawDistance, LocalMaxCollisionDistance);

	const double StartTime = FPlatformTime::Seconds();

	// Initialize any regions that are closer to the viewer than the draw or collision distance.
	// Include an additional ring of regions around what is being drawn or colliding so it has plenty of frames to spread initialization over before the data is needed.r
	const FInt3 MinInitRegionCoordinates = FInt3::Max(Parameters.MinRegionCoordinates, VoxelToRegionCoordinates(FInt3::Floor(LocalViewPosition - FVector(LocalMaxDrawAndCollisionDistance))) - FInt3::Scalar(1));
	const FInt3 MaxInitRegionCoordinates = FInt3::Min(Parameters.MaxRegionCoordinates, VoxelToRegionCoordinates(FInt3::Ceil(LocalViewPosition + FVector(LocalMaxDrawAndCollisionDistance))) + FInt3::Scalar(1));
	const float RegionExpansionRadius = VoxelsPerRegion.ToFloat().GetMin();
	for (int32 RegionZ = MinInitRegionCoordinates.Z; RegionZ <= MaxInitRegionCoordinates.Z && (FPlatformTime::Seconds() - StartTime) < MaxDesiredUpdateTime; ++RegionZ)
	{
		for (int32 RegionY = MinInitRegionCoordinates.Y; RegionY <= MaxInitRegionCoordinates.Y && (FPlatformTime::Seconds() - StartTime) < MaxDesiredUpdateTime; ++RegionY)
		{
			for (int32 RegionX = MinInitRegionCoordinates.X; RegionX <= MaxInitRegionCoordinates.X && (FPlatformTime::Seconds() - StartTime) < MaxDesiredUpdateTime; ++RegionX)
			{
				const FInt3 RegionCoordinates(RegionX, RegionY, RegionZ);
				const FBox RegionBounds((RegionCoordinates * VoxelsPerRegion).ToFloat(), ((RegionCoordinates + FInt3::Scalar(1)) * VoxelsPerRegion).ToFloat());
				if (RegionBounds.ComputeSquaredDistanceToPoint(LocalViewPosition) < FMath::Square(LocalMaxDrawAndCollisionDistance + RegionExpansionRadius))
				{
					const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
					if (!RegionIndex)
					{
						const int32 RegionIndex = Regions.Num();
						FVoxelRegion& Region = *new(Regions) FVoxelRegion;
						Region.Coordinates = RegionCoordinates;

						// Initialize the region's Voxels to the empty material.
						Region.VoxelContents.Init(Parameters.EmptyMaterialIndex, 1 << Parameters.VoxelsPerRegionLog2.SumComponents());

						// Compute the region's non-empty height map.
						UpdateMaxNonEmptyVoxelMap(Region, FInt3::Scalar(0), VoxelsPerRegion - FInt3::Scalar(1));

						// Add the region to the coordinate map.
						RegionCoordinatesToIndex.Add(RegionCoordinates, RegionIndex);

						// Call the InitRegion delegate for the new region.
						OnInitRegion.Execute(RegionCoordinates);
					}
				}
			}
		}
	}

	// Create render components for any chunks closer to the viewer than the draw distance, and destroy any that are no longer inside the draw distance.
	// Do this visibility check in 2D so the chunks underneath those on the horizon are also drawn even if they are too far.
	const FInt3 MinRenderChunkCoordinates = VoxelToRenderChunkCoordinates(FInt3::Max(MinVoxelCoordinates, FInt3::Floor(LocalViewPosition - FVector(LocalMaxDrawDistance))));
	const FInt3 MaxRenderChunkCoordinates = VoxelToRenderChunkCoordinates(FInt3::Min(MaxVoxelCoordinates, FInt3::Ceil(LocalViewPosition + FVector(LocalMaxDrawDistance))));
	for (auto ChunkIt = RenderChunkCoordinatesToComponent.CreateIterator(); ChunkIt; ++ChunkIt)
	{
		const FInt3 MinChunkVoxelCoordinates = ChunkIt.Key() * VoxelsPerRenderChunk;
		const FInt3 MaxChunkVoxelCoordinates = MinChunkVoxelCoordinates + VoxelsPerRenderChunk - FInt3::Scalar(1);
		const FBox ChunkBounds(
			FInt3(MinChunkVoxelCoordinates.X, MinChunkVoxelCoordinates.Y, MinVoxelCoordinates.Z).ToFloat(),
			FInt3(MinChunkVoxelCoordinates.X, MinChunkVoxelCoordinates.Y, MaxVoxelCoordinates.Z).ToFloat()
		);
		if (ChunkBounds.ComputeSquaredDistanceToPoint(LocalViewPosition) > FMath::Square(LocalMaxDrawDistance))
		{
			ChunkIt.Value()->DetachFromParent();
			ChunkIt.Value()->DestroyComponent();
			ChunkIt.RemoveCurrent();
		}
	}
	for (int32 ChunkZ = VoxelToRenderChunkCoordinates(MinVoxelCoordinates).Z; ChunkZ <= VoxelToRenderChunkCoordinates(MaxVoxelCoordinates).Z && (FPlatformTime::Seconds() - StartTime) < MaxDesiredUpdateTime; ++ChunkZ)
	{
		for (int32 ChunkY = MinRenderChunkCoordinates.Y; ChunkY <= MaxRenderChunkCoordinates.Y && (FPlatformTime::Seconds() - StartTime) < MaxDesiredUpdateTime; ++ChunkY)
		{
			for (int32 ChunkX = MinRenderChunkCoordinates.X; ChunkX <= MaxRenderChunkCoordinates.X && (FPlatformTime::Seconds() - StartTime) < MaxDesiredUpdateTime; ++ChunkX)
			{
				const FInt3 ChunkCoordinates(ChunkX, ChunkY, ChunkZ);
				const FInt3 MinChunkVoxelCoordinates = ChunkCoordinates * VoxelsPerRenderChunk;
				const FInt3 MaxChunkVoxelCoordinates = MinChunkVoxelCoordinates + VoxelsPerRenderChunk - FInt3::Scalar(1);
				const FBox ChunkBounds(
					FInt3(MinChunkVoxelCoordinates.X, MinChunkVoxelCoordinates.Y, MinVoxelCoordinates.Z).ToFloat(),
					FInt3(MinChunkVoxelCoordinates.X, MinChunkVoxelCoordinates.Y, MaxVoxelCoordinates.Z).ToFloat()
				);
				if (ChunkBounds.ComputeSquaredDistanceToPoint(LocalViewPosition) < FMath::Square(LocalMaxDrawDistance))
				{
					UVoxelRenderComponent* RenderComponent = RenderChunkCoordinatesToComponent.FindRef(ChunkCoordinates);
					if (!RenderComponent)
					{
						// Initialize a new chunk component.
						RenderComponent = NewObject<UVoxelRenderComponent>(GetOwner());
						RenderComponent->Landscape = this;
						RenderComponent->Coordinates = ChunkCoordinates;

						// Set the component transform and register it.
						RenderComponent->SetRelativeLocation((ChunkCoordinates * VoxelsPerRenderChunk).ToFloat());
						RenderComponent->AttachTo(this);
						RenderComponent->RegisterComponent();

						// Add the chunk to the coordinate map and visible chunk array.
						RenderChunkCoordinatesToComponent.Add(ChunkCoordinates, RenderComponent);
					}

					// Flush low-priority pending updates to render components.
					if (RenderComponent->HasLowPriorityUpdatePending)
					{
						RenderComponent->MarkRenderStateDirty();
						RenderComponent->HasLowPriorityUpdatePending = false;
					}
				}
			}
		}
	}

	// Create collision components for any chunks closer to the viewer than the collision distance, and destroy any that are no longer inside the draw distance.
	const FInt3 MinCollisionChunkCoordinates = VoxelToCollisionChunkCoordinates(FInt3::Max(MinVoxelCoordinates, FInt3::Floor(LocalViewPosition - FVector(LocalMaxCollisionDistance))));
	const FInt3 MaxCollisionChunkCoordinates = VoxelToCollisionChunkCoordinates(FInt3::Min(MaxVoxelCoordinates, FInt3::Ceil(LocalViewPosition + FVector(LocalMaxCollisionDistance))));
	const float LocalCollisionChunkRadius = VoxelsPerCollisionChunk.ToFloat().Size();
	for (auto ChunkIt = CollisionChunkCoordinatesToComponent.CreateIterator(); ChunkIt; ++ChunkIt)
	{
		const FBox ChunkBounds((ChunkIt.Key() * VoxelsPerCollisionChunk).ToFloat(), ((ChunkIt.Key() + FInt3::Scalar(1)) * VoxelsPerCollisionChunk).ToFloat());
		if (FInt3::Any(ChunkIt.Key() < MinCollisionChunkCoordinates)
			|| FInt3::Any(ChunkIt.Key() > MaxCollisionChunkCoordinates)
			|| ChunkBounds.ComputeSquaredDistanceToPoint(LocalViewPosition) > FMath::Square(LocalMaxCollisionDistance))
		{
			ChunkIt.Value()->DetachFromParent();
			ChunkIt.Value()->DestroyComponent();
			ChunkIt.RemoveCurrent();
		}
	}
	for (int32 ChunkZ = MinCollisionChunkCoordinates.Z; ChunkZ <= MaxCollisionChunkCoordinates.Z; ++ChunkZ)
	{
		for (int32 ChunkY = MinCollisionChunkCoordinates.Y; ChunkY <= MaxCollisionChunkCoordinates.Y; ++ChunkY)
		{
			for (int32 ChunkX = MinCollisionChunkCoordinates.X; ChunkX <= MaxCollisionChunkCoordinates.X; ++ChunkX)
			{
				const FInt3 ChunkCoordinates(ChunkX, ChunkY, ChunkZ);
				const FBox ChunkBounds((ChunkCoordinates * VoxelsPerCollisionChunk).ToFloat(), ((ChunkCoordinates + FInt3::Scalar(1)) * VoxelsPerCollisionChunk).ToFloat());
				if (ChunkBounds.ComputeSquaredDistanceToPoint(LocalViewPosition) < FMath::Square(LocalMaxCollisionDistance))
				{
					if (!CollisionChunkCoordinatesToComponent.FindRef(ChunkCoordinates))
					{
						// Initialize a new chunk component.
						UVoxelCollisionComponent* Chunk = NewObject<UVoxelCollisionComponent>(GetOwner());
						Chunk->Landscape = this;
						Chunk->Coordinates = ChunkCoordinates;

						// Set the component transform and register it.
						Chunk->SetRelativeLocation((ChunkCoordinates * VoxelsPerCollisionChunk).ToFloat());
						Chunk->AttachTo(this);
						Chunk->RegisterComponent();

						// Add the chunk to the coordinate map and visible chunk array.
						CollisionChunkCoordinatesToComponent.Add(ChunkCoordinates, Chunk);
					}
				}
			}
		}
	}
}

FBoxSphereBounds UVoxelLandscapeComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	// Return a bounds that fills the world.
	return FBoxSphereBounds(FVector(0, 0, 0), FVector(1, 1, 1) * HALF_WORLD_MAX, FMath::Sqrt(3.0f * HALF_WORLD_MAX));
}

FVoxelLandscapeParameters::FVoxelLandscapeParameters()
	: EmptyMaterialIndex(0)
	, VoxelsPerRegionLog2(5, 5, 7)
	, RenderChunksPerRegionLog2(0, 0, 2)
	, CollisionChunksPerRegionLog2(1, 1, 2)
	, MinRegionCoordinates(-1024, -1024, 0)
	, MaxRegionCoordinates(+1024, +1024, 0)
	, AmbientOcclusionBlurRadius(2)
{
	Materials.Add(FVoxelMaterial());
}

UVoxelLandscapeComponent::UVoxelLandscapeComponent(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = true;

	Init(Parameters);
}

void UVoxelLandscapeComponent::OnRegister()
{
	Super::OnRegister();

	for (auto ChunkIt = RenderChunkCoordinatesToComponent.CreateConstIterator(); ChunkIt; ++ChunkIt)
	{
		ChunkIt.Value()->RegisterComponent();
	}
	for (auto ChunkIt = CollisionChunkCoordinatesToComponent.CreateConstIterator(); ChunkIt; ++ChunkIt)
	{
		ChunkIt.Value()->RegisterComponent();
	}
}

void UVoxelLandscapeComponent::OnUnregister()
{
	for (auto ChunkIt = RenderChunkCoordinatesToComponent.CreateConstIterator(); ChunkIt; ++ChunkIt)
	{
		ChunkIt.Value()->UnregisterComponent();
	}
	for (auto ChunkIt = CollisionChunkCoordinatesToComponent.CreateConstIterator(); ChunkIt; ++ChunkIt)
	{
		ChunkIt.Value()->UnregisterComponent();
	}

	Super::OnUnregister();
}