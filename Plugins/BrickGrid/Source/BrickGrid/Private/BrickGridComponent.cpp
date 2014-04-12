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

	// Don't allow fractional chunks/region.
	Parameters.ChunksPerRegionLog2 = Max(Parameters.ChunksPerRegionLog2, FInt3::Scalar(0));

	// Limit each chunk to 128x128x128 bricks, which is the largest power of 2 size that can be rendered using 8-bit relative vertex positions.
	Parameters.BricksPerChunkLog2 = Clamp(Parameters.BricksPerChunkLog2, FInt3::Scalar(0), FInt3::Scalar(7));

	// Derive the chunk and region sizes from the log2 inputs.
	BricksPerRegionLog2 = Parameters.BricksPerChunkLog2 + Parameters.ChunksPerRegionLog2;
	BricksPerChunk = Exp2(Parameters.BricksPerChunkLog2);
	ChunksPerRegion = Exp2(Parameters.ChunksPerRegionLog2);
	BricksPerRegion = Exp2(BricksPerRegionLog2);

	// Clamp the min/max region coordinates to keep brick coordinates within 32-bit signed integers.
	Parameters.MinRegionCoordinates = Max(Parameters.MinRegionCoordinates, FInt3::Scalar(INT_MIN) / BricksPerRegion);
	Parameters.MaxRegionCoordinates = Min(Parameters.MaxRegionCoordinates, (FInt3::Scalar(INT_MAX) - BricksPerRegion + FInt3::Scalar(1)) / BricksPerRegion);
	MinBrickCoordinates = Parameters.MinRegionCoordinates * BricksPerRegion;
	MaxBrickCoordinates = Parameters.MaxRegionCoordinates * BricksPerRegion + BricksPerRegion - FInt3::Scalar(1);

	// Reset the regions and reregister the component.
	FComponentReregisterContext ReregisterContext(this);
	Regions.Empty();
	RegionCoordinatesToIndex.Empty();
	for(auto ChunkIt = ChunkCoordinatesToRenderComponent.CreateConstIterator();ChunkIt;++ChunkIt)
	{
		ChunkIt.Value()->DetachFromParent();
		ChunkIt.Value()->DestroyComponent();
	}
	for(auto ChunkIt = ChunkCoordinatesToCollisionComponent.CreateConstIterator();ChunkIt;++ChunkIt)
	{
		ChunkIt.Value()->DetachFromParent();
		ChunkIt.Value()->DestroyComponent();
	}
	ChunkCoordinatesToRenderComponent.Empty();
	ChunkCoordinatesToCollisionComponent.Empty();
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

int32 UBrickGridComponent::GetBrick(const FInt3& BrickCoordinates) const
{
	if(All(BrickCoordinates >= MinBrickCoordinates) && All(BrickCoordinates <= MaxBrickCoordinates))
	{
		const FInt3 RegionCoordinates = BrickToRegionCoordinates(BrickCoordinates);
		const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
		if(RegionIndex != NULL)
		{
			const FInt3 SubregionCoordinates = BrickCoordinates - (RegionCoordinates << BricksPerRegionLog2);
			const uint32 BrickIndex = (((SubregionCoordinates.Y << BricksPerRegionLog2.X) + SubregionCoordinates.X) << BricksPerRegionLog2.Z) + SubregionCoordinates.Z;

			const FBrickRegion& Region = Regions[*RegionIndex];
			return Region.BrickContents[BrickIndex];
		}
	}
	return Parameters.EmptyMaterialIndex;
}

bool UBrickGridComponent::SetBrick(const FInt3& BrickCoordinates, int32 MaterialIndex)
{
	if(SetBrickWithoutInvalidatingComponents(BrickCoordinates,MaterialIndex))
	{
		// Mark primitive components for chunks containing faces of this brick.
		const FInt3 MinChunkCoordinates = BrickToChunkCoordinates(BrickCoordinates - FInt3::Scalar(1));
		const FInt3 MaxChunkCoordinates = BrickToChunkCoordinates(BrickCoordinates + FInt3::Scalar(1));
		InvalidateChunkComponents(MinChunkCoordinates,MaxChunkCoordinates);

		return true;
	}
	return false;
}

bool UBrickGridComponent::SetBrickWithoutInvalidatingComponents(const FInt3& BrickCoordinates, int32 MaterialIndex)
{
	if (All(BrickCoordinates >= MinBrickCoordinates) && All(BrickCoordinates <= MaxBrickCoordinates) && MaterialIndex < Parameters.Materials.Num())
	{
		const FInt3 RegionCoordinates = BrickToRegionCoordinates(BrickCoordinates);
		const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
		if (RegionIndex != NULL)
		{
			const FInt3 SubregionCoordinates = BrickCoordinates - (RegionCoordinates << BricksPerRegionLog2);
			const uint32 BrickIndex = (((SubregionCoordinates.Y << BricksPerRegionLog2.X) + SubregionCoordinates.X) << BricksPerRegionLog2.Z) + SubregionCoordinates.Z;

			FBrickRegion& Region = Regions[*RegionIndex];
			Region.BrickContents[BrickIndex] = MaterialIndex;

			return true;
		}
	}
	return false;
}

