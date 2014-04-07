// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once
#include "BrickGridComponent.h"
#include "BrickChunkComponent.generated.h"

/** Represents rendering and collision for a chunk of a BrickGridComponent. */
UCLASS(hidecategories=(Object,LOD,Physics), editinlinenew, ClassGroup=Rendering)
class UBrickChunkComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

public:

	// The coordinates of this chunk.
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = Chunk)
	FInt3 Coordinates;

	// The brick grid this chunk is representing.
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = Chunk)
	class UBrickGridComponent* Grid;

	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;
	virtual int32 GetNumMaterials() const OVERRIDE;
	virtual class UMaterialInterface* GetMaterial(int32 ElementIndex) const OVERRIDE;
	// End UPrimitiveComponent interface.

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const OVERRIDE;
	virtual class UBodySetup* GetBodySetup() OVERRIDE;
	// End USceneComponent interface.

private:

	// Collision body.
	UPROPERTY(transient, duplicatetransient)
	class UBodySetup* CollisionBodySetup;

	// Initializes the collision body.
	void UpdateCollisionBody();
};
