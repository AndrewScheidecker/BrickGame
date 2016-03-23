// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once
#include "BrickGridComponent.generated.h"

namespace BrickGridConstants
{
	enum { MaxBricksPerRegionAxisLog2 = 7 };
	enum { MaxBricksPerRegionAxis = 1 << MaxBricksPerRegionAxisLog2 };
};

/** Shifts a number right with sign extension. */
inline int32 SignedShiftRight(int32 A,int32 B)
{
	// The C standard doesn't define whether shifting a signed integer right will extend the sign bit, but the VC2013 compiler does so.
	// If using a different compiler, assert that it does the same, although it is our problem if it does not.
	#if defined(_MSC_VER) && _MSC_VER == 1800
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

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = Bricks)
	class UMaterialInterface* OverrideTopSurfaceMaterial;

	FBrickMaterial() : SurfaceMaterial(NULL), OverrideTopSurfaceMaterial(NULL) {}
};

/** Information about a brick. */
USTRUCT(BlueprintType)
struct FBrick
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = Bricks)
	int32 MaterialIndex;

	FBrick() {}
	FBrick(int32 InMaterialIndex) : MaterialIndex(InMaterialIndex) {}
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
	FVector ToFloat() const { return FVector(X,Y,Z); }
	int32 SumComponents() const { return X + Y + Z; }

	friend uint32 GetTypeHash(const FInt3& Coordinates)
	{
		return FCrc::MemCrc32(&Coordinates,sizeof(Coordinates));
	}
	#define DEFINE_VECTOR_OPERATOR(symbol) \
		friend FInt3 operator symbol(const FInt3& A, const FInt3& B) \
		{ \
			return FInt3(A.X symbol B.X, A.Y symbol B.Y, A.Z symbol B.Z); \
		}
	DEFINE_VECTOR_OPERATOR(+);
	DEFINE_VECTOR_OPERATOR(-);
	DEFINE_VECTOR_OPERATOR(*);
	DEFINE_VECTOR_OPERATOR(/);
	DEFINE_VECTOR_OPERATOR(&);
	DEFINE_VECTOR_OPERATOR(<<);
	DEFINE_VECTOR_OPERATOR(>>);
	DEFINE_VECTOR_OPERATOR(<);
	DEFINE_VECTOR_OPERATOR(<=);
	DEFINE_VECTOR_OPERATOR(>);
	DEFINE_VECTOR_OPERATOR(>=);
	friend bool operator==(const FInt3& A, const FInt3& B)
	{
		return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
	}
	static inline FInt3 SignedShiftRight(const FInt3& A,const FInt3& B)
	{
		return FInt3(::SignedShiftRight(A.X,B.X),::SignedShiftRight(A.Y,B.Y),::SignedShiftRight(A.Z,B.Z));
	}
	static inline FInt3 Exp2(const FInt3& A)
	{
		return FInt3(1 << A.X,1 << A.Y,1 << A.Z);
	}
	static inline FInt3 CeilLog2(const FInt3& A)
	{
		return FInt3(FMath::CeilLogTwo(A.X),FMath::CeilLogTwo(A.Y),FMath::CeilLogTwo(A.Z));
	}
	static inline FInt3 Max(const FInt3& A,const FInt3& B)
	{
		return FInt3(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), FMath::Max(A.Z, B.Z));
	}
	static inline FInt3 Min(const FInt3& A,const FInt3& B)
	{
		return FInt3(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Min(A.Z, B.Z));
	}
	static inline FInt3 Clamp(const FInt3& A,const FInt3& MinA,const FInt3& MaxA)
	{
		return Min(Max(A,MinA),MaxA);
	}
	static inline FInt3 Floor(const FVector& V)
	{
		return FInt3(FMath::FloorToInt(V.X), FMath::FloorToInt(V.Y), FMath::FloorToInt(V.Z));
	}
	static inline FInt3 Ceil(const FVector& V)
	{
		return FInt3(FMath::CeilToInt(V.X), FMath::CeilToInt(V.Y), FMath::CeilToInt(V.Z));
	}
	static inline bool Any(const FInt3& A)
	{
		return A.X || A.Y || A.Z;
	}
	static inline bool All(const FInt3& A)
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

	// Contains the material index for each brick, stored in an 8-bit integer.
	UPROPERTY()
	TArray<uint8> BrickContents;

	// Contains the occupied brick with highest Z in this region for each XY coordinate in the region. -1 means no non-empty bricks in this region at that XY.
	TArray<int8> MaxNonEmptyBrickRegionZs;
};

