// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once
#include "BrickGridComponent.generated.h"

/** Shifts a number right with sign extension. */
inline int32 SignedShiftRight(int32 A,int32 B)
{
	// The C standard doesn't define whether shifting a signed integer right will extend the sign bit, but the VC2013 compiler does so.
	// If using a different compiler, assert that it does the same, although it is our problem if it does not.
	#if 0 && defined(_MSC_VER) && _MSC_VER == 1800
		return A >> B;
	#else
		const int32 Result = A >> B;
		if(A < 0)
		{
			check(~(~A >> B) == Result);
		}
		return Result;
	#endif
}

/** Information about a brick material. */
USTRUCT(BlueprintType)
struct FBrickMaterial
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = Bricks)
	class UMaterialInterface* SurfaceMaterial;

	FBrickMaterial() : SurfaceMaterial(NULL) {}
};

/** A 3D integer vector. */
USTRUCT(BlueprintType,Atomic)
struct FInt3
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = Coordinates)
	int32 X;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = Coordinates)
	int32 Y;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = Coordinates)
	int32 Z;

	FInt3() {}
	FInt3(int32 InX,int32 InY,int32 InZ) : X(InX), Y(InY), Z(InZ) {}

	static FInt3 Scalar(int32 I)
	{
		return FInt3(I,I,I);
	}

	operator FIntVector() const { return FIntVector(X,Y,Z); }
	operator FVector() const { return FVector(X,Y,Z); }

	friend uint32 GetTypeHash(const FInt3& Coordinates)
	{
		return FCrc::MemCrc32(&Coordinates,sizeof(Coordinates));
	}
	#define DEFINE_VECTOR_OPERATOR(symbol) \
		friend FInt3 operator##symbol(const FInt3& A, const FInt3& B) \
		{ \
			return FInt3(A.X symbol B.X, A.Y symbol B.Y, A.Z symbol B.Z); \
		}
	DEFINE_VECTOR_OPERATOR(+);
	DEFINE_VECTOR_OPERATOR(-);
	DEFINE_VECTOR_OPERATOR(*);
	DEFINE_VECTOR_OPERATOR(/);
	DEFINE_VECTOR_OPERATOR(&);
	DEFINE_VECTOR_OPERATOR(<<);
	DEFINE_VECTOR_OPERATOR(<);
	DEFINE_VECTOR_OPERATOR(<=);
	DEFINE_VECTOR_OPERATOR(>);
	DEFINE_VECTOR_OPERATOR(>=);
	friend bool operator==(const FInt3& A, const FInt3& B)
	{
		return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
	}
	friend FInt3 SignedShiftRight(const FInt3& A,const FInt3& B)
	{
		return FInt3(SignedShiftRight(A.X,B.X),SignedShiftRight(A.Y,B.Y),SignedShiftRight(A.Z,B.Z));
	}
	friend FInt3 Exp2(const FInt3& A)
	{
		return FInt3(1 << A.X,1 << A.Y,1 << A.Z);
	}
	friend FInt3 CeilLog2(const FInt3& A)
	{
		return FInt3(FMath::CeilLogTwo(A.X),FMath::CeilLogTwo(A.Z),FMath::CeilLogTwo(A.Z));
	}
	friend FInt3 Max(const FInt3& A,const FInt3& B)
	{
		return FInt3(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), FMath::Max(A.Z, B.Z));
	}
	friend FInt3 Min(const FInt3& A,const FInt3& B)
	{
		return FInt3(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Min(A.Z, B.Z));
	}
	friend FInt3 Clamp(const FInt3& A,const FInt3& MinA,const FInt3& MaxA)
	{
		return Min(Max(A,MinA),MaxA);
	}
	friend FInt3 Floor(const FVector& V)
	{
		return FInt3(FMath::Floor(V.X),FMath::Floor(V.Y),FMath::Floor(V.Z));
	}
	friend FInt3 Ceil(const FVector& V)
	{
		return FInt3(FMath::Ceil(V.X),FMath::Ceil(V.Y),FMath::Ceil(V.Z));
	}
	friend bool Any(const FInt3& A)
	{
		return A.X || A.Y || A.Z;
	}
	friend bool All(const FInt3& A)
	{
		return A.X && A.Y && A.Z;
	}
};

/** A region of the brick grid. */
USTRUCT()
struct FBrickRegion
{
	GENERATED_USTRUCT_BODY()

	// The coordinates of the region.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Region)
	FInt3 Coordinates;

	// Contains the material index for each brick, packed into 32-bit integers.
	UPROPERTY()
	TArray<uint32> BrickContents;
};

