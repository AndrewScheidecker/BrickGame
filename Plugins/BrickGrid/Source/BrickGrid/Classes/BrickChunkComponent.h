// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once

#include "BrickChunkComponent.generated.h"

USTRUCT()
struct FBrickMaterial
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Bricks)
	class UMaterialInterface* SurfaceMaterial;

	FBrickMaterial() : SurfaceMaterial(NULL) {}
};

USTRUCT()
struct FBrickChunkParameters
{
	GENERATED_USTRUCT_BODY()

	// The number of bricks in the grid along the X dimension.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Bricks)
	int32 SizeX;

	// The number of bricks in the grid along the Y dimension.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Bricks)
	int32 SizeY;

	// The number of bricks in the grid along the Z dimension.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Bricks)
	int32 SizeZ;

	// The materials to render for each brick material.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Bricks)
	TArray<FBrickMaterial> Materials;

	// The material index that means "empty".
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Bricks)
	int32 EmptyMaterialIndex;
};

/** A 3D grid of bricks */
UCLASS(hidecategories=(Object,LOD, Physics), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class UBrickChunkComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

public:

	/**	Initializes the brick grid with the given dimensions and materials.
		Each brick may be one of BrickMaterialCount materials.
		EmptyMaterialIndex is the index of the material that denotes an empty brick. */
	UFUNCTION(BlueprintCallable, Category = Bricks)
	BRICKGRID_API void Init(const FBrickChunkParameters& Parameters);

	/** Reads the material index of the brick at some coordinates. A read from out-of-bounds coordinates returns EmptyMaterialIndex. */
	UFUNCTION(BlueprintCallable, Category = Bricks)
	BRICKGRID_API int32 GetBrick(int32 X,int32 Y,int32 Z) const;
	BRICKGRID_API int32 GetBrick(const FIntVector& XYZ) const;

	/** Writes the material of the brick at some coordinates. */
	UFUNCTION(BlueprintCallable, Category = Bricks)
	BRICKGRID_API void SetBrick(int32 X,int32 Y,int32 Z,int32 MaterialIndex);
	BRICKGRID_API void SetBrick(const FIntVector& XYZ, int32 MaterialIndex);

	/** Accesses the brick's parameters. */
	UFUNCTION(BlueprintCallable,Category=Bricks)
	BRICKGRID_API const FBrickChunkParameters& GetParameters() const;

	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;
	virtual int32 GetNumMaterials() const OVERRIDE { return GetParameters().Materials.Num(); }
	virtual class UMaterialInterface* GetMaterial(int32 ElementIndex) const OVERRIDE;
	// End UPrimitiveComponent interface.

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const OVERRIDE;
	virtual class UBodySetup* GetBodySetup() OVERRIDE;
	// End USceneComponent interface.

private:

	UPROPERTY()
	FBrickChunkParameters Parameters;

	// Collision body.
	UPROPERTY(transient, duplicatetransient)
	class UBodySetup* CollisionBodySetup;

	// Contains the material index for each brick, packed into 32-bit integers.
	UPROPERTY()
	TArray<uint32> BrickContents;

	// The number of bits used to store each brick's material index in BrickContents.
	UPROPERTY()
	uint32 BitsPerBrick;

	// Log base 2 of the number of bits used to store each brick's material index in BrickContents.
	UPROPERTY()
	uint32 BitsPerBrickLog2;

	// Log base 2 of the number of brick material indices packed into each 32-bit integer in BrickContents.
	UPROPERTY()
	uint32 BricksPerDWordLog2;

	// Calculates the index in BrickContents, the shift, and the mask for the brick at some coordinates. The mask is in terms of the bits that have been shifted so the LSB is in bit 0.
	void CalcIndexShiftMask(uint32 X,uint32 Y,uint32 Z,uint32& OutDWordIndex,uint32& OutShift,uint32& OutMask) const;

	// Initializes the collision body.
	void UpdateCollisionBody();
};
