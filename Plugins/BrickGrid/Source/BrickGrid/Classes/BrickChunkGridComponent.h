// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once
#include "BrickChunkComponent.h"
#include "BrickChunkGridComponent.generated.h"

/** 3D integer coordinates for a chunk within a chunk grid. */
USTRUCT()
struct FChunkCoordinates
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Coordinates)
	int32 X;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Coordinates)
	int32 Y;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Coordinates)
	int32 Z;

	FChunkCoordinates() {}
	FChunkCoordinates(int32 InX,int32 InY,int32 InZ) : X(InX), Y(InY), Z(InZ) {}

	friend uint32 GetTypeHash(const FChunkCoordinates& Coordinates)
	{
		return FCrc::MemCrc32(&Coordinates,sizeof(Coordinates));
	}
	friend bool operator==(const FChunkCoordinates& A,const FChunkCoordinates& B)
	{
		return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
	}
};

// The type of OnInitChunk delegates.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBrickChunkGrid_InitChunk,const FChunkCoordinates&,Coordinates,class UBrickChunkComponent*,Chunk);

/** A 3D grid of brick chunks. */
UCLASS(hidecategories=(Object,LOD, Physics), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class UBrickChunkGridComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	// The parameters used by each chunk of the grid.
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=Bricks)
	FBrickChunkParameters ChunkParameters;

	// The minimum chunk coordinates allowed.
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Bricks)
	FChunkCoordinates MinChunkCoordinates;

	// The maximum chunk coordinates allowed.
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Bricks)
	FChunkCoordinates MaxChunkCoordinates;

	// Only chunks closer to the camera than this distance will be drawn.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Bricks)
	float ChunkDrawRadius;

	// The maximum number of chunks allocated by each call to UpdateCameraPosition.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Bricks)
	int32 MaxChunksAllocatedPerFrame;

	// The class to use for chunks.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Bricks)
	TSubclassOf<class UBrickChunkComponent> ChunkClass;	

	// A delegate that is called when a new chunk is allocated.
	UPROPERTY(BlueprintAssignable,Category=Bricks)
	FBrickChunkGrid_InitChunk OnInitChunk;

	// Updates the chunk visibility for a new camera position.
	UFUNCTION(BlueprintCallable,Category=Bricks)
	void UpdateCameraPosition(const FVector& WorldCameraPosition);

	// Accesses the chunk at some coordinates, if it exists.
	UFUNCTION(BlueprintCallable,Category=Bricks)
	class UBrickChunkComponent* GetChunk(const FChunkCoordinates& Coordinates) const;

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const OVERRIDE;
	// End USceneComponent interface.

	// Begin UObject interface.
	#if WITH_EDITOR
		virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
	#endif
	// End UObject interface.

private:

	// A map from chunk coordinates to the chunk's primitive component.
	TMap<FChunkCoordinates,class UBrickChunkComponent*> ChunkMap;

	// Allocates a chunk for the given coordinates.
	void AllocateChunk(const FChunkCoordinates& Coordinates);
};
