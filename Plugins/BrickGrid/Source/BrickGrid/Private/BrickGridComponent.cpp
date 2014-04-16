// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickGridPluginPrivatePCH.h"
#include "BrickRenderComponent.h"
#include "BrickCollisionComponent.h"
#include "BrickGridComponent.h"

void UBrickGridComponent::Init(const FBrickGridParameters& InParameters)
{
	Parameters = InParameters;

	// Validate the empty material index.
	Parameters.EmptyMaterialIndex = FMath::Clamp<int32>(Parameters.EmptyMaterialIndex, 0, Parameters.Materials.Num() - 1);

	// Limit each region to 128x128x128 bricks, which is the largest power of 2 size that can be rendered using 8-bit relative vertex positions.
	Parameters.BricksPerRegionLog2 = FInt3::Clamp(Parameters.BricksPerRegionLog2, FInt3::Scalar(0), FInt3::Scalar(7));

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
		ChunkIt.Value()->DetachFromParent();
		ChunkIt.Value()->DestroyComponent();
	}
	for(auto ChunkIt = CollisionChunkCoordinatesToComponent.CreateConstIterator();ChunkIt;++ChunkIt)
	{
		ChunkIt.Value()->DetachFromParent();
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
	// Recreate the region coordinate to index map.
	for(auto RegionIt = Regions.CreateConstIterator();RegionIt;++RegionIt)
	{
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
			return FBrick(Region.BrickContents[BrickIndex],Region.HasAmbientOcclusionFactors ? Region.BrickAmbientOcclusion[BrickIndex] : 255);
		}
	}
	return FBrick(Parameters.EmptyMaterialIndex,255);
}

bool UBrickGridComponent::SetBrick(const FInt3& BrickCoordinates, int32 MaterialIndex)
{
	if(SetBrickWithoutInvalidatingComponents(BrickCoordinates,MaterialIndex))
	{
		InvalidateChunkComponents(BrickCoordinates,BrickCoordinates);
		return true;
	}
	return false;
}

bool UBrickGridComponent::SetBrickWithoutInvalidatingComponents(const FInt3& BrickCoordinates, int32 MaterialIndex)
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
			return true;
		}
	}
	return false;
}

