// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once

#include "BrickGridComponent.generated.h"

/** Component that renders a 3D grid of cubes. */
UCLASS(hidecategories=(Object,LOD, Physics), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class UBrickGridComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

public:

	UFUNCTION(BlueprintCallable, Category=BrickGrid)
	void Init(int32 BrickCountX, int32 BrickCountY, int32 BrickCountZ,int32 BrickMaterialCount,int32 DefaultMaterial);

	int32 Get(const FIntVector& XYZ) const;
	UFUNCTION(BlueprintCallable, Category = BrickGrid)
	int32 Get(int32 X,int32 Y,int32 Z) const;

	void Set(const FIntVector& XYZ,int32 MaterialIndex);
	UFUNCTION(BlueprintCallable, Category = BrickGrid)
	void Set(int32 X,int32 Y,int32 Z,int32 MaterialIndex);

	uint32 GetSizeX() const { return SizeX; }
	uint32 GetSizeY() const { return SizeY; }
	uint32 GetSizeZ() const { return SizeZ; }
	uint32 GetNumBrickMaterials() const { return NumBrickMaterials; }

	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;
	// End UPrimitiveComponent interface.

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const OVERRIDE;
	// End USceneComponent interface.

	// Begin UMeshComponent interface.
	virtual int32 GetNumMaterials() const OVERRIDE{ return GetNumBrickMaterials(); }
	// End UMeshComponent interface.

private:

	UPROPERTY()
	TArray<uint32> BrickContents;

	UPROPERTY()
	uint32 SizeX;

	UPROPERTY()
	uint32 SizeY;

	UPROPERTY()
	uint32 SizeZ;

	UPROPERTY()
	uint32 NumBrickMaterials;

	UPROPERTY()
	int32 DefaultMaterialIndex;

	UPROPERTY()
	uint32 BitsPerBrick;

	UPROPERTY()
	uint32 BitsPerBrickLog2;

	UPROPERTY()
	uint32 BricksPerDWordLog2;

	void CalcIndexShiftMask(uint32 X,uint32 Y,uint32 Z,uint32& OutDWordIndex,uint32& OutShift,uint32& OutMask) const;
};
