// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once

#include "BrickGridComponent.generated.h"

/** Component that renders a 3D grid of cubes. */
UCLASS(hidecategories=(Object,LOD, Physics), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class UBrickGridComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

public:

	/**	Initializes the brick grid with the given dimensions and materials.
		Each brick may be one of BrickMaterialCount materials.
		EmptyMaterialIndex is the index of the material that denotes an empty brick. */
	UFUNCTION(BlueprintCallable, Category=BrickGrid)
	void Init(int32 BrickCountX, int32 BrickCountY, int32 BrickCountZ,int32 BrickMaterialCount,int32 EmptyMaterialIndex);

	/** Reads the material index of the brick at some coordinates. A read from out-of-bounds coordinates returns EmptyMaterialIndex. */
	UFUNCTION(BlueprintCallable, Category = BrickGrid)
	int32 GetBrick(int32 X,int32 Y,int32 Z) const;
	int32 GetBrick(const FIntVector& XYZ) const;

	/** Writes the material of the brick at some coordinates. */
	UFUNCTION(BlueprintCallable, Category = BrickGrid)
	void SetBrick(int32 X,int32 Y,int32 Z,int32 MaterialIndex);
	void SetBrick(const FIntVector& XYZ, int32 MaterialIndex);

	// Accessors.
	uint32 GetSizeX() const { return SizeX; }
	uint32 GetSizeY() const { return SizeY; }
	uint32 GetSizeZ() const { return SizeZ; }
	uint32 GetNumBrickMaterials() const { return NumBrickMaterials; }
	int32 GetEmptyMaterialIndex() const { return EmptyMaterialIndex; }

	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;
	// End UPrimitiveComponent interface.

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const OVERRIDE;
	virtual class UBodySetup* GetBodySetup() OVERRIDE;
	// End USceneComponent interface.

	// Begin UMeshComponent interface.
	virtual int32 GetNumMaterials() const OVERRIDE{ return GetNumBrickMaterials(); }
	// End UMeshComponent interface.

private:

	// Collision body.
	UPROPERTY(transient, duplicatetransient)
	class UBodySetup* CollisionBodySetup;

	// Contains the material index for each brick, packed into 32-bit integers.
	UPROPERTY()
	TArray<uint32> BrickContents;

	// The number of bricks in the grid along the X dimension.
	UPROPERTY()
	uint32 SizeX;

	// The number of bricks in the grid along the Y dimension.
	UPROPERTY()
	uint32 SizeY;

	// The number of bricks in the grid along the Z dimension.
	UPROPERTY()
	uint32 SizeZ;

	/// The number of different materials a brick may contain.
	UPROPERTY()
	uint32 NumBrickMaterials;

	// The material index that means "empty".
	UPROPERTY()
	int32 EmptyMaterialIndex;

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
