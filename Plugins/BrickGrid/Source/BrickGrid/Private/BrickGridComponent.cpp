// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickGridPluginPrivatePCH.h"
#include "ComponentReregisterContext.h"
#include "BrickRenderComponent.h"
#include "BrickCollisionComponent.h"
#include "BrickGridComponent.h"

void UBrickGridComponent::Init(const FBrickGridParameters& InParameters)
{
	Parameters = InParameters;

	// Validate the empty material index.
	Parameters.EmptyMaterialIndex = FMath::Clamp<int32>(Parameters.EmptyMaterialIndex, 0, Parameters.Materials.Num() - 1);

	// Limit each region to 128x128x128 bricks, which is the largest power of 2 size that can be rendered using 8-bit relative vertex positions.
	Parameters.BricksPerRegionLog2 = FInt3::Clamp(Parameters.BricksPerRegionLog2, FInt3::Scalar(0), FInt3::Scalar(BrickGridConstants::MaxBricksPerRegionAxisLog2));

	// Don't allow fractional chunks/region, or chunks smaller than one brick.
	Parameters.RenderChunksPerRegionLog2 = FInt3::Clamp(Parameters.RenderChunksPerRegionLog2, FInt3::Scalar(0), Parameters.BricksPerRegionLog2);
	Parameters.CollisionChunksPerRegionLog2 = FInt3::Clamp(Parameters.CollisionChunksPerRegionLog2, FInt3::Scalar(0), Parameters.BricksPerRegionLog2);

	// Derive the chunk and region sizes from the log2 inputs.
	BricksPerRenderChunkLog2 = Parameters.BricksPerRegionLog2 - Parameters.RenderChunksPerRegionLog2;
	BricksPerCollisionChunkLog2 = Parameters.BricksPerRegionLog2 - Parameters.CollisionChunksPerRegionLog2;
	BricksPerRegion = BricksPerCollisionChunk = FInt3::Exp2(Parameters.BricksPerRegionLog2);
	BricksPerRenderChunk = FInt3::Exp2(BricksPerRenderChunkLog2);
	BricksPerCollisionChunk = FInt3::Exp2(BricksPerCollisionChunkLog2);
	RenderChunksPerRegion = FInt3::Exp2(Parameters.RenderChunksPerRegionLog2);
	CollisionChunksPerRegion = FInt3::Exp2(Parameters.CollisionChunksPerRegionLog2);
	BricksPerRegion = FInt3::Exp2(Parameters.BricksPerRegionLog2);

	// Clamp the min/max region coordinates to keep brick coordinates within 32-bit signed integers.
	Parameters.MinRegionCoordinates = FInt3::Max(Parameters.MinRegionCoordinates, FInt3::Scalar(INT_MIN) / BricksPerRegion);
	Parameters.MaxRegionCoordinates = FInt3::Min(Parameters.MaxRegionCoordinates, (FInt3::Scalar(INT_MAX) - BricksPerRegion + FInt3::Scalar(1)) / BricksPerRegion);
	MinBrickCoordinates = Parameters.MinRegionCoordinates * BricksPerRegion;
	MaxBrickCoordinates = Parameters.MaxRegionCoordinates * BricksPerRegion + BricksPerRegion - FInt3::Scalar(1);

	// Limit the ambient occlusion blur radius to be a positive value.
	Parameters.AmbientOcclusionBlurRadius = FMath::Max(0,Parameters.AmbientOcclusionBlurRadius);

	// Reset the regions and reregister the component.
	FComponentReregisterContext ReregisterContext(this);
	Regions.Empty();
	RegionCoordinatesToIndex.Empty();
	for(auto ChunkIt = RenderChunkCoordinatesToComponent.CreateConstIterator();ChunkIt;++ChunkIt)
	{
		ChunkIt.Value()->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepRelative,false));
		ChunkIt.Value()->DestroyComponent();
	}
	for(auto ChunkIt = CollisionChunkCoordinatesToComponent.CreateConstIterator();ChunkIt;++ChunkIt)
	{
		ChunkIt.Value()->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepRelative,false));
		ChunkIt.Value()->DestroyComponent();
	}
	RenderChunkCoordinatesToComponent.Empty();
	CollisionChunkCoordinatesToComponent.Empty();
}

FBrickGridData UBrickGridComponent::GetData() const
{
	FBrickGridData Result;
	Result.Regions = Regions;
	return Result;
}

void UBrickGridComponent::SetData(const FBrickGridData& Data)
{
	Init(Parameters);
	Regions = Data.Regions;

	for(auto RegionIt = Regions.CreateIterator();RegionIt;++RegionIt)
	{
		// Compute the max non-empty brick map for the new regions.
		UpdateMaxNonEmptyBrickMap(*RegionIt,FInt3::Scalar(0),BricksPerRegion - FInt3::Scalar(1));

		// Recreate the region coordinate to index map.
		RegionCoordinatesToIndex.Add(RegionIt->Coordinates,RegionIt.GetIndex());
	}
}

