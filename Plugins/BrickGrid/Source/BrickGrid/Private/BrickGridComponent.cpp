// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickGridPluginPrivatePCH.h"
#include "BrickChunkComponent.h"
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

	// Reset the regions and reregister the component (which destroys the visible chunks).
	Regions.Empty();
	RegionCoordinatesToIndex.Empty();
	ReregisterComponent();
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
				UBrickChunkComponent* Component = ChunkCoordinatesToComponent.FindRef(FInt3(ChunkX,ChunkY,ChunkZ));
				if(Component)
				{
					Component->MarkRenderStateDirty();
				}
			}
		}
	}
}

void UBrickGridComponent::UpdateVisibleChunks(const FVector& WorldViewPosition,float MaxDrawDistance,int32 MaxRegionsToCreate,FBrickGrid_InitRegion OnInitRegion)
{
	const FVector LocalViewPosition = GetComponenTransform().InverseTransformPosition(WorldViewPosition);
	const float LocalMaxDrawDistance = FMath::Max(0.0f,MaxDrawDistance / GetComponenTransform().GetScale3D().GetMin());
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
					const FInt3 RegionCoordinates = SignedShiftRight(ChunkCoordinates,Parameters.ChunksPerRegionLog2);
					const int32* const RegionIndex = RegionCoordinatesToIndex.Find(RegionCoordinates);
					if(!RegionIndex)
					{
						CreateRegion(RegionCoordinates,OnInitRegion);
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
	UBrickChunkComponent* Chunk = ConstructObject<UBrickChunkComponent>(Parameters.ChunkClass, GetOwner());
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

void UBrickGridComponent::CreateRegion(const FInt3& Coordinates,FBrickGrid_InitRegion InitRegion)
{
	const int32 RegionIndex = Regions.Num();
	FBrickRegion& Region = *new(Regions) FBrickRegion;
	Region.Coordinates = Coordinates;

	// Initialize the region's bricks to the empty material.
	Region.BrickContents.Init(Parameters.EmptyMaterialIndex, 1 << (BricksPerRegionLog2.X + BricksPerRegionLog2.Y + BricksPerRegionLog2.Z));

	// Add the region to the coordinate map.
	RegionCoordinatesToIndex.Add(Coordinates,RegionIndex);

	// Call the InitRegion delegate for the new region.
	InitRegion.Execute(Coordinates);
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
, ChunkClass(UBrickChunkComponent::StaticClass())
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

void UBrickGridComponent::OnUnregister()
{
	for(auto ChunkIt = VisibleChunks.CreateConstIterator();ChunkIt;++ChunkIt)
	{
		UBrickChunkComponent* Chunk = *ChunkIt;
		Chunk->DetachFromParent();
		Chunk->DestroyComponent();
	}
	VisibleChunks.Empty();
	ChunkCoordinatesToComponent.Empty();

	Super::OnUnregister();
}
