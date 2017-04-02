// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "VoxelLandscapeComponent.generated.h"

namespace VoxelLandscapeConstants
{
	enum { MaxVoxelsPerRegionAxisLog2 = 7 };
	enum { MaxVoxelsPerRegionAxis = 1 << MaxVoxelsPerRegionAxisLog2 };
};

/** Shifts a number right with sign extension. */
inline int32 SignedShiftRight(int32 A, int32 B)
{
	// The C standard doesn't define whether shifting a signed integer right will extend the sign bit, but the VC2013 compiler does so.
	// If using a different compiler, assert that it does the same, although it is our problem if it does not.
#if defined(_MSC_VER) && _MSC_VER == 1800
	return A >> B;
#else
	const int32 Result = A >> B;
	if (A < 0)
	{
		check(~(~A >> B) == Result);
	}
	return Result;
#endif
}

/** Information about a Voxel material. */
USTRUCT(BlueprintType)
struct FVoxelMaterial
{
	GENERATED_USTRUCT_BODY()

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Voxels)
		class UMaterialInterface* SurfaceMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Voxels)
		class UMaterialInterface* OverrideTopSurfaceMaterial;

	FVoxelMaterial() : SurfaceMaterial(NULL), OverrideTopSurfaceMaterial(NULL) {}
};

/** Information about a Voxel. */
USTRUCT(BlueprintType)
struct FVoxel
{
	GENERATED_USTRUCT_BODY()

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Voxels)
		int32 MaterialIndex;

	FVoxel() {}
	FVoxel(int32 InMaterialIndex) : MaterialIndex(InMaterialIndex) {}
};

/** A 3D integer vector. */
USTRUCT(BlueprintType, Atomic)
struct FInt3
{
	GENERATED_USTRUCT_BODY()

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Coordinates)
		int32 X;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Coordinates)
		int32 Y;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Coordinates)
		int32 Z;

	FInt3() {}
	FInt3(int32 InX, int32 InY, int32 InZ) : X(InX), Y(InY), Z(InZ) {}

	static FInt3 Scalar(int32 I)
	{
		return FInt3(I, I, I);
	}

	operator FIntVector() const { return FIntVector(X, Y, Z); }
	FVector ToFloat() const { return FVector(X, Y, Z); }
	int32 SumComponents() const { return X + Y + Z; }

	friend uint32 GetTypeHash(const FInt3& Coordinates)
	{
		return FCrc::MemCrc32(&Coordinates, sizeof(Coordinates));
	}
#define DEFINE_VECTOR_OPERATOR(symbol) \
		friend FInt3 operator symbol(const FInt3& A, const FInt3& B) \
		{ \
			return FInt3(A.X symbol B.X, A.Y symbol B.Y, A.Z symbol B.Z); \
		}
	DEFINE_VECTOR_OPERATOR(+);
	DEFINE_VECTOR_OPERATOR(-);
	DEFINE_VECTOR_OPERATOR(*);
	DEFINE_VECTOR_OPERATOR(/ );
	DEFINE_VECTOR_OPERATOR(&);
	DEFINE_VECTOR_OPERATOR(<< );
	DEFINE_VECTOR_OPERATOR(>> );
	DEFINE_VECTOR_OPERATOR(<);
	DEFINE_VECTOR_OPERATOR(<= );
	DEFINE_VECTOR_OPERATOR(>);
	DEFINE_VECTOR_OPERATOR(>= );
	friend bool operator==(const FInt3& A, const FInt3& B)
	{
		return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
	}
	static inline FInt3 SignedShiftRight(const FInt3& A, const FInt3& B)
	{
		return FInt3(::SignedShiftRight(A.X, B.X), ::SignedShiftRight(A.Y, B.Y), ::SignedShiftRight(A.Z, B.Z));
	}
	static inline FInt3 Exp2(const FInt3& A)
	{
		return FInt3(1 << A.X, 1 << A.Y, 1 << A.Z);
	}
	static inline FInt3 CeilLog2(const FInt3& A)
	{
		return FInt3(FMath::CeilLogTwo(A.X), FMath::CeilLogTwo(A.Y), FMath::CeilLogTwo(A.Z));
	}
	static inline FInt3 Max(const FInt3& A, const FInt3& B)
	{
		return FInt3(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), FMath::Max(A.Z, B.Z));
	}
	static inline FInt3 Min(const FInt3& A, const FInt3& B)
	{
		return FInt3(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Min(A.Z, B.Z));
	}
	static inline FInt3 Clamp(const FInt3& A, const FInt3& MinA, const FInt3& MaxA)
	{
		return Min(Max(A, MinA), MaxA);
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

/** A region of the Voxel grid. */
USTRUCT()
struct FVoxelRegion
{
	GENERATED_USTRUCT_BODY()

		// The coordinates of the region.
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Region)
		FInt3 Coordinates;

	// Contains the material index for each Voxel, stored in an 8-bit integer.
	UPROPERTY()
		TArray<uint8> VoxelContents;

	// Contains the occupied Voxel with highest Z in this region for each XY coordinate in the region. -1 means no non-empty Voxels in this region at that XY.
	TArray<int8> MaxNonEmptyVoxelRegionZs;

	// Contains the coordinates of the complex bricks in the region, and their Material Indexes
	TMap<FInt3, uint8>RegionComplexVoxelsIndexes;
};