FBrick UBrickGridComponent::GetBrick(const FInt3& BrickCoordinates) const
{
	if(FInt3::All(BrickCoordinates >= MinBrickCoordinates) && FInt3::All(BrickCoordinates <= MaxBrickCoordinates))
	{
		const FInt3 RegionCoordinates = BrickToRegionCoordinates(BrickCoordinates);
		const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
		if(RegionIndex != NULL)
		{
			const uint32 BrickIndex = BrickCoordinatesToRegionBrickIndex(RegionCoordinates,BrickCoordinates);
			const FBrickRegion& Region = Regions[*RegionIndex];
			return FBrick(Region.BrickContents[BrickIndex]);
		}
	}
	return FBrick(Parameters.EmptyMaterialIndex);
}

void UBrickGridComponent::GetBrickMaterialArray(const FInt3& GetMinBrickCoordinates,const FInt3& GetMaxBrickCoordinates,TArray<uint8>& OutBrickMaterials) const
{
	const FInt3 OutputSize = GetMaxBrickCoordinates - GetMinBrickCoordinates + FInt3::Scalar(1);
	const FInt3 GetMinRegionCoordinates = BrickToRegionCoordinates(GetMinBrickCoordinates);
	const FInt3 GetMaxRegionCoordinates = BrickToRegionCoordinates(GetMaxBrickCoordinates);
	for(int32 RegionY = GetMinRegionCoordinates.Y;RegionY <= GetMaxRegionCoordinates.Y;++RegionY)
	{
		for(int32 RegionX = GetMinRegionCoordinates.X;RegionX <= GetMaxRegionCoordinates.X;++RegionX)
		{
			for(int32 RegionZ = GetMinRegionCoordinates.Z;RegionZ <= GetMaxRegionCoordinates.Z;++RegionZ)
			{
				const FInt3 RegionCoordinates(RegionX,RegionY,RegionZ);
				const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
				const FInt3 MinRegionBrickCoordinates = FInt3(RegionX,RegionY,RegionZ) * BricksPerRegion;
				const FInt3 MinOutputRegionBrickCoordinates = FInt3::Max(FInt3::Scalar(0),GetMinBrickCoordinates - MinRegionBrickCoordinates);
				const FInt3 MaxOutputRegionBrickCoordinates = FInt3::Min(BricksPerRegion - FInt3::Scalar(1),GetMaxBrickCoordinates - MinRegionBrickCoordinates);
				for(int32 RegionBrickY = MinOutputRegionBrickCoordinates.Y;RegionBrickY <= MaxOutputRegionBrickCoordinates.Y;++RegionBrickY)
				{
					for(int32 RegionBrickX = MinOutputRegionBrickCoordinates.X;RegionBrickX <= MaxOutputRegionBrickCoordinates.X;++RegionBrickX)
					{
						const int32 OutputX = MinRegionBrickCoordinates.X + RegionBrickX - GetMinBrickCoordinates.X;
						const int32 OutputY = MinRegionBrickCoordinates.Y + RegionBrickY - GetMinBrickCoordinates.Y;
						const int32 OutputMinZ = MinRegionBrickCoordinates.Z + MinOutputRegionBrickCoordinates.Z - GetMinBrickCoordinates.Z;
						const int32 OutputSizeZ = MaxOutputRegionBrickCoordinates.Z - MinOutputRegionBrickCoordinates.Z + 1;
						const uint32 OutputBaseBrickIndex = (OutputY * OutputSize.X + OutputX) * OutputSize.Z + OutputMinZ;
						const uint32 RegionBaseBrickIndex = (((RegionBrickY << Parameters.BricksPerRegionLog2.X) + RegionBrickX) << Parameters.BricksPerRegionLog2.Z) + MinOutputRegionBrickCoordinates.Z;
						if(RegionIndex)
						{
							FMemory::Memcpy(&OutBrickMaterials[OutputBaseBrickIndex],&Regions[*RegionIndex].BrickContents[RegionBaseBrickIndex],OutputSizeZ * sizeof(uint8));
						}
						else
						{
							FMemory::Memset(&OutBrickMaterials[OutputBaseBrickIndex],Parameters.EmptyMaterialIndex,OutputSizeZ * sizeof(uint8));
						}
					}
				}
			}
		}
	}
}

