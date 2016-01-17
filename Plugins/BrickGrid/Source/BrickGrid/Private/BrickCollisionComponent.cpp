// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickGridPluginPrivatePCH.h"
#include "BrickCollisionComponent.h"
#include "BrickGridComponent.h"
#include "PhysicsEngine/BodySetup.h"

// Maps face index to normal.
static const FInt3 FaceNormals[6] =
{
	FInt3(-1, 0, 0),
	FInt3(+1, 0, 0),
	FInt3(0, -1, 0),
	FInt3(0, +1, 0),
	FInt3(0, 0, -1),
	FInt3(0, 0, +1)
};

UBrickCollisionComponent::UBrickCollisionComponent( const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	bVisible = true;
	bAutoRegister = false;
	
	SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
}

FPrimitiveSceneProxy* UBrickCollisionComponent::CreateSceneProxy()
{
	// Update the collision body if the rendering proxy has been recreated for any reason, since the causes mostly also invalidate the collision body.
	UpdateCollisionBody();

	return NULL;
}

FBoxSphereBounds UBrickCollisionComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	FBoxSphereBounds NewBounds;
	NewBounds.Origin = NewBounds.BoxExtent = Grid->BricksPerCollisionChunk.ToFloat() / 2.0f;
	NewBounds.SphereRadius = NewBounds.BoxExtent.Size();
	return NewBounds.TransformBy(LocalToWorld);
}

class UBodySetup* UBrickCollisionComponent::GetBodySetup()
{
	if (!CollisionBodySetup)
	{
		UpdateCollisionBody();
	}
	return CollisionBodySetup;
}

void UBrickCollisionComponent::UpdateCollisionBody()
{
	const double StartTime = FPlatformTime::Seconds();
	
	if (!CollisionBodySetup)
	{
		CollisionBodySetup = NewObject<UBodySetup>(this);
		CollisionBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	}

	CollisionBodySetup->AggGeom.BoxElems.Reset();

	const FInt3 MinBrickCoordinates = Coordinates << Grid->BricksPerCollisionChunkLog2;
	const FInt3 MaxBrickCoordinates = MinBrickCoordinates + Grid->BricksPerCollisionChunk - FInt3::Scalar(1);
	const FInt3 LocalBrickExpansion = FInt3::Scalar(1);
	const FInt3 MinLocalBrickCoordinates = MinBrickCoordinates - LocalBrickExpansion;

	// Read the brick materials for all the bricks that affect this chunk.
	const FInt3 LocalBricksDim = Grid->BricksPerCollisionChunk + LocalBrickExpansion * FInt3::Scalar(2);
	TArray<uint8> LocalBrickMaterials;
	LocalBrickMaterials.SetNumUninitialized(LocalBricksDim.X * LocalBricksDim.Y * LocalBricksDim.Z);
	Grid->GetBrickMaterialArray(MinLocalBrickCoordinates,MinLocalBrickCoordinates + LocalBricksDim - FInt3::Scalar(1),LocalBrickMaterials);

	// Iterate over each brick in the chunk.
	const int32 EmptyMaterialIndex = Grid->Parameters.EmptyMaterialIndex;
	for(int32 Y = MinBrickCoordinates.Y; Y <= MaxBrickCoordinates.Y; ++Y)
	{
		for(int32 X = MinBrickCoordinates.X; X <= MaxBrickCoordinates.X; ++X)
		{
			for(int32 Z = MinBrickCoordinates.Z; Z <= MaxBrickCoordinates.Z; ++Z)
			{
				const FInt3 BrickCoordinates(X,Y,Z);
				const FInt3 LocalBrickCoordinates = BrickCoordinates - MinLocalBrickCoordinates;
				// Only create collision boxes for bricks that aren't empty.
				const int32 BrickMaterial = LocalBrickMaterials[(LocalBrickCoordinates.Y * LocalBricksDim.X + LocalBrickCoordinates.X) * LocalBricksDim.Z + LocalBrickCoordinates.Z];
				if (BrickMaterial != EmptyMaterialIndex)
				{
					// Only create collision boxes for bricks that are adjacent to an empty brick.
					bool HasEmptyNeighbor = false;
					for(uint32 FaceIndex = 0;FaceIndex < 6;++FaceIndex)
					{
						const FInt3 FacingLocalBrickCoordinates = LocalBrickCoordinates + FaceNormals[FaceIndex];
						if(LocalBrickMaterials[(FacingLocalBrickCoordinates.Y * LocalBricksDim.X + FacingLocalBrickCoordinates.X) * LocalBricksDim.Z + FacingLocalBrickCoordinates.Z] == EmptyMaterialIndex)
						{
							HasEmptyNeighbor = true;
							break;
						}
					}
					if(HasEmptyNeighbor)
					{
						FKBoxElem& BoxElement = *new(CollisionBodySetup->AggGeom.BoxElems) FKBoxElem;

						// Physics bodies are uniformly scaled by the minimum component of the 3D scale.
						// Compute the non-uniform scaling components to apply to the box center/extent.
						const FVector AbsScale3D = ComponentToWorld.GetScale3D().GetAbs();
						const FVector NonUniformScale3D = AbsScale3D / AbsScale3D.GetMin();

						// Set the box center and size.
						BoxElement.Center = ((BrickCoordinates - MinBrickCoordinates).ToFloat() + FVector(0.5f)) * NonUniformScale3D;
						BoxElement.X = NonUniformScale3D.X;
						BoxElement.Y = NonUniformScale3D.Y;
						BoxElement.Z = NonUniformScale3D.Z;
					}
				}
			}
		}
	}

	// Recreate the physics state, which includes an Face of the CollisionBodySetup.
	RecreatePhysicsState();

	UE_LOG(LogStats,Log,TEXT("UBrickCollisionComponent::UpdateCollisionBody took %fms to create %u boxes"),1000.0f * float(FPlatformTime::Seconds() - StartTime),CollisionBodySetup->AggGeom.BoxElems.Num());
}