/** The parameters for a VoxelLandscapeComponent. */
USTRUCT(BlueprintType)
struct FVoxelLandscapeParameters
{
	GENERATED_USTRUCT_BODY()

		// The materials to render for each Voxel material.
		UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Materials)
		TArray<FVoxelMaterial> Materials;

	// The material index that means "empty".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Materials)
		int32 EmptyMaterialIndex;

	// The number of Voxels along each axis of a region is 2^VoxelsPerChunkLog2
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Chunks)
		FInt3 VoxelsPerRegionLog2;

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

	// The radius in Voxels of the blur applied to the ambient occlusion.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Lighting)
		int32 AmbientOcclusionBlurRadius;

	FVoxelLandscapeParameters();
};

/** Allows blueprint to extract a black-box representation of the Voxel grid's persistent data. */
USTRUCT(BlueprintType)
struct FVoxelLandscapeData
{
	GENERATED_USTRUCT_BODY()

		// All regions of the grid.
		UPROPERTY()
		TArray<struct FVoxelRegion> Regions;
};

// The type of OnInitRegion delegates.
DECLARE_DYNAMIC_DELEGATE_OneParam(FVoxelLandscape_InitRegion, FInt3, RegionCoordinates);

/** A 3D grid of Voxels. */
UCLASS(hidecategories = (Object, LOD, Physics), editinlinenew, meta = (BlueprintSpawnableComponent), ClassGroup = Rendering)
class VOXELLANDSCAPE_API UVoxelLandscapeComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	// Initializes the grid with the given parameters.
	UFUNCTION(BlueprintCallable, Category = "Voxel Grid")
		void Init(const FVoxelLandscapeParameters& Parameters);

	// Returns a copy of the grid's Voxel data.
	UFUNCTION(BlueprintCallable, Category = "Voxel Grid")
		FVoxelLandscapeData GetData() const;

	// Returns a two arrays, one for the coordinates of the complex voxels of a region and another for their material indices
	UFUNCTION(BlueprintCallable, Category = "Brick Grid")
		void GetRegionComplexVoxelsCoordinates(const FInt3 RegionCoordinates, TArray<FInt3> &outCoordinates, TArray<uint8> &outComplexVoxelIndices);

	// Sets the grid's Voxel data.
	UFUNCTION(BlueprintCallable, Category = "Voxel Grid")
		void SetData(const FVoxelLandscapeData& Data);

	// Reads the Voxel at the given coordinates.
	UFUNCTION(BlueprintCallable, Category = "Voxel Grid")
		FVoxel GetVoxel(const FInt3& VoxelCoordinates) const;

	void GetVoxelMaterialArray(const FInt3& MinVoxelCoordinates, const FInt3& MaxVoxelCoordinates, TArray<uint8>& OutVoxelMaterials) const;
	void SetVoxelMaterialArray(const FInt3& MinVoxelCoordinates, const FInt3& MaxVoxelCoordinates, const TArray<uint8>& VoxelMaterials);

	// Returns a height-map containing the non-empty Voxel with greatest Z for each XY in the rectangle bounded by MinVoxelCoordinates.XY-MaxVoxelCoordinates.XY.
	// The returned heights are relative to MinVoxelCoordinates.Z, but MaxVoxelCoordinates.Z is ignored.
	// OutHeightmap should be allocated by the caller to contain an int8 for each XY in the rectangle, and is indexed by OutHeightMap[Y * SizeX + X].
	void GetMaxNonEmptyVoxelZ(const FInt3& MinVoxelCoordinates, const FInt3& MaxVoxelCoordinates, TArray<int8>& OutHeightMap) const;

	// Writes the Voxel at the given coordinates.
	UFUNCTION(BlueprintCallable, Category = "Voxel Grid")
		bool SetVoxel(const FInt3& VoxelCoordinates, int32 MaterialIndex);

	// Invalidates the chunk components for a range of Voxel coordinates.
	UFUNCTION(BlueprintCallable, Category = "Voxel Grid")
		void InvalidateChunkComponents(const FInt3& MinVoxelCoordinates, const FInt3& MaxVoxelCoordinates);

	// Updates the visible chunks for a given view position.
	UFUNCTION(BlueprintCallable, Category = "Voxel Grid")
		void Update(const FVector& WorldViewPosition, float MaxDrawDistance, float MaxCollisionDistance, float MaxDesiredUpdateTime, FVoxelLandscape_InitRegion InitRegion);

	// The parameters for the grid.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Grid")
		FVoxelLandscapeParameters Parameters;

	// Properties derived from the grid parameters.

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Grid")
		FInt3 VoxelsPerRenderChunk;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Grid")
		FInt3 VoxelsPerCollisionChunk;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Grid")
		FInt3 VoxelsPerRenderChunkLog2;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Grid")
		FInt3 VoxelsPerCollisionChunkLog2;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Grid")
		FInt3 RenderChunksPerRegion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Grid")
		FInt3 CollisionChunksPerRegion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Grid")
		FInt3 VoxelsPerRegion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Grid")
		FInt3 MinVoxelCoordinates;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Grid")
		FInt3 MaxVoxelCoordinates;

	inline FInt3 VoxelToRenderChunkCoordinates(const FInt3& VoxelCoordinates) const
	{
		return FInt3::SignedShiftRight(VoxelCoordinates, VoxelsPerRenderChunkLog2);
	}
	inline FInt3 VoxelToCollisionChunkCoordinates(const FInt3& VoxelCoordinates) const
	{
		return FInt3::SignedShiftRight(VoxelCoordinates, VoxelsPerCollisionChunkLog2);
	}
	inline FInt3 VoxelToRegionCoordinates(const FInt3& VoxelCoordinates) const
	{
		return FInt3::SignedShiftRight(VoxelCoordinates, Parameters.VoxelsPerRegionLog2);
	}

	// USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const override;
	// UActorComponent interface.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