void UBrickGridComponent::SetBrickMaterialArray(const FInt3& SetMinBrickCoordinates,const FInt3& SetMaxBrickCoordinates,const TArray<uint8>& BrickMaterials)
{
	const FInt3 InputSize = SetMaxBrickCoordinates - SetMinBrickCoordinates + FInt3::Scalar(1);
	const FInt3 SetMinRegionCoordinates = BrickToRegionCoordinates(SetMinBrickCoordinates);
	const FInt3 SetMaxRegionCoordinates = BrickToRegionCoordinates(SetMaxBrickCoordinates);
	for(int32 RegionY = SetMinRegionCoordinates.Y;RegionY <= SetMaxRegionCoordinates.Y;++RegionY)
	{
		for(int32 RegionX = SetMinRegionCoordinates.X;RegionX <= SetMaxRegionCoordinates.X;++RegionX)
		{
			for(int32 RegionZ = SetMinRegionCoordinates.Z;RegionZ <= SetMaxRegionCoordinates.Z;++RegionZ)
			{
				const FInt3 RegionCoordinates(RegionX,RegionY,RegionZ);
				const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
				const FInt3 MinRegionBrickCoordinates = FInt3(RegionX,RegionY,RegionZ) * BricksPerRegion;
				const FInt3 MinInputRegionBrickCoordinates = FInt3::Max(FInt3::Scalar(0),SetMinBrickCoordinates - MinRegionBrickCoordinates);
				const FInt3 MaxInputRegionBrickCoordinates = FInt3::Min(BricksPerRegion - FInt3::Scalar(1),SetMaxBrickCoordinates - MinRegionBrickCoordinates);
				for(int32 RegionBrickY = MinInputRegionBrickCoordinates.Y;RegionBrickY <= MaxInputRegionBrickCoordinates.Y;++RegionBrickY)
				{
					for(int32 RegionBrickX = MinInputRegionBrickCoordinates.X;RegionBrickX <= MaxInputRegionBrickCoordinates.X;++RegionBrickX)
					{
						const int32 InputX = MinRegionBrickCoordinates.X + RegionBrickX - SetMinBrickCoordinates.X;
						const int32 InputY = MinRegionBrickCoordinates.Y + RegionBrickY - SetMinBrickCoordinates.Y;
						const int32 InputMinZ = MinRegionBrickCoordinates.Z + MinInputRegionBrickCoordinates.Z - SetMinBrickCoordinates.Z;
						const int32 InputSizeZ = MaxInputRegionBrickCoordinates.Z - MinInputRegionBrickCoordinates.Z + 1;
						const uint32 InputBaseBrickIndex = (InputY * InputSize.X + InputX) * InputSize.Z + InputMinZ;
						const uint32 RegionBaseBrickIndex = (((RegionBrickY << Parameters.BricksPerRegionLog2.X) + RegionBrickX) << Parameters.BricksPerRegionLog2.Z) + MinInputRegionBrickCoordinates.Z;
						if(RegionIndex)
						{
							FMemory::Memcpy(&Regions[*RegionIndex].BrickContents[RegionBaseBrickIndex],&BrickMaterials[InputBaseBrickIndex],InputSizeZ * sizeof(uint8));
						}
					}
				}
			}
		}
	}

	InvalidateChunkComponents(SetMinBrickCoordinates,SetMaxBrickCoordinates);
}

bool UBrickGridComponent::SetBrick(const FInt3& BrickCoordinates, int32 MaterialIndex)
{
	if(FInt3::All(BrickCoordinates >= MinBrickCoordinates) && FInt3::All(BrickCoordinates <= MaxBrickCoordinates) && MaterialIndex < Parameters.Materials.Num())
	{
		const FInt3 RegionCoordinates = BrickToRegionCoordinates(BrickCoordinates);
		const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
		if (RegionIndex != NULL)
		{
			const uint32 BrickIndex = BrickCoordinatesToRegionBrickIndex(RegionCoordinates,BrickCoordinates);
			FBrickRegion& Region = Regions[*RegionIndex];
			Region.BrickContents[BrickIndex] = MaterialIndex;
			InvalidateChunkComponents(BrickCoordinates,BrickCoordinates);
			return true;
		}
	}
	return false;
}

void UBrickGridComponent::UpdateMaxNonEmptyBrickMap(FBrickRegion& Region,const FInt3 MinDirtyRegionBrickCoordinates,const FInt3 MaxDirtyRegionBrickCoordinates) const
{
	// Allocate the map.
	if(!Region.MaxNonEmptyBrickRegionZs.Num())
	{
		Region.MaxNonEmptyBrickRegionZs.SetNumUninitialized(1 << (Parameters.BricksPerRegionLog2.X + Parameters.BricksPerRegionLog2.Y));
	}

	// For each XY in the chunk, find the highest non-empty brick between the bottom of the chunk and the top of the grid.
	for(int32 RegionBrickY = MinDirtyRegionBrickCoordinates.Y;RegionBrickY <= MaxDirtyRegionBrickCoordinates.Y;++RegionBrickY)
	{
		for(int32 RegionBrickX = MinDirtyRegionBrickCoordinates.X;RegionBrickX <= MaxDirtyRegionBrickCoordinates.X;++RegionBrickX)
		{
			int32 MaxNonEmptyRegionBrickZ = BricksPerRegion.Z - 1;
			for(;MaxNonEmptyRegionBrickZ >= 0;--MaxNonEmptyRegionBrickZ)
			{
				const uint32 RegionBrickIndex = (((RegionBrickY << Parameters.BricksPerRegionLog2.X) + RegionBrickX) << Parameters.BricksPerRegionLog2.Z) + MaxNonEmptyRegionBrickZ;
				if(Region.BrickContents[RegionBrickIndex] != Parameters.EmptyMaterialIndex)
				{
					break;
				}
			}
			Region.MaxNonEmptyBrickRegionZs[(RegionBrickY << Parameters.BricksPerRegionLog2.X) + RegionBrickX] = (int8)MaxNonEmptyRegionBrickZ;
		}
	}
}

