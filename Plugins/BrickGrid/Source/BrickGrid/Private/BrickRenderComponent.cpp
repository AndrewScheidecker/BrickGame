// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickGridPluginPrivatePCH.h"
#include "BrickRenderComponent.h"
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
	uint8 AmbientOcclusionFactor;
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
		const FVector FaceNormal = FaceNormals[FaceIndex].ToFloat();
		const FVector ProjectedFaceTangentX = (UnprojectedTangentX - FaceNormal * (UnprojectedTangentX | FaceNormal)).SafeNormal();
		TangentXBuffer.Tangents.Add(FPackedNormal(ProjectedFaceTangentX));
		TangentZBuffer.Tangents.Add(FPackedNormal(FVector4(FaceNormal, FMath::Sign(UnprojectedTangentY | (FaceNormal ^ ProjectedFaceTangentX)))));

		// Initialize the vertex factory's stream components.
		DataType NewData;
		NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&VertexBuffer, FBrickVertex, X, VET_UByte4N);
		NewData.TextureCoordinates.Add(STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&VertexBuffer, FBrickVertex, X, VET_UByte4N));
		NewData.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&VertexBuffer, FBrickVertex, X, VET_UByte4N);
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

	FBrickChunkSceneProxy(UBrickRenderComponent* Component)
	: FPrimitiveSceneProxy(Component)
	{
		const double StartTime = FPlatformTime::Seconds();

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

		// Update the AO for this chunk's bricks (and those adjacent, for filtering).
		UBrickGridComponent* Grid = Component->Grid;
		const FInt3 MinBrickCoordinates = Component->Coordinates << Grid->BricksPerRenderChunkLog2;
		const FInt3 MaxBrickCoordinates = MinBrickCoordinates + Grid->BricksPerRenderChunk - FInt3::Scalar(1);
		const FInt3 MinAdjacentBrickCoordinates = MinBrickCoordinates - FInt3::Scalar(1);
		const FInt3 MaxAdjacentBrickCoordinates = MaxBrickCoordinates + FInt3::Scalar(1);
		Grid->UpdateAO(MinAdjacentBrickCoordinates,MaxAdjacentBrickCoordinates);

		// Read the brick materials and ambient occlusion for this chunk and adjacent bricks.
		const FInt3 AdjacentBricksDim = Grid->BricksPerRenderChunk + FInt3::Scalar(2);
		TArray<uint8> BrickAmbientFactors;
		TArray<uint8> BrickMaterials;
		BrickAmbientFactors.Init(AdjacentBricksDim.X * AdjacentBricksDim.Y * AdjacentBricksDim.Z);
		BrickMaterials.Init(AdjacentBricksDim.X * AdjacentBricksDim.Y * AdjacentBricksDim.Z);
		for(int32 LocalY = 0; LocalY < AdjacentBricksDim.Y; ++LocalY)
		{
			for(int32 LocalX = 0; LocalX < AdjacentBricksDim.X; ++LocalX)
			{
				for(int32 LocalZ = 0; LocalZ < AdjacentBricksDim.Z; ++LocalZ)
				{
					const FBrick Brick = Grid->GetBrick(MinAdjacentBrickCoordinates + FInt3(LocalX,LocalY,LocalZ));
					const uint32 LocalIndex = (LocalY * AdjacentBricksDim.X + LocalX) * AdjacentBricksDim.Z + LocalZ;
					BrickMaterials[LocalIndex] = Brick.MaterialIndex;
					BrickAmbientFactors[LocalIndex] = Brick.AmbientOcclusionFactor;
				}
			}
		}

		// Compute a filtered per-vertex ambient occlusion factor.
		const FInt3 VertexDim = Grid->BricksPerRenderChunk + FInt3::Scalar(1);
		TArray<uint8> VertexAmbientFactors;
		VertexAmbientFactors.Init(VertexDim.X * VertexDim.Y * VertexDim.Z);
		for(int32 VertexY = 0; VertexY < VertexDim.Y; ++VertexY)
		{
			for(int32 VertexX = 0; VertexX < VertexDim.X; ++VertexX)
			{
				for(int32 VertexZ = 0; VertexZ < VertexDim.Z; ++VertexZ)
				{
					uint32 AdjacentAmbientFactorSum = 0;
					for(uint32 AdjacentIndex = 0;AdjacentIndex < 8;++AdjacentIndex)
					{
						const uint32 LocalX = VertexX + ((AdjacentIndex >> 0) & 1);
						const uint32 LocalY = VertexY + ((AdjacentIndex >> 1) & 1);
						const uint32 LocalZ = VertexZ + (AdjacentIndex >> 2);
						const uint32 LocalIndex = (LocalY * AdjacentBricksDim.X + LocalX) * AdjacentBricksDim.Z + LocalZ;
						AdjacentAmbientFactorSum += BrickAmbientFactors[LocalIndex];
					}

					// Average the ambient factor from all 8 bricks adjacent to the vertex.
					// Normalize it with an implicit factor of 2 since we'll treat the result as a hemisphere percent.
					const uint8 AverageAdjacentAmbientFactor = (uint8)FMath::Min<uint32>(255,AdjacentAmbientFactorSum / 4);

					const uint32 VertexIndex = (VertexY * VertexDim.X + VertexX) * VertexDim.Z + VertexZ;
					VertexAmbientFactors[VertexIndex] = AverageAdjacentAmbientFactor;
				}
			}
		}

		// Iterate over each brick in the chunk.
		const int32 EmptyMaterialIndex = Grid->Parameters.EmptyMaterialIndex;
		for(int32 LocalY = 1; LocalY < Grid->BricksPerRenderChunk.Y + 1; ++LocalY)
		{
			for(int32 LocalX = 1; LocalX < Grid->BricksPerRenderChunk.X + 1; ++LocalX)
			{
				for(int32 LocalZ = 1; LocalZ < Grid->BricksPerRenderChunk.Z + 1; ++LocalZ)
				{
					// Only draw faces of bricks that aren't empty.
					const uint32 LocalIndex = (LocalY * AdjacentBricksDim.X + LocalX) * AdjacentBricksDim.Z + LocalZ;
					const uint8 BrickMaterial = BrickMaterials[LocalIndex];
					if (BrickMaterial != EmptyMaterialIndex)
					{
						const FInt3 RelativeBrickCoordinates(LocalX - 1,LocalY - 1,LocalZ - 1);
						for(uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
						{
							// Only draw faces that face empty bricks.
							const int32 FacingLocalX = LocalX + FaceNormals[FaceIndex].X;
							const int32 FacingLocalY = LocalY + FaceNormals[FaceIndex].Y;
							const int32 FacingLocalZ = LocalZ + FaceNormals[FaceIndex].Z;
							const uint32 FacingLocalIndex = (FacingLocalY * AdjacentBricksDim.X + FacingLocalX) * AdjacentBricksDim.Z + FacingLocalZ;
							const uint32 FrontBrickMaterial = BrickMaterials[FacingLocalIndex];
							if(FrontBrickMaterial == EmptyMaterialIndex)
							{
								// Write the vertices for the brick face.
								const int32 BaseFaceVertexIndex = VertexBuffer.Vertices.AddUninitialized(4);
								FBrickVertex* FaceVertex = &VertexBuffer.Vertices[BaseFaceVertexIndex];
								for (uint32 FaceVertexIndex = 0; FaceVertexIndex < 4; ++FaceVertexIndex, ++FaceVertex)
								{
									const FInt3 CornerVertexOffset = GetCornerVertexOffset(FaceVertices[FaceIndex][FaceVertexIndex]);
									const FInt3 Position = RelativeBrickCoordinates + CornerVertexOffset;
									FaceVertex->X = Position.X;
									FaceVertex->Y = Position.Y;
									FaceVertex->Z = Position.Z;
									FaceVertex->AmbientOcclusionFactor = VertexAmbientFactors[(Position.Y * VertexDim.X + Position.X) * VertexDim.Z + Position.Z];
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

		UE_LOG(LogStats,Log,TEXT("FBrickChunkSceneProxy constructor took %fms to create %u indices and %u vertices"),1000.0f * float(FPlatformTime::Seconds() - StartTime),IndexBuffer.Indices.Num(),VertexBuffer.Vertices.Num());
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

UBrickRenderComponent::UBrickRenderComponent( const FPostConstructInitializeProperties& PCIP )
	: Super( PCIP )
{
	PrimaryComponentTick.bCanEverTick = false;
	CastShadow = true;
	bUseAsOccluder = true;
	bCanEverAffectNavigation = true;	
	bAutoRegister = false;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

FPrimitiveSceneProxy* UBrickRenderComponent::CreateSceneProxy()
{
	return new FBrickChunkSceneProxy(this);
}

int32 UBrickRenderComponent::GetNumMaterials() const
{
	return Grid->Parameters.Materials.Num();
}

class UMaterialInterface* UBrickRenderComponent::GetMaterial(int32 ElementIndex) const
{
	return Grid != NULL && ElementIndex >= 0 && ElementIndex < Grid->Parameters.Materials.Num() 
		? Grid->Parameters.Materials[ElementIndex].SurfaceMaterial
		: NULL;
}

FBoxSphereBounds UBrickRenderComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	FBoxSphereBounds NewBounds;
	NewBounds.Origin = NewBounds.BoxExtent = Grid->BricksPerRenderChunk.ToFloat() / 2.0f;
	NewBounds.SphereRadius = NewBounds.BoxExtent.Size();
	return NewBounds.TransformBy(LocalToWorld);
}
