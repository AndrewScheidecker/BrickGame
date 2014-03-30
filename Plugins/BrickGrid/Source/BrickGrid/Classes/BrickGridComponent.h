// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once

#include "BrickGridComponent.generated.h"

USTRUCT()
struct FBrickLayer
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Layer)
	uint8 MaterialIndex;

	UPROPERTY(EditAnywhere, Category = Layer)
	float MinThickness;

	UPROPERTY(EditAnywhere, Category = Layer)
	float MaxThickness;

	UPROPERTY(EditAnywhere, Category = Layer)
	float NoiseScale;
};

/** Component that renders a 3D grid of cubes. */
UCLASS(hidecategories=(Object,LOD, Physics), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class UBrickGridComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BrickGrid)
	int32 BrickCountX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BrickGrid)
	int32 BrickCountY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BrickGrid)
	int32 BrickCountZ;

	UPROPERTY(EditAnywhere, Category = BrickGrid)
	TArray<FBrickLayer> Layers;

private:

	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;
	// End UPrimitiveComponent interface.

	// Begin UMeshComponent interface.
	virtual int32 GetNumMaterials() const OVERRIDE;
	// End UMeshComponent interface.

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const OVERRIDE;
	// End USceneComponent interface.

	// Begin UObject interface.
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);
	// End UObject interface. 

	friend class FBrickGridSceneProxy;

	UPROPERTY()
	TArray<uint8> BrickContents;
	
	void InitBrickContents();
};
