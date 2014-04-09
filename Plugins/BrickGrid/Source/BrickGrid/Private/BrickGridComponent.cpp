// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickGridPluginPrivatePCH.h"
#include "BrickChunkComponent.h"
#include "BrickGridComponent.h"

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
	return EmptyMaterialIndex;
}

bool UBrickGridComponent::SetBrick(const FInt3& BrickCoordinates, int32 MaterialIndex)
{
	if(All(BrickCoordinates >= MinBrickCoordinates) && All(BrickCoordinates <= MaxBrickCoordinates) && MaterialIndex < Materials.Num())
	{
		const FInt3 RegionCoordinates = BrickToRegionCoordinates(BrickCoordinates);
		const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
		if (RegionIndex != NULL)
		{
			const FInt3 SubregionCoordinates = BrickCoordinates - (RegionCoordinates << BricksPerRegionLog2);
			const uint32 BrickIndex = (((SubregionCoordinates.Y << BricksPerRegionLog2.X) + SubregionCoordinates.X) << BricksPerRegionLog2.Z) + SubregionCoordinates.Z;

			FBrickRegion& Region = Regions[*RegionIndex];
			Region.BrickContents[BrickIndex] = MaterialIndex;

			UBrickChunkComponent* Component = ChunkCoordinatesToComponent.FindRef(BrickToChunkCoordinates(BrickCoordinates));
			if(Component)
			{
				Component->MarkRenderStateDirty();
			}

			return true;
		}
	}
	return false;
}

void UBrickGridComponent::UpdateVisibleChunks(const FVector& WorldViewPosition,int32 MaxRegionsToCreate)
{
	const FVector LocalViewPosition = GetComponenTransform().InverseTransformPosition(WorldViewPosition);
	const float LocalMaxDrawDistance = MaxDrawDistance / GetComponenTransform().GetScale3D().GetMin();
	const FVector ChunkCenterOffset = FVector(BricksPerChunk.X,BricksPerChunk.Y,BricksPerChunk.Z) / 2.0f;
	const float LocalChunkRadius = ChunkCenterOffset.Size();

	const FInt3 MinVisibleChunkCoordinates = BrickToChunkCoordinates(Max(MinBrickCoordinates, Floor(LocalViewPosition - FVector(LocalMaxDrawDistance))));
	const FInt3 MaxVisibleChunkCoordinates = BrickToChunkCoordinates(Min(MaxBrickCoordinates, Ceil(LocalViewPosition + FVector(LocalMaxDrawDistance))));

	for(int32 ChunkZ = MinVisibleChunkCoordinates.Z;ChunkZ <= MaxVisibleChunkCoordinates.Z;++ChunkZ)
	{
		for(int32 ChunkY = MinVisibleChunkCoordinates.Y;ChunkY <= MaxVisibleChunkCoordinates.Y;++ChunkY)
		{
			for(int32 ChunkX = MinVisibleChunkCoordinates.X;ChunkX <= MaxVisibleChunkCoordinates.X;++ChunkX)
			{
				const FInt3 ChunkCoordinates(ChunkX,ChunkY,ChunkZ);
				UBrickChunkComponent* Chunk = ChunkCoordinatesToComponent.FindRef(ChunkCoordinates);
				if(!Chunk)
				{
					// Ensure that a region is initialized before any chunks within it.
					const FInt3 RegionCoordinates = SignedShiftRight(ChunkCoordinates,ChunksPerRegionLog2);
					const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
					if(!RegionIndex)
					{
						CreateRegion(RegionCoordinates);
					}
					CreateChunk(ChunkCoordinates);
				}
			}
		}
	}
}

void UBrickGridComponent::CreateChunk(const FInt3& Coordinates)
{
	// Initialize a new chunk component.
	UBrickChunkComponent* Chunk = ConstructObject<UBrickChunkComponent>(ChunkClass,GetOwner());
	Chunk->Grid = this;
	Chunk->Coordinates = Coordinates;

	// Set the component transform and register it.
	Chunk->SetRelativeLocation(FVector(Coordinates.X * BricksPerChunk.X,Coordinates.Y * BricksPerChunk.Y,Coordinates.Z * BricksPerChunk.Z));
	Chunk->AttachTo(this);
	Chunk->RegisterComponent();

	// Add the chunk to the coordinate map and visible chunk array.
	ChunkCoordinatesToComponent.Add(Coordinates,Chunk);
	VisibleChunks.Add(Chunk);
}