private:

	// All regions of the grid.
	UPROPERTY(Transient, DuplicateTransient)
		TArray<struct FVoxelRegion> Regions;

	// Transient maps to help lookup regions and chunks by coordinates.
	TMap<FInt3, int32> RegionCoordinatesToIndex;
	TMap<FInt3, class UVoxelRenderComponent*> RenderChunkCoordinatesToComponent;
	TMap<FInt3, class UVoxelCollisionComponent*> CollisionChunkCoordinatesToComponent;

	// Initializes the derived constants from the properties they are derived from.
	void ComputeDerivedConstants();

	// Creates a chunk for the given coordinates.
	void CreateChunk(const FInt3& Coordinates);

	// Creates a region for the given coordinates.
	void CreateRegion(const FInt3& Coordinates, FVoxelLandscape_InitRegion OnInitRegion);

	// Maps Voxel coordinates within a region to a Voxel index.
	inline uint32 SubregionVoxelCoordinatesToRegionVoxelIndex(const FInt3 SubregionVoxelCoordinates) const
	{
		return (((SubregionVoxelCoordinates.Y << Parameters.VoxelsPerRegionLog2.X) + SubregionVoxelCoordinates.X) << Parameters.VoxelsPerRegionLog2.Z) + SubregionVoxelCoordinates.Z;
	}
	inline uint32 VoxelCoordinatesToRegionVoxelIndex(const FInt3& RegionCoordinates, const FInt3& VoxelCoordinates) const
	{
		return SubregionVoxelCoordinatesToRegionVoxelIndex(VoxelCoordinates - (RegionCoordinates << Parameters.VoxelsPerRegionLog2));

	}

	// Updates the non-empty height map for a single region.
	void UpdateMaxNonEmptyVoxelMap(FVoxelRegion& Region, const FInt3 MinDirtyVoxelCoordinates, const FInt3 MaxDirtyVoxelCoordinates) const;
	// Updates the TMap of complex voxels for a single region.
	void UpdateRegionComplexVoxelsMap(FVoxelRegion& Region, const FInt3 MinDirtyRegionBrickCoordinates, const FInt3 MaxDirtyRegionBrickCoordinates) const;
};