// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickGridPluginPrivatePCH.h"
#include "BrickChunkComponent.h"
#include "BrickGridComponent.h"

// Maps brick corner indices to 3D coordinates.
static FInt3 GetCornerVertexOffset(uint8 BrickVertexIndex)
{
	return (FInt3::Scalar(BrickVertexIndex) >> FInt3(2,1,0)) & FInt3::Scalar(1);
}
// Maps face index and face vertex index to brick corner indices.
static const uint8 FaceVertices[6][4] =
{
	{ 2, 3, 1, 0 },		// -X
	{ 4, 5, 7, 6 },		// +X
	{ 0, 1, 5, 4 },		// -Y
	{ 6, 7, 3, 2 },		// +Y
	{ 4, 6, 2, 0 },	// -Z
	{ 1, 3, 7, 5 }		// +Z
};
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

/**	An element of the vertex buffer given to the GPU by the CPU brick tessellator.
	8-bit coordinates are used for efficiency. */
struct FBrickVertex
{
	uint8 X;
	uint8 Y;
	uint8 Z;
	uint8 Padding0;
};

/** Vertex Buffer */
class FBrickChunkVertexBuffer : public FVertexBuffer 
{
public:
	TArray<FBrickVertex> Vertices;
	virtual void InitRHI()
	{
		if (Vertices.Num() > 0)
		{
			VertexBufferRHI = RHICreateVertexBuffer(Vertices.Num() * sizeof(FBrickVertex), NULL, BUF_Dynamic);
			// Copy the vertex data into the vertex buffer.
			void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, 0, Vertices.Num() * sizeof(FBrickVertex), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, Vertices.GetTypedData(), Vertices.Num() * sizeof(FBrickVertex));
			RHIUnlockVertexBuffer(VertexBufferRHI);
		}
	}
};

/** Index Buffer */
class FBrickChunkIndexBuffer : public FIndexBuffer 
{
public:
	TArray<int16> Indices;
	virtual void InitRHI()
	{
		if (Indices.Num() > 0)
		{
			IndexBufferRHI = RHICreateIndexBuffer(sizeof(int16), Indices.Num() * sizeof(int16), NULL, BUF_Static);
			// Write the indices to the index buffer.
			void* Buffer = RHILockIndexBuffer(IndexBufferRHI, 0, Indices.Num() * sizeof(int16), RLM_WriteOnly);
			FMemory::Memcpy(Buffer, Indices.GetTypedData(), Indices.Num() * sizeof(int16));
			RHIUnlockIndexBuffer(IndexBufferRHI);
		}
	}
};

/** Tangent Buffer */
class FBrickChunkTangentBuffer : public FVertexBuffer
{
public:
	TArray<FPackedNormal> Tangents;
	virtual void InitRHI()
	{
		if (Tangents.Num() > 0)
		{
			VertexBufferRHI = RHICreateVertexBuffer(Tangents.Num() * sizeof(FPackedNormal), NULL, BUF_Dynamic);
			// Copy the vertex data into the vertex buffer.
			void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, 0, Tangents.Num() * sizeof(FPackedNormal), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, Tangents.GetTypedData(), Tangents.Num() * sizeof(FPackedNormal));
			RHIUnlockVertexBuffer(VertexBufferRHI);
		}
	}
};
/** Vertex Factory */
class FBrickChunkVertexFactory : public FLocalVertexFactory
{
public:

	void Init(const FBrickChunkVertexBuffer& VertexBuffer,uint32 FaceIndex)
	{
		const FVector UnprojectedTangentX = FVector(+1,-1,0).SafeNormal();
		const FVector UnprojectedTangentY(-1,-1,-1);
		const FVector FaceNormal = FaceNormals[FaceIndex];
		const FVector ProjectedFaceTangentX = (UnprojectedTangentX - FaceNormal * (UnprojectedTangentX | FaceNormal)).SafeNormal();
		TangentXBuffer.Tangents.Add(FPackedNormal(ProjectedFaceTangentX));
		TangentZBuffer.Tangents.Add(FPackedNormal(FVector4(FaceNormal, FMath::Sign(UnprojectedTangentY | (FaceNormal ^ ProjectedFaceTangentX)))));

		// Initialize the vertex factory's stream components.
		DataType NewData;
		NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&VertexBuffer, FBrickVertex, X, VET_UByte4N);
		NewData.TextureCoordinates.Add(STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&VertexBuffer, FBrickVertex, X, VET_UByte4N));
		// Use a stride of 0 to use the same TangentX/TangentZ for all faces using this vertex factory.
		NewData.TangentBasisComponents[0] = FVertexStreamComponent(&TangentXBuffer,0,0,VET_PackedNormal);
		NewData.TangentBasisComponents[1] = FVertexStreamComponent(&TangentZBuffer,0,0,VET_PackedNormal);
		check(!IsInRenderingThread());
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitBrickChunkVertexFactory,
			FBrickChunkVertexFactory*,VertexFactory,this,
			DataType,NewData,NewData,
		{
			VertexFactory->SetData(NewData);
		});
	}

	virtual void InitResource()
	{
		TangentXBuffer.InitResource();
		TangentZBuffer.InitResource();
		FLocalVertexFactory::InitResource();
	}
	virtual void ReleaseResource()
	{
		TangentXBuffer.ReleaseResource();
		TangentZBuffer.ReleaseResource();
		FLocalVertexFactory::ReleaseResource();
	}

