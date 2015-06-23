// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickGridPluginPrivatePCH.h"
#include "BrickRenderComponent.h"
#include "BrickGridComponent.h"
#include "BrickAmbientOcclusion.inl"

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

	FBrickVertex() {}
	FBrickVertex(FInt3 InCoordinates,uint8 InAmbientOcclusionFactor)
	: X(InCoordinates.X), Y(InCoordinates.Y), Z(InCoordinates.Z), AmbientOcclusionFactor(InAmbientOcclusionFactor)
	{}
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
			FRHIResourceCreateInfo CreateInfo;
			VertexBufferRHI = RHICreateVertexBuffer(Vertices.Num() * sizeof(FBrickVertex), BUF_Dynamic, CreateInfo);
			// Copy the vertex data into the vertex buffer.
			void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, 0, Vertices.Num() * sizeof(FBrickVertex), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, Vertices.GetData(), Vertices.Num() * sizeof(FBrickVertex));
			RHIUnlockVertexBuffer(VertexBufferRHI);
		}
	}
};

/** Index Buffer */
class FBrickChunkIndexBuffer : public FIndexBuffer 
{
public:
	TArray<uint16> Indices;
	virtual void InitRHI()
	{
		if (Indices.Num() > 0)
		{
			FRHIResourceCreateInfo CreateInfo;
			IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), Indices.Num() * sizeof(uint16), BUF_Static, CreateInfo);
			// Write the indices to the index buffer.
			void* Buffer = RHILockIndexBuffer(IndexBufferRHI, 0, Indices.Num() * sizeof(uint16), RLM_WriteOnly);
			FMemory::Memcpy(Buffer, Indices.GetData(), Indices.Num() * sizeof(uint16));
			RHIUnlockIndexBuffer(IndexBufferRHI);
		}
	}
};

/** Tangent Buffer */
class FBrickChunkTangentBuffer : public FVertexBuffer
{
public:
	virtual void InitRHI()
	{
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateVertexBuffer(12 * sizeof(FPackedNormal), BUF_Dynamic, CreateInfo);
		// Copy the vertex data into the vertex buffer.
		FPackedNormal* TangentBufferData = (FPackedNormal*)RHILockVertexBuffer(VertexBufferRHI, 0, 12 * sizeof(FPackedNormal), RLM_WriteOnly);
		for(int32 FaceIndex = 0;FaceIndex < 6;++FaceIndex)
		{
			const FVector UnprojectedTangentX = FVector(+1,-1,0).GetSafeNormal();
			const FVector UnprojectedTangentY(-1,-1,-1);
			const FVector FaceNormal = FaceNormals[FaceIndex].ToFloat();
			const FVector ProjectedFaceTangentX = (UnprojectedTangentX - FaceNormal * (UnprojectedTangentX | FaceNormal)).GetSafeNormal();
			*TangentBufferData++ = ProjectedFaceTangentX;
			*TangentBufferData++ = FVector4(FaceNormal, FMath::Sign(UnprojectedTangentY | (FaceNormal ^ ProjectedFaceTangentX)));
		}
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}
};

TGlobalResource<FBrickChunkTangentBuffer> TangentBuffer;

/** Vertex Factory */
class FBrickChunkVertexFactory : public FLocalVertexFactory
{
public:

	void Init(const FBrickChunkVertexBuffer& VertexBuffer,const FPrimitiveSceneProxy* InPrimitiveSceneProxy,uint8 InFaceIndex)
	{
		PrimitiveSceneProxy = InPrimitiveSceneProxy;
		FaceIndex = InFaceIndex;

		// Initialize the vertex factory's stream components.
		DataType NewData;
		NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&VertexBuffer, FBrickVertex, X, VET_UByte4N);
		NewData.TextureCoordinates.Add(STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&VertexBuffer, FBrickVertex, X, VET_UByte4N));
		NewData.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&VertexBuffer, FBrickVertex, X, VET_Color);
		// Use a stride of 0 to use the same TangentX/TangentZ for all faces using this vertex factory.
		NewData.TangentBasisComponents[0] = FVertexStreamComponent(&TangentBuffer,sizeof(FPackedNormal) * (2 * FaceIndex + 0),0,VET_PackedNormal);
		NewData.TangentBasisComponents[1] = FVertexStreamComponent(&TangentBuffer,sizeof(FPackedNormal) * (2 * FaceIndex + 1),0,VET_PackedNormal);
		check(!IsInRenderingThread());
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitBrickChunkVertexFactory,
			FBrickChunkVertexFactory*,VertexFactory,this,
			DataType,NewData,NewData,
		{
			VertexFactory->SetData(NewData);
		});
	}

	#if UE4_HAS_IMPROVED_MESHBATCH_ELEMENT_VISIBILITY
		virtual uint64 GetStaticBatchElementVisibility(const class FSceneView& View, const struct FMeshBatch* Batch) const override
		{
			return IsStaticBatchVisible(View.ViewMatrices.ViewOrigin,Batch) ? 1 : 0;
		}
		virtual uint64 GetStaticBatchElementShadowVisibility(const class FSceneView& View, const FLightSceneProxy* LightSceneProxy, const struct FMeshBatch* Batch) const override
		{
			return IsStaticBatchVisible(LightSceneProxy->GetPosition(),Batch) ? 1 : 0;
		}
	#endif

private:

	const FPrimitiveSceneProxy* PrimitiveSceneProxy;
	uint8 FaceIndex;

	bool IsStaticBatchVisible(const FVector4& ViewPosition,const struct FMeshBatch* Batch) const
	{
		const uint8 FaceIndex = Batch->Elements[0].UserIndex;
		const FBox BoundingBox = PrimitiveSceneProxy->GetBounds().GetBox();
		const FVector MinRelativePosition = ViewPosition - BoundingBox.Min * ViewPosition.W;
		const FVector MaxRelativePosition = ViewPosition - BoundingBox.Max * ViewPosition.W;
		switch(FaceIndex)
		{
		case 0:	return MaxRelativePosition.X < 0.0f;
		case 1:	return MinRelativePosition.X > 0.0f;
		case 2:	return MaxRelativePosition.Y < 0.0f;
		case 3:	return MinRelativePosition.Y > 0.0f;
		case 4:	return MaxRelativePosition.Z < 0.0f;
		case 5:	return MinRelativePosition.Z > 0.0f;
		default: return false;
		}
	}
};

/** Scene proxy */
class FBrickChunkSceneProxy : public FPrimitiveSceneProxy
{
public:

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

	FGraphEventRef SetupCompletionEvent;

	TArray<uint8> LocalBrickMaterials;

	FBrickChunkSceneProxy(UBrickRenderComponent* Component,const TArray<uint8>&& InLocalBrickMaterials)
	: FPrimitiveSceneProxy(Component)
	, LocalBrickMaterials(InLocalBrickMaterials)
	{}