void UBrickGridComponent::GetMaxNonEmptyBrickZ(const FInt3& GetMinBrickCoordinates,const FInt3& GetMaxBrickCoordinates,TArray<int8>& OutHeightMap) const
{
	const FInt3 OutputSize = GetMaxBrickCoordinates - GetMinBrickCoordinates + FInt3::Scalar(1);
	const FInt3 MinRegionCoordinates = BrickToRegionCoordinates(GetMinBrickCoordinates);
	const FInt3 MaxRegionCoordinates = BrickToRegionCoordinates(GetMaxBrickCoordinates);
	const FInt3 TopMaxRegionCoordinates = FInt3(MaxRegionCoordinates.X,MaxRegionCoordinates.Y,Parameters.MaxRegionCoordinates.Z);
	for(int32 RegionY = MinRegionCoordinates.Y;RegionY <= TopMaxRegionCoordinates.Y;++RegionY)
	{
		for(int32 RegionX = MinRegionCoordinates.X;RegionX <= TopMaxRegionCoordinates.X;++RegionX)
		{
			TArray<const FBrickRegion*> ZRegions;
			ZRegions.Empty(TopMaxRegionCoordinates.Z - MinRegionCoordinates.Z + 1);
			for(int32 RegionZ = MinRegionCoordinates.Z;RegionZ <= TopMaxRegionCoordinates.Z;++RegionZ)
			{
				const FInt3 RegionCoordinates(RegionX,RegionY,RegionZ);
				const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
				if(RegionIndex)
				{
					ZRegions.Add(&Regions[*RegionIndex]);
				}
			}
			const FInt3 MinRegionBrickCoordinates = FInt3(RegionX,RegionY,MinRegionCoordinates.Z) * BricksPerRegion;
			const FInt3 MinOutputRegionBrickCoordinates = FInt3::Max(FInt3::Scalar(0),GetMinBrickCoordinates - MinRegionBrickCoordinates);
			const FInt3 MaxOutputRegionBrickCoordinates = FInt3::Min(BricksPerRegion - FInt3::Scalar(1),GetMaxBrickCoordinates - MinRegionBrickCoordinates);
			for(int32 RegionBrickY = MinOutputRegionBrickCoordinates.Y;RegionBrickY <= MaxOutputRegionBrickCoordinates.Y;++RegionBrickY)
			{
				for(int32 RegionBrickX = MinOutputRegionBrickCoordinates.X;RegionBrickX <= MaxOutputRegionBrickCoordinates.X;++RegionBrickX)
				{
					int32 MaxNonEmptyBrickZ = MinBrickCoordinates.Z - 1;
					for(int32 RegionZIndex = ZRegions.Num() - 1;RegionZIndex >= 0;--RegionZIndex)
					{
						const FBrickRegion& Region = *ZRegions[RegionZIndex];
						const int8 RegionMaxNonEmptyZ = Region.MaxNonEmptyBrickRegionZs[(RegionBrickY << Parameters.BricksPerRegionLog2.X) + RegionBrickX];
						if(RegionMaxNonEmptyZ != -1)
						{
							MaxNonEmptyBrickZ = Region.Coordinates.Z * BricksPerRegion.Z + (int32)RegionMaxNonEmptyZ;
							break;
						}
					}

					const int32 OutputX = MinRegionBrickCoordinates.X + RegionBrickX - GetMinBrickCoordinates.X;
					const int32 OutputY = MinRegionBrickCoordinates.Y + RegionBrickY - GetMinBrickCoordinates.Y;
					OutHeightMap[OutputY * OutputSize.X + OutputX] = (int8)FMath::Clamp(MaxNonEmptyBrickZ - GetMinBrickCoordinates.Z,-1,127);
				}
			}
		}
	}
}

