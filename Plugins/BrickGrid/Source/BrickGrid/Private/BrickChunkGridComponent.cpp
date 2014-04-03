// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickGridPluginPrivatePCH.h"
#include "BrickChunkComponent.h"
#include "BrickChunkGridComponent.h"

UBrickChunkGridComponent::UBrickChunkGridComponent(const FPostConstructInitializeProperties& PCIP)
: Super( PCIP )
{
	PrimaryComponentTick.bCanEverTick = false;

	ChunkParameters.SizeX = 16;
	ChunkParameters.SizeY = 16;
	ChunkParameters.SizeZ = 32;
	ChunkParameters.Materials.Add(FBrickMaterial());
	ChunkParameters.EmptyMaterialIndex = 0;
	ChunkDrawRadius = 10000.0f;
	MaxChunksAllocatedPerFrame = 10;
	ChunkClass = UBrickChunkComponent::StaticClass();

	MinChunkCoordinates = FChunkCoordinates(INT_MIN,INT_MIN,0);
	MaxChunkCoordinates = FChunkCoordinates(INT_MAX,INT_MAX,0);
}

void UBrickChunkGridComponent::UpdateCameraPosition(const FVector& WorldCameraPosition)
{
	const FVector LocalCameraPosition = GetComponenTransform().InverseTransformPosition(WorldCameraPosition);
	const float LocalChunkDrawRadius = ChunkDrawRadius / GetComponenTransform().GetScale3D().GetMin();
	const FVector ChunkCenterOffset = FVector(ChunkParameters.SizeX,ChunkParameters.SizeY,ChunkParameters.SizeZ) / 2.0f;
	const float LocalChunkRadius = ChunkCenterOffset.Size();

	const int32 MinChunkX = FMath::Max(MinChunkCoordinates.X,FMath::Floor((LocalCameraPosition.X - LocalChunkDrawRadius) / ChunkParameters.SizeX));
	const int32 MinChunkY = FMath::Max(MinChunkCoordinates.Y,FMath::Floor((LocalCameraPosition.Y - LocalChunkDrawRadius) / ChunkParameters.SizeY));
	const int32 MinChunkZ = FMath::Max(MinChunkCoordinates.Z,FMath::Floor((LocalCameraPosition.Z - LocalChunkDrawRadius) / ChunkParameters.SizeZ));
	const int32 MaxChunkX = FMath::Min(MaxChunkCoordinates.X,FMath::Ceil((LocalCameraPosition.X + LocalChunkDrawRadius) / ChunkParameters.SizeX));
	const int32 MaxChunkY = FMath::Min(MaxChunkCoordinates.Y,FMath::Ceil((LocalCameraPosition.Y + LocalChunkDrawRadius) / ChunkParameters.SizeY));
	const int32 MaxChunkZ = FMath::Min(MaxChunkCoordinates.Z,FMath::Ceil((LocalCameraPosition.Z + LocalChunkDrawRadius) / ChunkParameters.SizeZ));

	const int32 MaxChunksAllocatedThisFrame = ChunkMap.Num() > 0 ? MaxChunksAllocatedPerFrame : INT_MAX;
	int32 ChunksAllocatedThisFrame = 0;
	for(int32 ChunkZ = MinChunkZ;ChunkZ <= MaxChunkZ && ChunksAllocatedThisFrame < MaxChunksAllocatedThisFrame;++ChunkZ)
	{
		for(int32 ChunkY = MinChunkY;ChunkY <= MaxChunkY && ChunksAllocatedThisFrame < MaxChunksAllocatedThisFrame;++ChunkY)
		{
			for(int32 ChunkX = MinChunkX;ChunkX <= MaxChunkX && ChunksAllocatedThisFrame < MaxChunksAllocatedThisFrame;++ChunkX)
			{
				const FChunkCoordinates Coordinates(ChunkX,ChunkY,ChunkZ);
				const UBrickChunkComponent* Chunk = ChunkMap.FindRef(Coordinates);
				if(Chunk == NULL)
				{
					AllocateChunk(Coordinates);
					++ChunksAllocatedThisFrame;
				}
			}
		}
	}
}

UBrickChunkComponent* UBrickChunkGridComponent::GetChunk(const FChunkCoordinates& Coordinates) const
{
	return ChunkMap.FindRef(Coordinates);
}

void UBrickChunkGridComponent::AllocateChunk(const FChunkCoordinates& Coordinates)
{
	UBrickChunkComponent* Chunk = ConstructObject<UBrickChunkComponent>(ChunkClass,GetOwner());
	Chunk->SetRelativeLocation(FVector(Coordinates.X * ChunkParameters.SizeX,Coordinates.Y * ChunkParameters.SizeY,Coordinates.Z * ChunkParameters.SizeZ));
	Chunk->AttachTo(this);
	Chunk->RegisterComponent();
	ChunkMap.Add(Coordinates,Chunk);
	Chunk->Init(ChunkParameters);
	OnInitChunk.Broadcast(Coordinates,Chunk);
}

FBoxSphereBounds UBrickChunkGridComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	// Return a bounds that fills the world.
	return FBoxSphereBounds(FVector(0,0,0),FVector(1,1,1) * HALF_WORLD_MAX,FMath::Sqrt(3.0f * HALF_WORLD_MAX));
}

#if WITH_EDITOR
	void UBrickChunkGridComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
	{
		if(PropertyChangedEvent.Property != NULL)
		{
			const FName PropertyName = PropertyChangedEvent.Property->GetFName();
			if(PropertyName == FName(TEXT("ChunkParameters")))
			{
				// Limit each chunk to 255x255x255 bricks, which is the limit of what can be rendered using 8-bit relative vertex positions.
				ChunkParameters.SizeX = (uint32)FMath::Clamp(ChunkParameters.SizeX, 0, 255);
				ChunkParameters.SizeY = (uint32)FMath::Clamp(ChunkParameters.SizeY, 0, 255);
				ChunkParameters.SizeZ = (uint32)FMath::Clamp(ChunkParameters.SizeZ, 0, 255);

				// Validate the empty material index.
				ChunkParameters.EmptyMaterialIndex = FMath::Clamp<int32>(ChunkParameters.EmptyMaterialIndex, 0, ChunkParameters.Materials.Num());
			}
			else if(PropertyName == FName(TEXT("ChunkDrawRadius")))
			{
				ChunkDrawRadius = FMath::Max(0.0f,ChunkDrawRadius);
			}
		}

		Super::PostEditChangeProperty(PropertyChangedEvent);
	}
#endif