void UBrickGridComponent::CreateRegion(const FInt3& Coordinates)
{
	const int32 RegionIndex = Regions.Num();
	FBrickRegion& Region = *new(Regions) FBrickRegion;
	Region.Coordinates = Coordinates;

	// Initialize the region's bricks to the empty material.
	Region.BrickContents.Init(EmptyMaterialIndex,1 << (BricksPerRegionLog2.X + BricksPerRegionLog2.Y + BricksPerRegionLog2.Z));

	// Add the region to the coordinate map.
	RegionCoordinatesToIndex.Add(Coordinates,RegionIndex);

	// Call the InitRegion delegate for the new region.
	OnInitRegion.Broadcast(this,Coordinates);
}

FBoxSphereBounds UBrickGridComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	// Return a bounds that fills the world.
	return FBoxSphereBounds(FVector(0,0,0),FVector(1,1,1) * HALF_WORLD_MAX,FMath::Sqrt(3.0f * HALF_WORLD_MAX));
}

void UBrickGridComponent::ComputeDerivedConstants()
{
	// Derive the chunk and region sizes from the log2 inputs.
	BricksPerRegionLog2 = BricksPerChunkLog2 + ChunksPerRegionLog2;
	BricksPerChunk = Exp2(BricksPerChunkLog2);
	ChunksPerRegion = Exp2(ChunksPerRegionLog2);
	BricksPerRegion = Exp2(BricksPerRegionLog2);
	MinBrickCoordinates = MinRegionCoordinates * BricksPerRegion;
	MaxBrickCoordinates = MaxRegionCoordinates * BricksPerRegion + BricksPerRegion - FInt3::Scalar(1);
}

UBrickGridComponent::UBrickGridComponent(const FPostConstructInitializeProperties& PCIP)
: Super( PCIP )
{
	PrimaryComponentTick.bStartWithTickEnabled =true;

	BricksPerChunkLog2 = FInt3::Scalar(4);
	ChunksPerRegionLog2 = FInt3::Scalar(0);
	MinRegionCoordinates = FInt3(-1024,-1024,0);
	MaxRegionCoordinates = FInt3(1024,1024,0);

	Materials.Add(FBrickMaterial());
	EmptyMaterialIndex = 0;

	MaxDrawDistance = 5000.0f;

	ChunkClass = UBrickChunkComponent::StaticClass();

	// Update the derived constants.
	ComputeDerivedConstants();
}

#if WITH_EDITOR
	void UBrickGridComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
	{
		if(PropertyChangedEvent.Property != NULL)
		{
			const FName PropertyName = PropertyChangedEvent.Property->GetFName();
			if(	PropertyName == FName(TEXT("BricksPerChunkLog2"))
			||	PropertyName == FName(TEXT("MaxRegionCoordinates"))
			||	PropertyName == FName(TEXT("ChunksPerRegionLog2"))
			||	PropertyName == FName(TEXT("MinRegionCoordinates"))
			||	PropertyName == FName(TEXT("EmptyMaterialIndex"))
			||	PropertyName == FName(TEXT("ChunkDrawRadius")))
			{
				// Don't allow fractional chunks/region.
				ChunksPerRegionLog2 = Max(ChunksPerRegionLog2, FInt3::Scalar(0));

				// Limit each chunk to 128x128x128 bricks, which is the largest power of 2 size that can be rendered using 8-bit relative vertex positions.
				BricksPerChunkLog2 = Clamp(BricksPerChunkLog2, FInt3::Scalar(0), FInt3::Scalar(7));

				// Clamp the min/max region coordinates to keep brick coordinates within 32-bit signed integers.
				MinRegionCoordinates = Max(MinRegionCoordinates, FInt3::Scalar(INT_MIN) / BricksPerRegion);
				MaxRegionCoordinates = Max(MaxRegionCoordinates, (FInt3::Scalar(INT_MAX) - BricksPerRegion + FInt3::Scalar(1)) / BricksPerRegion);

				// Validate the empty material index.
				EmptyMaterialIndex = FMath::Clamp<int32>(EmptyMaterialIndex, 0, Materials.Num() - 1);

				// Don't allow negative draw radius.
				MaxDrawDistance = FMath::Max(0.0f, MaxDrawDistance);

				// Update the derived constants.
				ComputeDerivedConstants();
			}
		}

		Super::PostEditChangeProperty(PropertyChangedEvent);
	}
#endif

void UBrickGridComponent::PostLoad()
{
	Super::PostLoad();

	// Update the derived constants.
	ComputeDerivedConstants();

	// Recreate the region coordinate to index map, which isn't serialized.
	for(auto RegionIt = Regions.CreateConstIterator();RegionIt;++RegionIt)
	{
		RegionCoordinatesToIndex.Add(RegionIt->Coordinates,RegionIt.GetIndex());
	}
}