void UBrickGridComponent::InvalidateChunkComponents(const FInt3& GetMinBrickCoordinates,const FInt3& GetMaxBrickCoordinates)
{
	// Expand the brick box by 1 brick so that bricks facing the one being invalidated are also updated.
	const FInt3 FacingExpansionExtent = FInt3::Scalar(1);

	// Update the region non-empty brick max Z maps.
	const FInt3 MinRegionCoordinates = BrickToRegionCoordinates(GetMinBrickCoordinates);
	const FInt3 MaxRegionCoordinates = BrickToRegionCoordinates(GetMaxBrickCoordinates);
	for(int32 RegionZ = Parameters.MinRegionCoordinates.Z;RegionZ <= MaxRegionCoordinates.Z;++RegionZ)
	{
		for(int32 RegionY = MinRegionCoordinates.Y;RegionY <= MaxRegionCoordinates.Y;++RegionY)
		{
			for(int32 RegionX = MinRegionCoordinates.X;RegionX <= MaxRegionCoordinates.X;++RegionX)
			{
				const FInt3 RegionCoordinates(RegionX,RegionY,RegionZ);
				const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
				if(RegionIndex)
				{
					const FInt3 MinRegionBrickCoordinates = RegionCoordinates * BricksPerRegion;
					const FInt3 MinDirtyRegionBrickCoordinates = FInt3::Max(FInt3::Scalar(0),GetMinBrickCoordinates - MinRegionBrickCoordinates);
					const FInt3 MaxDirtyRegionBrickCoordinates = FInt3::Min(BricksPerRegion - FInt3::Scalar(1),GetMaxBrickCoordinates - MinRegionBrickCoordinates);
					UpdateMaxNonEmptyBrickMap(Regions[*RegionIndex],MinDirtyRegionBrickCoordinates,MaxDirtyRegionBrickCoordinates);
				}
			}
		}
	}

	// Invalidate render components. Note that because of ambient occlusion, the render chunks need to be invalidated all the way to the bottom of the grid!
	const FInt3 AmbientOcclusionExpansionExtent = FInt3::Scalar(Parameters.AmbientOcclusionBlurRadius);
	const FInt3 RenderExpansionExtent = AmbientOcclusionExpansionExtent + FacingExpansionExtent;
	const FInt3 MinRenderChunkCoordinates = BrickToRenderChunkCoordinates(GetMinBrickCoordinates - RenderExpansionExtent);
	const FInt3 MaxRenderChunkCoordinates = BrickToRenderChunkCoordinates(GetMaxBrickCoordinates + RenderExpansionExtent);
	for(int32 ChunkX = MinRenderChunkCoordinates.X;ChunkX <= MaxRenderChunkCoordinates.X;++ChunkX)
	{
		for(int32 ChunkY = MinRenderChunkCoordinates.Y;ChunkY <= MaxRenderChunkCoordinates.Y;++ChunkY)
		{
			for(int32 ChunkZ = MinBrickCoordinates.Z;ChunkZ <= MaxRenderChunkCoordinates.Z;++ChunkZ)
			{
				UBrickRenderComponent* RenderComponent = RenderChunkCoordinatesToComponent.FindRef(FInt3(ChunkX,ChunkY,ChunkZ));
				if(RenderComponent)
				{
					if(ChunkZ >= MinRenderChunkCoordinates.Z)
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
	const FInt3 MinCollisionChunkCoordinates = BrickToCollisionChunkCoordinates(GetMinBrickCoordinates - FacingExpansionExtent);
	const FInt3 MaxCollisionChunkCoordinates = BrickToCollisionChunkCoordinates(GetMaxBrickCoordinates + FacingExpansionExtent);
	for(int32 ChunkX = MinCollisionChunkCoordinates.X;ChunkX <= MaxCollisionChunkCoordinates.X;++ChunkX)
	{
		for(int32 ChunkY = MinCollisionChunkCoordinates.Y;ChunkY <= MaxCollisionChunkCoordinates.Y;++ChunkY)
		{
			for(int32 ChunkZ = MinCollisionChunkCoordinates.Z;ChunkZ <= MaxCollisionChunkCoordinates.Z;++ChunkZ)
			{
				UBrickCollisionComponent* CollisionComponent = CollisionChunkCoordinatesToComponent.FindRef(FInt3(ChunkX, ChunkY, ChunkZ));
				if (CollisionComponent)
				{
					CollisionComponent->MarkRenderStateDirty();
				}
			}
		}
	}
}

void UBrickGridComponent::Update(const FVector& WorldViewPosition,float MaxDrawDistance,float MaxCollisionDistance,float MaxDesiredUpdateTime,FBrickGrid_InitRegion OnInitRegion)
{
	const FVector LocalViewPosition = GetComponentTransform().InverseTransformPosition(WorldViewPosition);
	const float LocalMaxDrawDistance = FMath::Max(0.0f,MaxDrawDistance / GetComponentTransform().GetScale3D().GetMin());
	const float LocalMaxCollisionDistance = FMath::Max(0.0f,MaxCollisionDistance / GetComponentTransform().GetScale3D().GetMin());
	const float LocalMaxDrawAndCollisionDistance = FMath::Max(LocalMaxDrawDistance,LocalMaxCollisionDistance);

	const double StartTime = FPlatformTime::Seconds();

	// Initialize any regions that are closer to the viewer than the draw or collision distance.
	// Include an additional ring of regions around what is being drawn or colliding so it has plenty of frames to spread initialization over before the data is needed.r
	const FInt3 MinInitRegionCoordinates = FInt3::Max(Parameters.MinRegionCoordinates,BrickToRegionCoordinates(FInt3::Floor(LocalViewPosition - FVector(LocalMaxDrawAndCollisionDistance))) - FInt3::Scalar(1));
	const FInt3 MaxInitRegionCoordinates = FInt3::Min(Parameters.MaxRegionCoordinates,BrickToRegionCoordinates(FInt3::Ceil(LocalViewPosition + FVector(LocalMaxDrawAndCollisionDistance))) + FInt3::Scalar(1));
	const float RegionExpansionRadius = BricksPerRegion.ToFloat().GetMin();
	for(int32 RegionZ = MinInitRegionCoordinates.Z;RegionZ <= MaxInitRegionCoordinates.Z && (FPlatformTime::Seconds() - StartTime) < MaxDesiredUpdateTime;++RegionZ)
	{
		for(int32 RegionY = MinInitRegionCoordinates.Y;RegionY <= MaxInitRegionCoordinates.Y && (FPlatformTime::Seconds() - StartTime) < MaxDesiredUpdateTime;++RegionY)
		{
			for(int32 RegionX = MinInitRegionCoordinates.X;RegionX <= MaxInitRegionCoordinates.X && (FPlatformTime::Seconds() - StartTime) < MaxDesiredUpdateTime;++RegionX)
			{
				const FInt3 RegionCoordinates(RegionX,RegionY,RegionZ);
				const FBox RegionBounds((RegionCoordinates * BricksPerRegion).ToFloat(),((RegionCoordinates + FInt3::Scalar(1)) * BricksPerRegion).ToFloat());
				if(RegionBounds.ComputeSquaredDistanceToPoint(LocalViewPosition) < FMath::Square(LocalMaxDrawAndCollisionDistance + RegionExpansionRadius))
				{
					if(!RegionCoordinatesToIndex.Find(RegionCoordinates))
					{
						const int32 RegionIndex = Regions.Num();
						FBrickRegion& Region = *new(Regions) FBrickRegion;
						Region.Coordinates = RegionCoordinates;

						// Initialize the region's bricks to the empty material.
						Region.BrickContents.Init(Parameters.EmptyMaterialIndex, 1 << Parameters.BricksPerRegionLog2.SumComponents());

						// Compute the region's non-empty height map.
						UpdateMaxNonEmptyBrickMap(Region,FInt3::Scalar(0),BricksPerRegion - FInt3::Scalar(1));

						// Add the region to the coordinate map.
						RegionCoordinatesToIndex.Add(RegionCoordinates,RegionIndex);

						// Call the InitRegion delegate for the new region.
						OnInitRegion.Execute(RegionCoordinates);
					}
				}
			}
		}
	}

	// Create render components for any chunks closer to the viewer than the draw distance, and destroy any that are no longer inside the draw distance.
	// Do this visibility check in 2D so the chunks underneath those on the horizon are also drawn even if they are too far.
	const FInt3 MinRenderChunkCoordinates = BrickToRenderChunkCoordinates(FInt3::Max(MinBrickCoordinates, FInt3::Floor(LocalViewPosition - FVector(LocalMaxDrawDistance))));
	const FInt3 MaxRenderChunkCoordinates = BrickToRenderChunkCoordinates(FInt3::Min(MaxBrickCoordinates, FInt3::Ceil(LocalViewPosition + FVector(LocalMaxDrawDistance))));
	for (auto ChunkIt = RenderChunkCoordinatesToComponent.CreateIterator(); ChunkIt; ++ChunkIt)
	{
		const FInt3 MinChunkBrickCoordinates = ChunkIt.Key() * BricksPerRenderChunk;
		const FInt3 MaxChunkBrickCoordinates = MinChunkBrickCoordinates + BricksPerRenderChunk - FInt3::Scalar(1);
		const FBox ChunkBounds(
			FInt3(MinChunkBrickCoordinates.X,MinChunkBrickCoordinates.Y,MinBrickCoordinates.Z).ToFloat(),
			FInt3(MinChunkBrickCoordinates.X,MinChunkBrickCoordinates.Y,MaxBrickCoordinates.Z).ToFloat()
			);
		if(ChunkBounds.ComputeSquaredDistanceToPoint(LocalViewPosition) > FMath::Square(LocalMaxDrawDistance))
		{
			ChunkIt.Value()->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepRelative,false));
			ChunkIt.Value()->DestroyComponent();
			ChunkIt.RemoveCurrent();
		}
	}
	for(int32 ChunkZ = BrickToRenderChunkCoordinates(MinBrickCoordinates).Z;ChunkZ <= BrickToRenderChunkCoordinates(MaxBrickCoordinates).Z && (FPlatformTime::Seconds() - StartTime) < MaxDesiredUpdateTime;++ChunkZ)
	{
		for(int32 ChunkY = MinRenderChunkCoordinates.Y;ChunkY <= MaxRenderChunkCoordinates.Y && (FPlatformTime::Seconds() - StartTime) < MaxDesiredUpdateTime;++ChunkY)
		{
			for(int32 ChunkX = MinRenderChunkCoordinates.X;ChunkX <= MaxRenderChunkCoordinates.X && (FPlatformTime::Seconds() - StartTime) < MaxDesiredUpdateTime;++ChunkX)
			{
				const FInt3 ChunkCoordinates(ChunkX,ChunkY,ChunkZ);
				const FInt3 MinChunkBrickCoordinates = ChunkCoordinates * BricksPerRenderChunk;
				const FInt3 MaxChunkBrickCoordinates = MinChunkBrickCoordinates + BricksPerRenderChunk - FInt3::Scalar(1);
				const FBox ChunkBounds(
					FInt3(MinChunkBrickCoordinates.X,MinChunkBrickCoordinates.Y,MinBrickCoordinates.Z).ToFloat(),
					FInt3(MinChunkBrickCoordinates.X,MinChunkBrickCoordinates.Y,MaxBrickCoordinates.Z).ToFloat()
					);
				if(ChunkBounds.ComputeSquaredDistanceToPoint(LocalViewPosition) < FMath::Square(LocalMaxDrawDistance))
				{
					UBrickRenderComponent* RenderComponent = RenderChunkCoordinatesToComponent.FindRef(ChunkCoordinates);
					if(!RenderComponent)
					{
						// Initialize a new chunk component.
						RenderComponent = NewObject<UBrickRenderComponent>(GetOwner());
						RenderComponent->Grid = this;
						RenderComponent->Coordinates = ChunkCoordinates;

						// Set the component transform and register it.
						RenderComponent->SetRelativeLocation((ChunkCoordinates * BricksPerRenderChunk).ToFloat());
						RenderComponent->AttachToComponent(this,FAttachmentTransformRules(EAttachmentRule::KeepRelative,false));
						RenderComponent->RegisterComponent();

						// Add the chunk to the coordinate map and visible chunk array.
						RenderChunkCoordinatesToComponent.Add(ChunkCoordinates,RenderComponent);
					}

					// Flush low-priority pending updates to render components.
					if(RenderComponent->HasLowPriorityUpdatePending)
					{
						RenderComponent->MarkRenderStateDirty();
						RenderComponent->HasLowPriorityUpdatePending = false;
					}
				}
			}
		}
	}

	// Create collision components for any chunks closer to the viewer than the collision distance, and destroy any that are no longer inside the draw distance.
	const FInt3 MinCollisionChunkCoordinates = BrickToCollisionChunkCoordinates(FInt3::Max(MinBrickCoordinates, FInt3::Floor(LocalViewPosition - FVector(LocalMaxCollisionDistance))));
	const FInt3 MaxCollisionChunkCoordinates = BrickToCollisionChunkCoordinates(FInt3::Min(MaxBrickCoordinates, FInt3::Ceil(LocalViewPosition + FVector(LocalMaxCollisionDistance))));
	const float LocalCollisionChunkRadius = BricksPerCollisionChunk.ToFloat().Size();
	for (auto ChunkIt = CollisionChunkCoordinatesToComponent.CreateIterator(); ChunkIt; ++ChunkIt)
	{
		const FBox ChunkBounds((ChunkIt.Key() * BricksPerCollisionChunk).ToFloat(),((ChunkIt.Key() + FInt3::Scalar(1)) * BricksPerCollisionChunk).ToFloat());
		if(	FInt3::Any(ChunkIt.Key() < MinCollisionChunkCoordinates)
		||	FInt3::Any(ChunkIt.Key() > MaxCollisionChunkCoordinates)
		||	ChunkBounds.ComputeSquaredDistanceToPoint(LocalViewPosition) > FMath::Square(LocalMaxCollisionDistance))
		{
			ChunkIt.Value()->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepRelative,false));
			ChunkIt.Value()->DestroyComponent();
			ChunkIt.RemoveCurrent();
		}
	}
	for(int32 ChunkZ = MinCollisionChunkCoordinates.Z;ChunkZ <= MaxCollisionChunkCoordinates.Z;++ChunkZ)
	{
		for(int32 ChunkY = MinCollisionChunkCoordinates.Y;ChunkY <= MaxCollisionChunkCoordinates.Y;++ChunkY)
		{
			for(int32 ChunkX = MinCollisionChunkCoordinates.X;ChunkX <= MaxCollisionChunkCoordinates.X;++ChunkX)
			{
				const FInt3 ChunkCoordinates(ChunkX,ChunkY,ChunkZ);
				const FBox ChunkBounds((ChunkCoordinates * BricksPerCollisionChunk).ToFloat(),((ChunkCoordinates + FInt3::Scalar(1)) * BricksPerCollisionChunk).ToFloat());
				if(ChunkBounds.ComputeSquaredDistanceToPoint(LocalViewPosition) < FMath::Square(LocalMaxCollisionDistance))
				{
					if(!CollisionChunkCoordinatesToComponent.FindRef(ChunkCoordinates))
					{
						// Initialize a new chunk component.
						UBrickCollisionComponent* Chunk = NewObject<UBrickCollisionComponent>(GetOwner());
						Chunk->Grid = this;
						Chunk->Coordinates = ChunkCoordinates;

						// Set the component transform and register it.
						Chunk->SetRelativeLocation((ChunkCoordinates * BricksPerCollisionChunk).ToFloat());
						Chunk->AttachToComponent(this,FAttachmentTransformRules(EAttachmentRule::KeepRelative,false));
						Chunk->RegisterComponent();

						// Add the chunk to the coordinate map and visible chunk array.
						CollisionChunkCoordinatesToComponent.Add(ChunkCoordinates,Chunk);
					}
				}
			}
		}
	}
}

