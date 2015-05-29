// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once
#include "BrickGridComponent.h"
#include "BrickRenderComponent.generated.h"

/** Represents rendering for a chunk of a BrickGridComponent. */
UCLASS(hidecategories=(Object,LOD,Physics), editinlinenew, ClassGroup=Rendering)
class BRICKGRID_API UBrickRenderComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

public:

	// The coordinates of this chunk.
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = Chunk)
	FInt3 Coordinates;

	// The brick grid this chunk is representing.
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = Chunk)
	class UBrickGridComponent* Grid;

	// Whether this chunk has a low-priority update pending (e.g. ambient occlusion). These updates are spread over multiple frames.
	UPROPERTY()
	bool HasLowPriorityUpdatePending;

	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	// End UPrimitiveComponent interface.

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const override;
	// End USceneComponent interface.
};