/** The parameters for a BrickGridComponent. */
USTRUCT(BlueprintType)
struct FBrickGridParameters
{
	GENERATED_USTRUCT_BODY()

	// The materials to render for each brick material.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Materials)
	TArray<FBrickMaterial> Materials;

	// The materials to render for each brick material.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Materials)
		TArray<uint8> TranslucentMaterialsIndexes;

	// The material index that means "empty".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Materials)
	int32 EmptyMaterialIndex;

	// The number of bricks along each axis of a region is 2^BricksPerChunkLog2
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Chunks)
	FInt3 BricksPerRegionLog2;

	// The number of chunks along each axis of a region is 2^ChunksPerRegionLog2
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Regions)
	FInt3 RenderChunksPerRegionLog2;

	// The number of collision chunks along each axis of a region is 2^ChunksPerRegionLog2
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Regions)
	FInt3 CollisionChunksPerRegionLog2;

	// The minimum region coordinates allowed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Regions)
	FInt3 MinRegionCoordinates;

	// The maximum region coordinates allowed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Regions)
	FInt3 MaxRegionCoordinates;

	// The radius in bricks of the blur applied to the ambient occlusion.
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category=Lighting)
	int32 AmbientOcclusionBlurRadius;

	FBrickGridParameters();
};

/** Allows blueprint to extract a black-box representation of the brick grid's persistent data. */
USTRUCT(BlueprintType)
struct FBrickGridData
{
	GENERATED_USTRUCT_BODY()

	// All regions of the grid.
	UPROPERTY()
	TArray<struct FBrickRegion> Regions;
};

// The type of OnInitRegion delegates.
DECLARE_DYNAMIC_DELEGATE_OneParam(FBrickGrid_InitRegion,FInt3,RegionCoordinates);