bool UBrickGridComponent::SaveCompressed(FString path)
{
	FBufferArchive bufArchive;

	FBrickGridData toBeSaved;
	toBeSaved.Regions = Regions;
	bufArchive << toBeSaved;

	UE_LOG(LogStats, Log, TEXT("Precompressed File Size: %s"), *FString::FromInt(bufArchive.Num()));

	TArray<uint8> compressedData;
	FArchiveSaveCompressedProxy proxy = FArchiveSaveCompressedProxy(compressedData, COMPRESS_ZLIB);

	proxy << bufArchive;

	proxy.Flush();

	UE_LOG(LogStats, Log, TEXT("Compressed File Size: %s"), *FString::FromInt(compressedData.Num()));

	bool saveResult = FFileHelper::SaveArrayToFile(compressedData, *path);

	proxy.FlushCache();
	compressedData.Empty();

	bufArchive.FlushCache();
	bufArchive.Empty();

	if (saveResult)
		bufArchive.Close();

	return saveResult;
}

bool UBrickGridComponent::LoadCompressed(FString path)
{
	if (!FPaths::FileExists(path))
		return false;

	TArray<uint8> compressedData;
	if (!FFileHelper::LoadFileToArray(compressedData, *path))
	{
		UE_LOG(LogStats, Error, TEXT("File seems invalid!"));
		return false;
	}

	FArchiveLoadCompressedProxy proxy = FArchiveLoadCompressedProxy(compressedData, COMPRESS_ZLIB);

	if (proxy.GetError())
	{
		UE_LOG(LogStats, Error, TEXT("The file could not be decompressed! Is it compressed?"));
		return false;
	}

	FBufferArchive bufArchive;
	proxy << bufArchive;

	FMemoryReader memReader = FMemoryReader(bufArchive, true);
	memReader.Seek(0);

	FBrickGridData toBeLoaded;

	memReader << toBeLoaded;

	SetData(toBeLoaded);

	compressedData.Empty();
	proxy.FlushCache();
	memReader.FlushCache();

	bufArchive.Empty();
	bufArchive.Close();

	return true;
}