void UBrickGridComponent::InvalidateChunkComponents(const FInt3& MinChunkCoordinates,const FInt3& MaxChunkCoordinates)
{
	for(int32 ChunkX = MinChunkCoordinates.X;ChunkX <= MaxChunkCoordinates.X;++ChunkX)
	{
		for(int32 ChunkY = MinChunkCoordinates.Y;ChunkY <= MaxChunkCoordinates.Y;++ChunkY)
		{
			for(int32 ChunkZ = MinChunkCoordinates.Z;ChunkZ <= MaxChunkCoordinates.Z;++ChunkZ)
			{
				UBrickRenderComponent* RenderComponent = ChunkCoordinatesToRenderComponent.FindRef(FInt3(ChunkX,ChunkY,ChunkZ));
				if(RenderComponent)
				{
					RenderComponent->MarkRenderStateDirty();
				}
				UBrickCollisionComponent* CollisionComponent = ChunkCoordinatesToCollisionComponent.FindRef(FInt3(ChunkX, ChunkY, ChunkZ));
				if (CollisionComponent)
				{
					CollisionComponent->MarkRenderStateDirty();
				}
			}
		}
	}
}

void UBrickGridComponent::UpdateVisibleChunks(const FVector& WorldViewPosition,float MaxDrawDistance,float MaxCollisionDistance,int32 MaxRegionsToCreate,FBrickGrid_InitRegion OnInitRegion)
{
	const FVector LocalViewPosition = GetComponenTransform().InverseTransformPosition(WorldViewPosition);
	const float LocalMaxDrawDistance = FMath::Max(0.0f,MaxDrawDistance / GetComponenTransform().GetScale3D().GetMin());
	const float LocalMaxCollisionDistance = FMath::Max(0.0f,MaxCollisionDistance / GetComponenTransform().GetScale3D().GetMin());
	const float LocalMaxDrawAndCollisionDistance = FMath::Max(LocalMaxDrawDistance,LocalMaxCollisionDistance);

	// Initialize any regions that are closer to the viewer than the draw or collision distance.
	// Include an additional ring of regions around what is being drawn or colliding so it has plenty of frames to spread initialization over before the data is needed.r
	const FInt3 MinInitRegionCoordinates = BrickToRegionCoordinates(Max(MinBrickCoordinates,Floor(LocalViewPosition - FVector(LocalMaxDrawAndCollisionDistance)) - FInt3::Scalar(1)));
	const FInt3 MaxInitRegionCoordinates = BrickToRegionCoordinates(Max(MinBrickCoordinates,Ceil(LocalViewPosition + FVector(LocalMaxDrawAndCollisionDistance)) + FInt3::Scalar(1)));
	int32 NumInitializedRegions = 0;
	for(int32 RegionZ = MinInitRegionCoordinates.Z;RegionZ <= MaxInitRegionCoordinates.Z && NumInitializedRegions < MaxRegionsToCreate;++RegionZ)
	{
		for(int32 RegionY = MinInitRegionCoordinates.Y;RegionY <= MaxInitRegionCoordinates.Y && NumInitializedRegions < MaxRegionsToCreate;++RegionY)
		{
			for(int32 RegionX = MinInitRegionCoordinates.X;RegionX <= MaxInitRegionCoordinates.X && NumInitializedRegions < MaxRegionsToCreate;++RegionX)
			{
				const FInt3 RegionCoordinates(RegionX,RegionY,RegionZ);
				const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
				if(!RegionIndex)
				{
					const int32 RegionIndex = Regions.Num();
					FBrickRegion& Region = *new(Regions) FBrickRegion;
					Region.Coordinates = RegionCoordinates;

					// Initialize the region's bricks to the empty material.
					Region.BrickContents.Init(Parameters.EmptyMaterialIndex, 1 << (BricksPerRegionLog2.X + BricksPerRegionLog2.Y + BricksPerRegionLog2.Z));

					// Add the region to the coordinate map.
					RegionCoordinatesToIndex.Add(RegionCoordinates,RegionIndex);

					// Call the InitRegion delegate for the new region.
					OnInitRegion.Execute(RegionCoordinates);

					++NumInitializedRegions;
				}
			}
		}
	}

	// Create render components for any chunks closer to the viewer than the draw distance, and destroy any that are no longer inside the draw distance.
	const FInt3 MinRenderChunkCoordinates = BrickToChunkCoordinates(Max(MinBrickCoordinates, Floor(LocalViewPosition - FVector(LocalMaxDrawDistance))));
	const FInt3 MaxRenderChunkCoordinates = BrickToChunkCoordinates(Min(MaxBrickCoordinates, Ceil(LocalViewPosition + FVector(LocalMaxDrawDistance))));
	for (auto ChunkIt = ChunkCoordinatesToRenderComponent.CreateIterator(); ChunkIt; ++ChunkIt)
	{
		if(Any(ChunkIt.Key() < MinRenderChunkCoordinates) || Any(ChunkIt.Key() > MaxRenderChunkCoordinates))
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
				UBrickRenderComponent* Chunk = ChunkCoordinatesToRenderComponent.FindRef(ChunkCoordinates);
				if(!Chunk)
				{
					// Initialize a new chunk component.
					UBrickRenderComponent* Chunk = ConstructObject<UBrickRenderComponent>(UBrickRenderComponent::StaticClass(), GetOwner());
					Chunk->Grid = this;
					Chunk->Coordinates = ChunkCoordinates;

					// Set the component transform and register it.
					Chunk->SetRelativeLocation(FVector(ChunkCoordinates.X * BricksPerChunk.X,ChunkCoordinates.Y * BricksPerChunk.Y,ChunkCoordinates.Z * BricksPerChunk.Z));
					Chunk->AttachTo(this);
					Chunk->RegisterComponent();

					// Add the chunk to the coordinate map and visible chunk array.
					ChunkCoordinatesToRenderComponent.Add(ChunkCoordinates,Chunk);
				}
			}
		}
	}

	// Create collision components for any chunks closer to the viewer than the collision distance, and destroy any that are no longer inside the draw distance.
	const FInt3 MinCollisionChunkCoordinates = BrickToChunkCoordinates(Max(MinBrickCoordinates, Floor(LocalViewPosition - FVector(LocalMaxCollisionDistance))));
	const FInt3 MaxCollisionChunkCoordinates = BrickToChunkCoordinates(Min(MaxBrickCoordinates, Ceil(LocalViewPosition + FVector(LocalMaxCollisionDistance))));
	for (auto ChunkIt = ChunkCoordinatesToCollisionComponent.CreateIterator(); ChunkIt; ++ChunkIt)
	{
		if(Any(ChunkIt.Key() < MinCollisionChunkCoordinates) || Any(ChunkIt.Key() > MaxCollisionChunkCoordinates))
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
				UBrickCollisionComponent* Chunk = ChunkCoordinatesToCollisionComponent.FindRef(ChunkCoordinates);
				if(!Chunk)
				{
					// Initialize a new chunk component.
					UBrickCollisionComponent* Chunk = ConstructObject<UBrickCollisionComponent>(UBrickCollisionComponent::StaticClass(), GetOwner());
					Chunk->Grid = this;
					Chunk->Coordinates = ChunkCoordinates;

					// Set the component transform and register it.
					Chunk->SetRelativeLocation(FVector(ChunkCoordinates.X * BricksPerChunk.X,ChunkCoordinates.Y * BricksPerChunk.Y,ChunkCoordinates.Z * BricksPerChunk.Z));
					Chunk->AttachTo(this);
					Chunk->RegisterComponent();

					// Add the chunk to the coordinate map and visible chunk array.
					ChunkCoordinatesToCollisionComponent.Add(ChunkCoordinates,Chunk);
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
, BricksPerChunkLog2(5,5,5)
, ChunksPerRegionLog2(0,0,0)
, MinRegionCoordinates(-1024,-1024,0)
, MaxRegionCoordinates(+1024,+1024,0)
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

	for(auto ChunkIt = ChunkCoordinatesToRenderComponent.CreateConstIterator();ChunkIt;++ChunkIt)
	{
		ChunkIt.Value()->RegisterComponent();
	}
	for (auto ChunkIt = ChunkCoordinatesToCollisionComponent.CreateConstIterator(); ChunkIt; ++ChunkIt)
	{
		ChunkIt.Value()->RegisterComponent();
	}
}

void UBrickGridComponent::OnUnregister()
{
	for (auto ChunkIt = ChunkCoordinatesToRenderComponent.CreateConstIterator(); ChunkIt; ++ChunkIt)
	{
		ChunkIt.Value()->UnregisterComponent();
	}
	for (auto ChunkIt = ChunkCoordinatesToCollisionComponent.CreateConstIterator(); ChunkIt; ++ChunkIt)
	{
		ChunkIt.Value()->UnregisterComponent();
	}

	Super::OnUnregister();
}