void UBrickGridComponent::InvalidateChunkComponents(const FInt3& MinBrickCoordinates,const FInt3& MaxBrickCoordinates)
{
	// Expand the brick box by 1 brick so that bricks facing the one being invalidated are also updated.
	const FInt3 FacingExpansionExtent = FInt3::Scalar(1);

	// Invalidate ambient occlusion.
	const FInt3 AmbientOcclusionExpansionExtent = FInt3::Scalar(Parameters.AmbientOcclusionBlurRadius);
	const FInt3 MinRegionCoordinates = BrickToRegionCoordinates(MinBrickCoordinates - AmbientOcclusionExpansionExtent);
	const FInt3 MaxRegionCoordinates = BrickToRegionCoordinates(MaxBrickCoordinates + AmbientOcclusionExpansionExtent);
	for(int32 RegionX = MinRegionCoordinates.X;RegionX <= MaxRegionCoordinates.X;++RegionX)
	{
		for(int32 RegionY = MinRegionCoordinates.Y;RegionY <= MaxRegionCoordinates.Y;++RegionY)
		{
			for(int32 RegionZ = MinRegionCoordinates.Z;RegionZ <= MaxRegionCoordinates.Z;++RegionZ)
			{
				int32* RegionIndex = RegionCoordinatesToIndex.Find(FInt3(RegionX,RegionY,RegionZ));
				if(RegionIndex)
				{
					Regions[*RegionIndex].HasAmbientOcclusionFactors = false;
				}
			}
		}
	}

	// Invalidate render components. Note that because of ambient occlusion, the render chunks need to be invalidated all the way to the bottom of the grid!
	const FInt3 RenderExpansionExtent = AmbientOcclusionExpansionExtent + FacingExpansionExtent;
	const FInt3 MinRenderChunkCoordinates = BrickToRenderChunkCoordinates(this->MinBrickCoordinates);
	const FInt3 MaxRenderChunkCoordinates = BrickToRenderChunkCoordinates(MaxBrickCoordinates + RenderExpansionExtent);
	for(int32 ChunkX = MinRenderChunkCoordinates.X;ChunkX <= MaxRenderChunkCoordinates.X;++ChunkX)
	{
		for(int32 ChunkY = MinRenderChunkCoordinates.Y;ChunkY <= MaxRenderChunkCoordinates.Y;++ChunkY)
		{
			for(int32 ChunkZ = MinRenderChunkCoordinates.Z;ChunkZ <= MaxRenderChunkCoordinates.Z;++ChunkZ)
			{
				UBrickRenderComponent* RenderComponent = RenderChunkCoordinatesToComponent.FindRef(FInt3(ChunkX,ChunkY,ChunkZ));
				if(RenderComponent)
				{
					RenderComponent->MarkRenderStateDirty();
				}
			}
		}
	}

	// Invalidate collision components.
	const FInt3 MinCollisionChunkCoordinates = BrickToCollisionChunkCoordinates(MinBrickCoordinates - FacingExpansionExtent);
	const FInt3 MaxCollisionChunkCoordinates = BrickToCollisionChunkCoordinates(MaxBrickCoordinates + FacingExpansionExtent);
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

void UBrickGridComponent::Update(const FVector& WorldViewPosition,float MaxDrawDistance,float MaxCollisionDistance,int32 MaxRegionsToCreate,FBrickGrid_InitRegion OnInitRegion)
{
	const FVector LocalViewPosition = GetComponenTransform().InverseTransformPosition(WorldViewPosition);
	const float LocalMaxDrawDistance = FMath::Max(0.0f,MaxDrawDistance / GetComponenTransform().GetScale3D().GetMin());
	const float LocalMaxCollisionDistance = FMath::Max(0.0f,MaxCollisionDistance / GetComponenTransform().GetScale3D().GetMin());
	const float LocalMaxDrawAndCollisionDistance = FMath::Max(LocalMaxDrawDistance,LocalMaxCollisionDistance);

	// Initialize any regions that are closer to the viewer than the draw or collision distance.
	// Include an additional ring of regions around what is being drawn or colliding so it has plenty of frames to spread initialization over before the data is needed.r
	const FInt3 MinInitRegionCoordinates = FInt3::Max(Parameters.MinRegionCoordinates,BrickToRegionCoordinates(FInt3::Floor(LocalViewPosition - FVector(LocalMaxDrawAndCollisionDistance))) - FInt3::Scalar(1));
	const FInt3 MaxInitRegionCoordinates = FInt3::Min(Parameters.MaxRegionCoordinates,BrickToRegionCoordinates(FInt3::Ceil(LocalViewPosition + FVector(LocalMaxDrawAndCollisionDistance))) + FInt3::Scalar(1));
	const float RegionExpansionRadius = BricksPerRegion.ToFloat().GetMin();
	int32 NumInitializedRegions = 0;
	for(int32 RegionZ = MinInitRegionCoordinates.Z;RegionZ <= MaxInitRegionCoordinates.Z && NumInitializedRegions < MaxRegionsToCreate;++RegionZ)
	{
		for(int32 RegionY = MinInitRegionCoordinates.Y;RegionY <= MaxInitRegionCoordinates.Y && NumInitializedRegions < MaxRegionsToCreate;++RegionY)
		{
			for(int32 RegionX = MinInitRegionCoordinates.X;RegionX <= MaxInitRegionCoordinates.X && NumInitializedRegions < MaxRegionsToCreate;++RegionX)
			{
				const FInt3 RegionCoordinates(RegionX,RegionY,RegionZ);
				const FBox RegionBounds((RegionCoordinates * BricksPerRegion).ToFloat(),((RegionCoordinates + FInt3::Scalar(1)) * BricksPerRegion).ToFloat());
				if(RegionBounds.ComputeSquaredDistanceToPoint(LocalViewPosition) < FMath::Square(LocalMaxDrawAndCollisionDistance + RegionExpansionRadius))
				{
					const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
					if(!RegionIndex)
					{
						const int32 RegionIndex = Regions.Num();
						FBrickRegion& Region = *new(Regions) FBrickRegion;
						Region.Coordinates = RegionCoordinates;

						// Initialize the region's bricks to the empty material.
						Region.BrickContents.Init(Parameters.EmptyMaterialIndex, 1 << (Parameters.BricksPerRegionLog2.X + Parameters.BricksPerRegionLog2.Y + Parameters.BricksPerRegionLog2.Z));

						// Add the region to the coordinate map.
						RegionCoordinatesToIndex.Add(RegionCoordinates,RegionIndex);

						// Call the InitRegion delegate for the new region.
						OnInitRegion.Execute(RegionCoordinates);

						++NumInitializedRegions;
					}
				}
			}
		}
	}

	// Create render components for any chunks closer to the viewer than the draw distance, and destroy any that are no longer inside the draw distance.
	const FInt3 MinRenderChunkCoordinates = BrickToRenderChunkCoordinates(FInt3::Max(MinBrickCoordinates, FInt3::Floor(LocalViewPosition - FVector(LocalMaxDrawDistance))));
	const FInt3 MaxRenderChunkCoordinates = BrickToRenderChunkCoordinates(FInt3::Min(MaxBrickCoordinates, FInt3::Ceil(LocalViewPosition + FVector(LocalMaxDrawDistance))));
	for (auto ChunkIt = RenderChunkCoordinatesToComponent.CreateIterator(); ChunkIt; ++ChunkIt)
	{
		const FBox ChunkBounds((ChunkIt.Key() * BricksPerRenderChunk).ToFloat(),((ChunkIt.Key() + FInt3::Scalar(1)) * BricksPerRenderChunk).ToFloat());
		if(	FInt3::Any(ChunkIt.Key() < MinRenderChunkCoordinates)
		||	FInt3::Any(ChunkIt.Key() > MaxRenderChunkCoordinates)
		||	ChunkBounds.ComputeSquaredDistanceToPoint(LocalViewPosition) > FMath::Square(LocalMaxDrawDistance))
		{
			ChunkIt.Value()->DetachFromParent();
			ChunkIt.Value()->DestroyComponent();
			ChunkIt.RemoveCurrent();
		}
	}
	for(int32 ChunkZ = MinRenderChunkCoordinates.Z;ChunkZ <= MaxRenderChunkCoordinates.Z;++ChunkZ)
	{
		for(int32 ChunkY = MinRenderChunkCoordinates.Y;ChunkY <= MaxRenderChunkCoordinates.Y;++ChunkY)
		{
			for(int32 ChunkX = MinRenderChunkCoordinates.X;ChunkX <= MaxRenderChunkCoordinates.X;++ChunkX)
			{
				const FInt3 ChunkCoordinates(ChunkX,ChunkY,ChunkZ);
				const FBox ChunkBounds((ChunkCoordinates * BricksPerRenderChunk).ToFloat(),((ChunkCoordinates + FInt3::Scalar(1)) * BricksPerRenderChunk).ToFloat());
				if(ChunkBounds.ComputeSquaredDistanceToPoint(LocalViewPosition) < FMath::Square(LocalMaxDrawDistance))
				{
					if(!RenderChunkCoordinatesToComponent.FindRef(ChunkCoordinates))
					{
						// Initialize a new chunk component.
						UBrickRenderComponent* Chunk = ConstructObject<UBrickRenderComponent>(UBrickRenderComponent::StaticClass(), GetOwner());
						Chunk->Grid = this;
						Chunk->Coordinates = ChunkCoordinates;

						// Set the component transform and register it.
						Chunk->SetRelativeLocation((ChunkCoordinates * BricksPerRenderChunk).ToFloat());
						Chunk->AttachTo(this);
						Chunk->RegisterComponent();

						// Add the chunk to the coordinate map and visible chunk array.
						RenderChunkCoordinatesToComponent.Add(ChunkCoordinates,Chunk);
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
			ChunkIt.Value()->DetachFromParent();
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
						UBrickCollisionComponent* Chunk = ConstructObject<UBrickCollisionComponent>(UBrickCollisionComponent::StaticClass(), GetOwner());
						Chunk->Grid = this;
						Chunk->Coordinates = ChunkCoordinates;

						// Set the component transform and register it.
						Chunk->SetRelativeLocation((ChunkCoordinates * BricksPerCollisionChunk).ToFloat());
						Chunk->AttachTo(this);
						Chunk->RegisterComponent();

						// Add the chunk to the coordinate map and visible chunk array.
						CollisionChunkCoordinatesToComponent.Add(ChunkCoordinates,Chunk);
					}
				}
			}
		}
	}
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

UBrickGridComponent::UBrickGridComponent(const FPostConstructInitializeProperties& PCIP)
: Super( PCIP )
{
	PrimaryComponentTick.bStartWithTickEnabled =true;

	Init(Parameters);
}

void UBrickGridComponent::PostLoad()
{
	Super::PostLoad();

	// Recreate the region coordinate to index map, which isn't serialized.
	for(auto RegionIt = Regions.CreateConstIterator();RegionIt;++RegionIt)
	{
		RegionCoordinatesToIndex.Add(RegionIt->Coordinates,RegionIt.GetIndex());
	}
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