// The type of OnInitChunk delegates.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBrickGrid_InitRegion,
	class UBrickGridComponent*,BrickGrid,
	const FInt3&,RegionCoordinates
	);

/** A 3D grid of brick chunks. */
UCLASS(hidecategories=(Object,LOD, Physics), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class UBrickGridComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	// The distance from the camera beyond which bricks may no longer be drawn.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = View)
	float MaxDrawDistance;

	// The materials to render for each brick material.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Materials)
	TArray<FBrickMaterial> Materials;

	// The material index that means "empty".
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Materials)
	int32 EmptyMaterialIndex;

	// The number of bricks along each axis of a chunk is 2^BricksPerChunkLog2
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Chunks)
	FInt3 BricksPerChunkLog2;

	// The class to use for chunks.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Chunks)
	TSubclassOf<class UBrickChunkComponent> ChunkClass;

	// The number of chunks along each axis of a region is 2^ChunksPerRegionLog2
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Regions)
	FInt3 ChunksPerRegionLog2;

	// The minimum chunk coordinates allowed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Regions)
	FInt3 MinRegionCoordinates;

	// The maximum chunk coordinates allowed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Regions)
	FInt3 MaxRegionCoordinates;

	// A delegate that is called when a new chunk is created.
	UPROPERTY(BlueprintAssignable, Category = Regions)
	FBrickGrid_InitRegion OnInitRegion;

	// Reads the brick at the given coordinates.
	UFUNCTION(BlueprintCallable,Category = "Brick Grid")
	BRICKGRID_API int32 GetBrick(const FInt3& BrickCoordinates) const;

	// Writes the brick at the given coordinates.
	UFUNCTION(BlueprintCallable,Category = "Brick Grid")
	BRICKGRID_API bool SetBrick(const FInt3& BrickCoordinates,int32 MaterialIndex);

	// Updates the visible chunks for a given view position.
	UFUNCTION(BlueprintCallable,Category = "Brick Grid")
	BRICKGRID_API void UpdateVisibleChunks(const FVector& WorldViewPosition,int32 MaxRegionsToCreate);

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category = Chunks)
	FInt3 BricksPerChunk;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category = Regions)
	FInt3 ChunksPerRegion;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category = Regions)
	FInt3 BricksPerRegionLog2;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category = Regions)
	FInt3 BricksPerRegion;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category = Bricks)
	FInt3 MinBrickCoordinates;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category = Bricks)
	FInt3 MaxBrickCoordinates;

	// USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const OVERRIDE;
	// UObject interface
	#if WITH_EDITOR
		virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
	#endif
	virtual void PostLoad() OVERRIDE;

private:

	// All regions of the grid.
	UPROPERTY()
	TArray<struct FBrickRegion> Regions;

	// The currently visibly chunks.
	UPROPERTY(Transient,DuplicateTransient)
	TArray<class UBrickChunkComponent*> VisibleChunks;

	// Transient maps to help lookup regions and chunks by coordinates.
	TMap<FInt3,int32> RegionCoordinatesToIndex;
	TMap<FInt3,class UBrickChunkComponent*> ChunkCoordinatesToComponent;

	// The number of bits used to store each brick's material index in BrickContents.
	uint32 BitsPerBrick;
	// Log base 2 of the number of bits used to store each brick's material index in BrickContents.
	uint32 BitsPerBrickLog2;
	// Log base 2 of the number of brick material indices packed into each 32-bit integer in BrickContents.
	uint32 BricksPerDWordLog2;

	// Initializes the derived constants from the properties they are derived from.
	void ComputeDerivedConstants();

	// Calculates the index in BrickContents, the shift, and the mask for the brick at some coordinates. The mask is in terms of the bits that have been shifted so the LSB is in bit 0.
	void CalcIndexShiftMask(uint32 X,uint32 Y,uint32 Z,uint32& OutDWordIndex,uint32& OutShift,uint32& OutMask) const;

	// Creates a chunk for the given coordinates.
	void CreateChunk(const FInt3& Coordinates);

	// Creates a region for the given coordinates.
	void CreateRegion(const FInt3& Coordinates);

	inline FInt3 BrickToChunkCoordinates(const FInt3& BrickCoordinates) const
	{
		return SignedShiftRight(BrickCoordinates,BricksPerChunkLog2);
	}
	inline FInt3 BrickToRegionCoordinates(const FInt3& BrickCoordinates) const
	{
		return SignedShiftRight(BrickCoordinates,BricksPerRegionLog2);
	}
};