	void BeginInitResources()
	{
		// Enqueue initialization of render resource
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(SetupCompletionFence,FGraphEventRef,SetupCompletionEvent,SetupCompletionEvent,
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(SetupCompletionEvent,ENamedThreads::RenderThread);
		});
		BeginInitResource(&VertexBuffer);
		BeginInitResource(&IndexBuffer);
		for(uint32 FaceIndex = 0;FaceIndex < 6;++FaceIndex)
		{
			VertexFactories[FaceIndex].Init(VertexBuffer,this,FaceIndex);
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

	virtual void OnTransformChanged() override
	{
		// Create a uniform buffer with the transform for the chunk.
		PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(FScaleMatrix(FVector(255, 255, 255)) * GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,const FSceneViewFamily& ViewFamily,uint32 VisibilityMap,class FMeshElementCollector& Collector) const override
	{
		// Set up the wireframe material Face.
		auto WireframeMaterialFace = new FColoredMaterialRenderProxy(
			WITH_EDITOR ? GEngine->WireframeMaterial->GetRenderProxy(IsSelected()) : NULL,
			FLinearColor(0, 0.5f, 1.f)
			);
		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialFace);

		// Draw the mesh elements in each view they are visible.
		for(int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
		{
			FMeshBatch& Batch = Collector.AllocateMesh();
			InitMeshBatch(Batch,ElementIndex,ViewFamily.EngineShowFlags.Wireframe ? WireframeMaterialFace : NULL);

			for(int32 ViewIndex = 0;ViewIndex < Views.Num();++ViewIndex)
			{
				if(VisibilityMap & (1 << ViewIndex))
				{
					Collector.AddMesh(ViewIndex,Batch);
				}
			}
		}
	}

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override
	{
		for(int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
		{
			FMeshBatch Batch;
			InitMeshBatch(Batch,ElementIndex,NULL);
			PDI->DrawMesh(Batch,FLT_MAX);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = View->Family->EngineShowFlags.Wireframe || IsSelected();
		Result.bStaticRelevance = !Result.bDynamicRelevance;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint(void) const { return(sizeof(*this) + GetAllocatedSize()); }
	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	void InitMeshBatch(FMeshBatch& OutBatch,int32 ElementIndex,FMaterialRenderProxy* WireframeMaterialFace) const
	{
		const FElement& Element = Elements[ElementIndex];
		OutBatch.bWireframe = WireframeMaterialFace != NULL;
		OutBatch.VertexFactory = &VertexFactories[Element.FaceIndex];
		OutBatch.MaterialRenderProxy = WireframeMaterialFace ? WireframeMaterialFace : Materials[Element.MaterialIndex]->GetRenderProxy(IsSelected());
		OutBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
		OutBatch.Type = PT_TriangleList;
		OutBatch.DepthPriorityGroup = SDPG_World;
		OutBatch.CastShadow = true;
		#if UE4_HAS_IMPROVED_MESHBATCH_ELEMENT_VISIBILITY
			OutBatch.bRequiresPerElementVisibility = true;
		#endif
		OutBatch.Elements[0].FirstIndex = Element.FirstIndex;
		OutBatch.Elements[0].NumPrimitives = Element.NumPrimitives;
		OutBatch.Elements[0].MinVertexIndex = 0;
		OutBatch.Elements[0].MaxVertexIndex = VertexBuffer.Vertices.Num() - 1;
		OutBatch.Elements[0].IndexBuffer = &IndexBuffer;
		OutBatch.Elements[0].PrimitiveUniformBuffer = PrimitiveUniformBuffer;
		OutBatch.Elements[0].UserIndex = Element.FaceIndex;
	}
};

UBrickRenderComponent::UBrickRenderComponent( const FObjectInitializer& Initializer )
	: Super( Initializer )
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
	const double StartTime = FPlatformTime::Seconds();

	HasLowPriorityUpdatePending = false;

	const FInt3 MinBrickCoordinates = Coordinates << Grid->BricksPerRenderChunkLog2;
	const FInt3 LocalBrickExpansion(Grid->Parameters.AmbientOcclusionBlurRadius + 1,Grid->Parameters.AmbientOcclusionBlurRadius + 1,1);
	const FInt3 MinLocalBrickCoordinates = MinBrickCoordinates - LocalBrickExpansion;

	// Read the brick materials for all the bricks that affect this chunk.
	const FInt3 LocalBricksDim = Grid->BricksPerRenderChunk + LocalBrickExpansion * FInt3::Scalar(2);
	TArray<uint8> LocalBrickMaterialsGameThread;
	LocalBrickMaterialsGameThread.SetNumUninitialized(LocalBricksDim.X * LocalBricksDim.Y * LocalBricksDim.Z);
	Grid->GetBrickMaterialArray(MinLocalBrickCoordinates,MinLocalBrickCoordinates + LocalBricksDim - FInt3::Scalar(1),LocalBrickMaterialsGameThread);

	// Check whether there are any non-empty bricks in this chunk.
	bool HasNonEmptyBrick = false;
	const int32 EmptyMaterialIndex = Grid->Parameters.EmptyMaterialIndex;
	for(int32 LocalBrickY = LocalBrickExpansion.Y; LocalBrickY < Grid->BricksPerRenderChunk.Y + LocalBrickExpansion.Y && !HasNonEmptyBrick; ++LocalBrickY)
	{
		for(int32 LocalBrickX = LocalBrickExpansion.X; LocalBrickX < Grid->BricksPerRenderChunk.X + LocalBrickExpansion.X && !HasNonEmptyBrick; ++LocalBrickX)
		{
			for(int32 LocalBrickZ = LocalBrickExpansion.Z; LocalBrickZ < Grid->BricksPerRenderChunk.Z + LocalBrickExpansion.Z && !HasNonEmptyBrick; ++LocalBrickZ)
			{
				const uint32 LocalBrickIndex = (LocalBrickY * LocalBricksDim.X + LocalBrickX) * LocalBricksDim.Z + LocalBrickZ;
				if(LocalBrickMaterialsGameThread[LocalBrickIndex] != EmptyMaterialIndex)
				{
					HasNonEmptyBrick = true;
				}
			}
		}
	}

	// Only create a scene proxy if there are some non-empty bricks in the chunk.
	FBrickChunkSceneProxy* SceneProxy = NULL;
	if(HasNonEmptyBrick)
	{
		SceneProxy = new FBrickChunkSceneProxy(this,MoveTemp(LocalBrickMaterialsGameThread));
		SceneProxy->SetupCompletionEvent = FFunctionGraphTask::CreateAndDispatchWhenReady([=]()
		{
			const double StartTime = FPlatformTime::Seconds();
			const TArray<uint8>& LocalBrickMaterials = SceneProxy->LocalBrickMaterials;
		
			struct FFaceBatch
			{
				TArray<uint16> Indices;
			};
			struct FMaterialBatch
			{
				FFaceBatch FaceBatches[6];
			};
			TArray<FMaterialBatch> MaterialBatches;
			MaterialBatches.Init(FMaterialBatch(),Grid->Parameters.Materials.Num());

			// Compute the ambient occlusion for the vertices in this chunk.
			const FInt3 LocalVertexDim = Grid->BricksPerRenderChunk + FInt3::Scalar(1);
			TArray<uint8> LocalVertexAmbientFactors;
			LocalVertexAmbientFactors.SetNumUninitialized(LocalVertexDim.X * LocalVertexDim.Y * LocalVertexDim.Z);
			ComputeChunkAO(Grid,MinLocalBrickCoordinates,LocalBrickExpansion,LocalBricksDim,LocalVertexDim,LocalBrickMaterials,LocalVertexAmbientFactors);

			// Create an array of the vertices needed to render this chunk, along with a map from 3D coordinates to indices.
			TArray<uint16> VertexIndexMap;
			VertexIndexMap.Empty(LocalVertexDim.X * LocalVertexDim.Y * LocalVertexDim.Z);
			for(int32 LocalVertexY = 0; LocalVertexY < LocalVertexDim.Y; ++LocalVertexY)
			{
				for(int32 LocalVertexX = 0; LocalVertexX < LocalVertexDim.X; ++LocalVertexX)
				{
					for(int32 LocalVertexZ = 0; LocalVertexZ < LocalVertexDim.Z; ++LocalVertexZ)
					{
						const FInt3 LocalVertexCoordinates(LocalVertexX,LocalVertexY,LocalVertexZ);

						bool HasEmptyAdjacentBrick = false;
						bool HasNonEmptyAdjacentBrick = false;
						for(uint32 AdjacentBrickIndex = 0;AdjacentBrickIndex < 8;++AdjacentBrickIndex)
						{
							const FInt3 LocalBrickCoordinates = LocalVertexCoordinates + GetCornerVertexOffset(AdjacentBrickIndex) + LocalBrickExpansion - FInt3::Scalar(1);
							const uint32 LocalBrickIndex = (LocalBrickCoordinates.Y * LocalBricksDim.X + LocalBrickCoordinates.X) * LocalBricksDim.Z + LocalBrickCoordinates.Z;
							if(LocalBrickMaterials[LocalBrickIndex] == EmptyMaterialIndex)
							{
								HasEmptyAdjacentBrick = true;
							}
							else
							{
								HasNonEmptyAdjacentBrick = true;
							}
						}

						if(HasEmptyAdjacentBrick && HasNonEmptyAdjacentBrick)
						{
							VertexIndexMap.Add(SceneProxy->VertexBuffer.Vertices.Num());
							new(SceneProxy->VertexBuffer.Vertices) FBrickVertex(
								LocalVertexCoordinates,
								LocalVertexAmbientFactors[(LocalVertexCoordinates.Y * LocalVertexDim.X + LocalVertexCoordinates.X) * LocalVertexDim.Z + LocalVertexCoordinates.Z]
								);
						}
						else
						{
							VertexIndexMap.Add(0);
						}
					}
				}
			}

			// Iterate over each brick in the chunk.
			for(int32 LocalBrickY = LocalBrickExpansion.Y; LocalBrickY < Grid->BricksPerRenderChunk.Y + LocalBrickExpansion.Y; ++LocalBrickY)
			{
				for(int32 LocalBrickX = LocalBrickExpansion.X; LocalBrickX < Grid->BricksPerRenderChunk.X + LocalBrickExpansion.X; ++LocalBrickX)
				{
					for(int32 LocalBrickZ = LocalBrickExpansion.Z; LocalBrickZ < Grid->BricksPerRenderChunk.Z + LocalBrickExpansion.Z; ++LocalBrickZ)
					{
						// Only draw faces of bricks that aren't empty.
						const uint32 LocalBrickIndex = (LocalBrickY * LocalBricksDim.X + LocalBrickX) * LocalBricksDim.Z + LocalBrickZ;
						const uint8 BrickMaterial = LocalBrickMaterials[LocalBrickIndex];
						if (BrickMaterial != EmptyMaterialIndex)
						{
							const FInt3 RelativeBrickCoordinates = FInt3(LocalBrickX,LocalBrickY,LocalBrickZ) - LocalBrickExpansion;
							for(uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
							{
								// Only draw faces that face empty bricks.
								const int32 FacingLocalBrickX = LocalBrickX + FaceNormals[FaceIndex].X;
								const int32 FacingLocalBrickY = LocalBrickY + FaceNormals[FaceIndex].Y;
								const int32 FacingLocalBrickZ = LocalBrickZ + FaceNormals[FaceIndex].Z;
								const uint32 FacingLocalBrickIndex = (FacingLocalBrickY * LocalBricksDim.X + FacingLocalBrickX) * LocalBricksDim.Z + FacingLocalBrickZ;
								const uint32 FrontBrickMaterial = LocalBrickMaterials[FacingLocalBrickIndex];
								if(FrontBrickMaterial == EmptyMaterialIndex)
								{
									uint16 FaceVertexIndices[4];
									for (uint32 FaceVertexIndex = 0; FaceVertexIndex < 4; ++FaceVertexIndex)
									{
										const FInt3 CornerVertexOffset = GetCornerVertexOffset(FaceVertices[FaceIndex][FaceVertexIndex]);
										const FInt3 LocalVertexCoordinates = RelativeBrickCoordinates + CornerVertexOffset;
										FaceVertexIndices[FaceVertexIndex] = VertexIndexMap[(LocalVertexCoordinates.Y * LocalVertexDim.X + LocalVertexCoordinates.X) * LocalVertexDim.Z + LocalVertexCoordinates.Z];
									}

									// Write the indices for the brick face.
									FFaceBatch& FaceBatch = MaterialBatches[BrickMaterial].FaceBatches[FaceIndex];
									uint16* FaceVertexIndex = &FaceBatch.Indices[FaceBatch.Indices.AddUninitialized(6)];
									*FaceVertexIndex++ = FaceVertexIndices[0];
									*FaceVertexIndex++ = FaceVertexIndices[1];
									*FaceVertexIndex++ = FaceVertexIndices[2];
									*FaceVertexIndex++ = FaceVertexIndices[0];
									*FaceVertexIndex++ = FaceVertexIndices[2];
									*FaceVertexIndex++ = FaceVertexIndices[3];
								}
							}
						}
					}
				}
			}

			// Create mesh elements for each batch.
			int32 NumIndices = 0;
			for(int32 BrickMaterialIndex = 0; BrickMaterialIndex < MaterialBatches.Num(); ++BrickMaterialIndex)
			{
				for(uint32 FaceIndex = 0;FaceIndex < 6;++FaceIndex)
				{
					NumIndices += MaterialBatches[BrickMaterialIndex].FaceBatches[FaceIndex].Indices.Num();
				}
			}
			SceneProxy->IndexBuffer.Indices.Empty(NumIndices);
			for(int32 BrickMaterialIndex = 0; BrickMaterialIndex < MaterialBatches.Num(); ++BrickMaterialIndex)
			{
				UMaterialInterface* SurfaceMaterial = Grid->Parameters.Materials[BrickMaterialIndex].SurfaceMaterial;
				if(SurfaceMaterial == NULL)
				{
					SurfaceMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
				}
				SceneProxy->MaterialRelevance |= SurfaceMaterial->GetRelevance_Concurrent(GetScene()->GetFeatureLevel());
				const int32 ProxyMaterialIndex = SceneProxy->Materials.AddUnique(SurfaceMaterial);

				UMaterialInterface* OverrideTopSurfaceMaterial = Grid->Parameters.Materials[BrickMaterialIndex].OverrideTopSurfaceMaterial;
				if(OverrideTopSurfaceMaterial)
				{
					SceneProxy->MaterialRelevance |= OverrideTopSurfaceMaterial->GetRelevance_Concurrent(GetScene()->GetFeatureLevel());
				}
				const int32 TopProxyMaterialIndex = OverrideTopSurfaceMaterial ? SceneProxy->Materials.AddUnique(OverrideTopSurfaceMaterial) : ProxyMaterialIndex;

				for(uint32 FaceIndex = 0;FaceIndex < 6;++FaceIndex)
				{
					const FFaceBatch& FaceBatch = MaterialBatches[BrickMaterialIndex].FaceBatches[FaceIndex];
					if (FaceBatch.Indices.Num() > 0)
					{
						FBrickChunkSceneProxy::FElement& Element = *new(SceneProxy->Elements)FBrickChunkSceneProxy::FElement;
						Element.FirstIndex = SceneProxy->IndexBuffer.Indices.Num();
						Element.NumPrimitives = FaceBatch.Indices.Num() / 3;
						Element.MaterialIndex = FaceIndex == 5 ? TopProxyMaterialIndex : ProxyMaterialIndex;
						Element.FaceIndex = FaceIndex;

						// Append the batch's indices to the index buffer.
						SceneProxy->IndexBuffer.Indices.Append(FaceBatch.Indices);
					}
				}
			}

			SceneProxy->LocalBrickMaterials.Empty();

			UE_LOG(LogStats,Log,TEXT("Brick render component setup took %fms to create %u indices and %u vertices"),1000.0f * float(FPlatformTime::Seconds() - StartTime),SceneProxy->IndexBuffer.Indices.Num(),SceneProxy->VertexBuffer.Vertices.Num());

		},TStatId(),NULL);

		SceneProxy->BeginInitResources();
	}

	UE_LOG(LogStats,Log,TEXT("UBrickRenderComponent::CreateSceneProxy took %fms"),1000.0f * float(FPlatformTime::Seconds() - StartTime));

	return SceneProxy;
}

FBoxSphereBounds UBrickRenderComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	FBoxSphereBounds NewBounds;
	NewBounds.Origin = NewBounds.BoxExtent = Grid->BricksPerRenderChunk.ToFloat() / 2.0f;
	NewBounds.SphereRadius = NewBounds.BoxExtent.Size();
	return NewBounds.TransformBy(LocalToWorld);
}