FBoxSphereBounds UBrickGridComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	// Return a bounds that fills the world.
	return FBoxSphereBounds(FVector(0,0,0),FVector(1,1,1) * HALF_WORLD_MAX,FMath::Sqrt(3.0f * HALF_WORLD_MAX));
}

FBrickGridParameters::FBrickGridParameters()
: EmptyMaterialIndex(0)
, BricksPerRegionLog2(5,5,7)
, RenderChunksPerRegionLog2(0,0,2)
, CollisionChunksPerRegionLog2(1,1,2)
, MinRegionCoordinates(-1024,-1024,0)
, MaxRegionCoordinates(+1024,+1024,0)
, AmbientOcclusionBlurRadius(2)
{
	Materials.Add(FBrickMaterial());
}

UBrickGridComponent::UBrickGridComponent(const FObjectInitializer& Initializer)
: Super(Initializer)
{
	PrimaryComponentTick.bStartWithTickEnabled =true;

	Init(Parameters);
}

void UBrickGridComponent::OnRegister()
{
	Super::OnRegister();

	for(auto ChunkIt = RenderChunkCoordinatesToComponent.CreateConstIterator();ChunkIt;++ChunkIt)
	{
		ChunkIt.Value()->RegisterComponent();
	}
	for (auto ChunkIt = CollisionChunkCoordinatesToComponent.CreateConstIterator(); ChunkIt; ++ChunkIt)
	{
		ChunkIt.Value()->RegisterComponent();
	}
}

void UBrickGridComponent::OnUnregister()
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