/** A 3D grid of bricks. */
UCLASS(hidecategories=(Object,LOD, Physics), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class BRICKGRID_API UBrickGridComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	// Initializes the grid with the given parameters.
	UFUNCTION(BlueprintCallable,Category = "Brick Grid")
	void Init(const FBrickGridParameters& Parameters);

	// Returns a copy of the grid's brick data.
	UFUNCTION(BlueprintCallable,Category = "Brick Grid")
	FBrickGridData GetData() const;

	// Sets the grid's brick data.
	UFUNCTION(BlueprintCallable,Category = "Brick Grid")
	void SetData(const FBrickGridData& Data);

	// Reads the brick at the given coordinates.
	UFUNCTION(BlueprintCallable,Category = "Brick Grid")
	FBrick GetBrick(const FInt3& BrickCoordinates) const;

	void GetBrickMaterialArray(const FInt3& MinBrickCoordinates,const FInt3& MaxBrickCoordinates,TArray<uint8>& OutBrickMaterials) const;
	void SetBrickMaterialArray(const FInt3& MinBrickCoordinates,const FInt3& MaxBrickCoordinates,const TArray<uint8>& BrickMaterials);

	// Returns a height-map containing the non-empty brick with greatest Z for each XY in the rectangle bounded by MinBrickCoordinates.XY-MaxBrickCoordinates.XY.
	// The returned heights are relative to MinBrickCoordinates.Z, but MaxBrickCoordinates.Z is ignored.
	// OutHeightmap should be allocated by the caller to contain an int8 for each XY in the rectangle, and is indexed by OutHeightMap[Y * SizeX + X].
	void GetMaxNonEmptyBrickZ(const FInt3& MinBrickCoordinates,const FInt3& MaxBrickCoordinates,TArray<int8>& OutHeightMap) const;

	// Writes the brick at the given coordinates.
	UFUNCTION(BlueprintCallable,Category = "Brick Grid")
	bool SetBrick(const FInt3& BrickCoordinates,int32 MaterialIndex);

	// Invalidates the chunk components for a range of brick coordinates.
	UFUNCTION(BlueprintCallable,Category = "Brick Grid")
	void InvalidateChunkComponents(const FInt3& MinBrickCoordinates,const FInt3& MaxBrickCoordinates);

	// Updates the visible chunks for a given view position.
	UFUNCTION(BlueprintCallable,Category = "Brick Grid")
	void Update(const FVector& WorldViewPosition,float MaxDrawDistance,float MaxCollisionDistance,float MaxDesiredUpdateTime,FBrickGrid_InitRegion InitRegion);

	// The parameters for the grid.
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Brick Grid")
	FBrickGridParameters Parameters;

	// Properties derived from the grid parameters.

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Brick Grid")
	FInt3 BricksPerRenderChunk;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Brick Grid")
	FInt3 BricksPerCollisionChunk;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Brick Grid")
	FInt3 BricksPerRenderChunkLog2;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Brick Grid")
	FInt3 BricksPerCollisionChunkLog2;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Brick Grid")
	FInt3 RenderChunksPerRegion;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Brick Grid")
	FInt3 CollisionChunksPerRegion;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Brick Grid")
	FInt3 BricksPerRegion;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Brick Grid")
	FInt3 MinBrickCoordinates;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Brick Grid")
	FInt3 MaxBrickCoordinates;

	inline FInt3 BrickToRenderChunkCoordinates(const FInt3& BrickCoordinates) const
	{
		return FInt3::SignedShiftRight(BrickCoordinates,BricksPerRenderChunkLog2);
	}
	inline FInt3 BrickToCollisionChunkCoordinates(const FInt3& BrickCoordinates) const
	{
		return FInt3::SignedShiftRight(BrickCoordinates,BricksPerCollisionChunkLog2);
	}
	inline FInt3 BrickToRegionCoordinates(const FInt3& BrickCoordinates) const
	{
		return FInt3::SignedShiftRight(BrickCoordinates,Parameters.BricksPerRegionLog2);
	}

	// USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const override;
	// UActorComponent interface.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

private:

	// All regions of the grid.
	UPROPERTY(Transient,DuplicateTransient)
	TArray<struct FBrickRegion> Regions;

	// Transient maps to help lookup regions and chunks by coordinates.
	TMap<FInt3,int32> RegionCoordinatesToIndex;
	TMap<FInt3,class UBrickRenderComponent*> RenderChunkCoordinatesToComponent;
	TMap<FInt3,class UBrickCollisionComponent*> CollisionChunkCoordinatesToComponent;

	// Initializes the derived constants from the properties they are derived from.
	void ComputeDerivedConstants();

	// Creates a chunk for the given coordinates.
	void CreateChunk(const FInt3& Coordinates);

	// Creates a region for the given coordinates.
	void CreateRegion(const FInt3& Coordinates,FBrickGrid_InitRegion OnInitRegion);

	// Maps brick coordinates within a region to a brick index.
	inline uint32 SubregionBrickCoordinatesToRegionBrickIndex(const FInt3 SubregionBrickCoordinates) const
	{
		return (((SubregionBrickCoordinates.Y << Parameters.BricksPerRegionLog2.X) + SubregionBrickCoordinates.X) << Parameters.BricksPerRegionLog2.Z) + SubregionBrickCoordinates.Z;
	}
	inline uint32 BrickCoordinatesToRegionBrickIndex(const FInt3& RegionCoordinates,const FInt3& BrickCoordinates) const
	{
		return SubregionBrickCoordinatesToRegionBrickIndex(BrickCoordinates - (RegionCoordinates << Parameters.BricksPerRegionLog2));
		
	}

	// Updates the non-empty height map for a single region.
	void UpdateMaxNonEmptyBrickMap(FBrickRegion& Region,const FInt3 MinDirtyBrickCoordinates,const FInt3 MaxDirtyBrickCoordinates) const;
};