private:

	FBrickChunkTangentBuffer TangentXBuffer;
	FBrickChunkTangentBuffer TangentZBuffer;
};

/** Scene proxy */
class FBrickChunkSceneProxy : public FPrimitiveSceneProxy
{
public:

	FBrickChunkSceneProxy(UBrickChunkComponent* Component)
	: FPrimitiveSceneProxy(Component)
	{
		// Batch the faces by material and direction.
		struct FFaceBatch
		{
			TArray<int16> Indices;
		};
		struct FMaterialBatch
		{
			FFaceBatch FaceBatches[6];
		};
		TArray<FMaterialBatch> MaterialBatches;
		MaterialBatches.Init(FMaterialBatch(),Component->Grid->Parameters.Materials.Num());

		// Iterate over each brick in the chunk.
		const UBrickGridComponent* Grid = Component->Grid;
		const FInt3 BricksPerChunk = Grid->BricksPerChunk;
		const FInt3 MinBrickCoordinates = Component->Coordinates << Grid->Parameters.BricksPerChunkLog2;
		const FInt3 MaxBrickCoordinatesPlus1 = MinBrickCoordinates + BricksPerChunk + FInt3::Scalar(1);
		const int32 EmptyMaterialIndex = Grid->Parameters.EmptyMaterialIndex;
		for(int32 Y = MinBrickCoordinates.Y; Y < MaxBrickCoordinatesPlus1.Y; ++Y)
		{
			for(int32 X = MinBrickCoordinates.X; X < MaxBrickCoordinatesPlus1.X; ++X)
			{
				for(int32 Z = MinBrickCoordinates.Z; Z < MaxBrickCoordinatesPlus1.Z; ++Z)
				{
					// Only draw faces of bricks that aren't empty.
					const FInt3 BrickCoordinates(X,Y,Z);
					const uint32 BrickMaterial = Grid->GetBrick(BrickCoordinates);
					if (BrickMaterial != EmptyMaterialIndex)
					{
						for(uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
						{
							// Only draw faces that face empty bricks.
							const FInt3 FrontBrickXYZ = BrickCoordinates + FaceNormals[FaceIndex];
							if (Grid->GetBrick(FrontBrickXYZ) == EmptyMaterialIndex)
							{
								// Write the vertices for the brick face.
								const int32 BaseFaceVertexIndex = VertexBuffer.Vertices.AddUninitialized(4);
								FBrickVertex* FaceVertex = &VertexBuffer.Vertices[BaseFaceVertexIndex];
								const FInt3 RelativeBrickCoordinates = BrickCoordinates - MinBrickCoordinates;
								for (uint32 FaceVertexIndex = 0; FaceVertexIndex < 4; ++FaceVertexIndex, ++FaceVertex)
								{
									const FInt3 Position = RelativeBrickCoordinates + GetCornerVertexOffset(FaceVertices[FaceIndex][FaceVertexIndex]);
									FaceVertex->X = Position.X;
									FaceVertex->Y = Position.Y;
									FaceVertex->Z = Position.Z;
								}

								// Write the indices for the brick face.
								FFaceBatch& FaceBatch = MaterialBatches[BrickMaterial].FaceBatches[FaceIndex];
								int16* FaceVertexIndex = &FaceBatch.Indices[FaceBatch.Indices.AddUninitialized(6)];
								*FaceVertexIndex++ = BaseFaceVertexIndex + 0;
								*FaceVertexIndex++ = BaseFaceVertexIndex + 1;
								*FaceVertexIndex++ = BaseFaceVertexIndex + 2;
								*FaceVertexIndex++ = BaseFaceVertexIndex + 0;
								*FaceVertexIndex++ = BaseFaceVertexIndex + 2;
								*FaceVertexIndex++ = BaseFaceVertexIndex + 3;
							}
						}
					}
				}
			}
		}

		// Create mesh elements for each batch.
		int32 NumIndices = 0;
		for(int32 MaterialIndex = 0; MaterialIndex < MaterialBatches.Num(); ++MaterialIndex)
		{
			for(uint32 FaceIndex = 0;FaceIndex < 6;++FaceIndex)
			{
				NumIndices += MaterialBatches[MaterialIndex].FaceBatches[FaceIndex].Indices.Num();
			}
		}
		IndexBuffer.Indices.Empty(NumIndices);
		for(int32 MaterialIndex = 0; MaterialIndex < MaterialBatches.Num(); ++MaterialIndex)
		{
			for(uint32 FaceIndex = 0;FaceIndex < 6;++FaceIndex)
			{
				const FFaceBatch& FaceBatch = MaterialBatches[MaterialIndex].FaceBatches[FaceIndex];
				if (FaceBatch.Indices.Num() > 0)
				{
					FElement& Element = *new(Elements)FElement;
					Element.FirstIndex = IndexBuffer.Indices.Num();
					Element.NumPrimitives = FaceBatch.Indices.Num() / 3;
					Element.MaterialIndex = MaterialIndex;
					Element.FaceIndex = FaceIndex;

					// Append the batch's indices to the index buffer.
					IndexBuffer.Indices.Append(FaceBatch.Indices);
				}
			}
		}

		// Copy the materials.
		for(int32 MaterialIndex = 0; MaterialIndex < Component->GetNumMaterials(); ++MaterialIndex)
		{
			UMaterialInterface* Material = Component->GetMaterial(MaterialIndex);
			if (Material == NULL)
			{
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}
			MaterialRelevance |= Material->GetRelevance_Concurrent();
			Materials.Add(Material);
		}

		// Enqueue initialization of render resource
		BeginInitResource(&VertexBuffer);
		BeginInitResource(&IndexBuffer);
		for(uint32 FaceIndex = 0;FaceIndex < 6;++FaceIndex)
		{
			VertexFactories[FaceIndex].Init(VertexBuffer,FaceIndex);
			BeginInitResource(&VertexFactories[FaceIndex]);
		}
	}

	virtual ~FBrickChunkSceneProxy()
	{
		VertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		for(uint32 FaceIndex = 0;FaceIndex < 6;++FaceIndex)
		{
			VertexFactories[FaceIndex].ReleaseResource();
		}
	}

	virtual void OnTransformChanged() OVERRIDE
	{
		// Create a uniform buffer with the transform for the chunk.
		PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(FScaleMatrix(FVector(255, 255, 255)) * GetLocalToWorld(), GetBounds(), GetLocalBounds(), true);
	}

	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View) OVERRIDE
	{
		// Set up the wireframe material Face.
		FColoredMaterialRenderProxy WireframeMaterialFace(
			WITH_EDITOR ? GEngine->WireframeMaterial->GetRenderProxy(IsSelected()) : NULL,
			FLinearColor(0, 0.5f, 1.f)
			);

		// Draw the mesh elements.
		for(int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
		{
			PDI->DrawMesh(GetMeshBatch(ElementIndex, View->Family->EngineShowFlags.Wireframe ? &WireframeMaterialFace : NULL));
		}
	}

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) OVERRIDE
	{
		for(int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
		{
			PDI->DrawMesh(GetMeshBatch(ElementIndex,NULL),0,FLT_MAX);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) OVERRIDE
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = View->Family->EngineShowFlags.Wireframe || IsSelected();
		Result.bStaticRelevance = !Result.bDynamicRelevance;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual bool CanBeOccluded() const OVERRIDE
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint(void) const { return(sizeof(*this) + GetAllocatedSize()); }
	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:

	FBrickChunkVertexBuffer VertexBuffer;
	FBrickChunkIndexBuffer IndexBuffer;
	FBrickChunkVertexFactory VertexFactories[6];

	struct FElement
	{
		uint32 FirstIndex;
		uint32 NumPrimitives;
		uint32 MaterialIndex;
		uint32 FaceIndex;
	};
	TArray<FElement> Elements;

	TArray<UMaterialInterface*> Materials;
	FMaterialRelevance MaterialRelevance;

	TUniformBufferRef<FPrimitiveUniformShaderParameters> PrimitiveUniformBuffer;

	FMeshBatch GetMeshBatch(int32 ElementIndex,FMaterialRenderProxy* WireframeMaterialFace)
	{
		const FElement& Element = Elements[ElementIndex];
		FMeshBatch Mesh;
		Mesh.bWireframe = WireframeMaterialFace != NULL;
		Mesh.VertexFactory = &VertexFactories[Element.FaceIndex];
		Mesh.MaterialRenderProxy = WireframeMaterialFace ? WireframeMaterialFace : Materials[Element.MaterialIndex]->GetRenderProxy(IsSelected());
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = SDPG_World;
		Mesh.CastShadow = true;
		Mesh.Elements[0].FirstIndex = Element.FirstIndex;
		Mesh.Elements[0].NumPrimitives = Element.NumPrimitives;
		Mesh.Elements[0].MinVertexIndex = IndexBuffer.Indices[Element.FirstIndex];
		Mesh.Elements[0].MaxVertexIndex = IndexBuffer.Indices[Element.FirstIndex + Element.NumPrimitives * 3 - 1];
		Mesh.Elements[0].IndexBuffer = &IndexBuffer;
		Mesh.Elements[0].PrimitiveUniformBuffer = PrimitiveUniformBuffer;
		return Mesh;
	}
};

UBrickChunkComponent::UBrickChunkComponent( const FPostConstructInitializeProperties& PCIP )
	: Super( PCIP )
{
	PrimaryComponentTick.bCanEverTick = false;
	CastShadow = true;
	bUseAsOccluder = true;
	bCanEverAffectNavigation = true;	

	CollisionBodySetup = ConstructObject<UBodySetup>(UBodySetup::StaticClass(), this);
	CollisionBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
}

FPrimitiveSceneProxy* UBrickChunkComponent::CreateSceneProxy()
{
	// Update the collision body if the rendering proxy has been recreated for any reason, since the causes mostly also invalidate the collision body.
	UpdateCollisionBody();

	return new FBrickChunkSceneProxy(this);
}

int32 UBrickChunkComponent::GetNumMaterials() const
{
	return Grid->Parameters.Materials.Num();
}

class UMaterialInterface* UBrickChunkComponent::GetMaterial(int32 ElementIndex) const
{
	return Grid != NULL && ElementIndex >= 0 && ElementIndex < Grid->Parameters.Materials.Num() 
		? Grid->Parameters.Materials[ElementIndex].SurfaceMaterial
		: NULL;
}

FBoxSphereBounds UBrickChunkComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	FBoxSphereBounds NewBounds;
	NewBounds.Origin = NewBounds.BoxExtent = FVector(Grid->BricksPerChunk.X,Grid->BricksPerChunk.Y,Grid->BricksPerChunk.Z) / 2.0f;
	NewBounds.SphereRadius = NewBounds.BoxExtent.Size();
	return NewBounds.TransformBy(LocalToWorld);
}

class UBodySetup* UBrickChunkComponent::GetBodySetup()
{
	if (!CollisionBodySetup)
	{
		UpdateCollisionBody();
	}
	return CollisionBodySetup;
}

void UBrickChunkComponent::UpdateCollisionBody()
{
	CollisionBodySetup->AggGeom.BoxElems.Reset();

	// Iterate over each brick in the chunk.
	const FInt3 BricksPerChunk = Grid->BricksPerChunk;
	const FInt3 MinBrickCoordinates = Coordinates << Grid->Parameters.BricksPerChunkLog2;
	const FInt3 MaxBrickCoordinatesPlus1 = MinBrickCoordinates + BricksPerChunk + FInt3::Scalar(1);
	const int32 EmptyMaterialIndex = Grid->Parameters.EmptyMaterialIndex;
	for(int32 Y = MinBrickCoordinates.Y; Y < MaxBrickCoordinatesPlus1.Y; ++Y)
	{
		for(int32 X = MinBrickCoordinates.X; X < MaxBrickCoordinatesPlus1.X; ++X)
		{
			for(int32 Z = MinBrickCoordinates.Z; Z < MaxBrickCoordinatesPlus1.Z; ++Z)
			{
				// Only create collision boxes for bricks that aren't empty.
				const FInt3 BrickCoordinates(X,Y,Z);
				const int32 BrickMaterial = Grid->GetBrick(BrickCoordinates);
				if (BrickMaterial != EmptyMaterialIndex)
				{
					// Only create collision boxes for bricks that are adjacent to an empty brick.
					bool HasEmptyNeighbor = false;
					for(uint32 FaceIndex = 0;FaceIndex < 6;++FaceIndex)
					{
						if(Grid->GetBrick(BrickCoordinates + FaceNormals[FaceIndex]) == EmptyMaterialIndex)
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
						BoxElement.Center = ((FVector)(BrickCoordinates - MinBrickCoordinates) + FVector(0.5f)) * NonUniformScale3D;
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
}
