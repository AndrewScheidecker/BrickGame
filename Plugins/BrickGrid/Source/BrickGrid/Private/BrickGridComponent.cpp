// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickGridPluginPrivatePCH.h"
#include "BrickGridComponent.h"

static const FIntVector BrickVertices[8] =
{
	FIntVector(0, 0, 0),
	FIntVector(0, 0, 1),
	FIntVector(0, 1, 0),
	FIntVector(0, 1, 1),
	FIntVector(1, 0, 0),
	FIntVector(1, 0, 1),
	FIntVector(1, 1, 0),
	FIntVector(1, 1, 1)
};
static const FIntVector FaceUVs[4] =
{
	FIntVector(0, 255, 0),
	FIntVector(0, 0, 0),
	FIntVector(255, 0, 0),
	FIntVector(255, 255, 0)
};
static const uint32 FaceVertices[6][4] =
{
	{ 2, 3, 1, 0 },		// -X
	{ 4, 5, 7, 6 },		// +X
	{ 0, 1, 5, 4 },		// -Y
	{ 6, 7, 3, 2 },		// +Y
	{ 4, 6, 2, 0 },		// -Z
	{ 1, 3, 7, 5 }		// +Z
};
static const FIntVector FaceNormal[6] =
{
	FIntVector(-1, 0, 0),
	FIntVector(+1, 0, 0),
	FIntVector(0, -1, 0),
	FIntVector(0, +1, 0),
	FIntVector(0, 0, -1),
	FIntVector(0, 0, +1)
};

struct FBrickVertex
{
	uint8 X;
	uint8 Y;
	uint8 Z;
	uint8 Padding0;
	uint8 U;
	uint8 V;
	uint8 Padding1;
	uint8 Padding2;
	FPackedNormal TangentX;
	FPackedNormal TangentZ;
	FBrickVertex(const FIntVector& XYZ, const FIntVector& UV, const FVector& InTangentX, const FVector& InTangentZ)
		: X(XYZ.X)
		, Y(XYZ.Y)
		, Z(XYZ.Z)
		, U(UV.X)
		, V(UV.Y)
		, TangentX(FVector(InTangentX))
		, TangentZ(FVector(InTangentZ))
	{}
};

/** Vertex Buffer */
class FBrickGridVertexBuffer : public FVertexBuffer 
{
public:
	TArray<FBrickVertex> Vertices;

	virtual void InitRHI()
	{
		if (Vertices.Num() > 0)
		{
			VertexBufferRHI = RHICreateVertexBuffer(Vertices.Num() * sizeof(FBrickVertex), NULL, BUF_Static);

			// Copy the vertex data into the vertex buffer.
			void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, 0, Vertices.Num() * sizeof(FBrickVertex), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, Vertices.GetTypedData(), Vertices.Num() * sizeof(FBrickVertex));
			RHIUnlockVertexBuffer(VertexBufferRHI);
		}
	}
};

/** Index Buffer */
class FBrickGridIndexBuffer : public FIndexBuffer 
{
public:
	TArray<int32> Indices;

	virtual void InitRHI()
	{
		if (Indices.Num() > 0)
		{
			IndexBufferRHI = RHICreateIndexBuffer(sizeof(int32), Indices.Num() * sizeof(int32), NULL, BUF_Static);

			// Write the indices to the index buffer.
			void* Buffer = RHILockIndexBuffer(IndexBufferRHI, 0, Indices.Num() * sizeof(int32), RLM_WriteOnly);
			FMemory::Memcpy(Buffer, Indices.GetTypedData(), Indices.Num() * sizeof(int32));
			RHIUnlockIndexBuffer(IndexBufferRHI);
		}
	}
};

/** Vertex Factory */
class FBrickGridVertexFactory : public FLocalVertexFactory
{
public:

	FBrickGridVertexFactory()
	{}


	/** Initialization */
	void Init(const FBrickGridVertexBuffer* VertexBuffer)
	{
		check(!IsInRenderingThread());

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitBrickGridVertexFactory,
			FBrickGridVertexFactory*,VertexFactory,this,
			const FBrickGridVertexBuffer*,VertexBuffer,VertexBuffer,
		{
			// Initialize the vertex factory's stream components.
			DataType NewData;
			NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FBrickVertex, X, VET_UByte4N);
			NewData.TextureCoordinates.Add(STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FBrickVertex, U, VET_UByte4N));
			NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FBrickVertex, TangentX, VET_PackedNormal);
			NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FBrickVertex, TangentZ, VET_PackedNormal);
			VertexFactory->SetData(NewData);
		});
	}
};

/** Scene proxy */
class FBrickGridSceneProxy : public FPrimitiveSceneProxy
{
public:

	FBrickGridSceneProxy(UBrickGridComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, MaterialRelevance(Component->GetMaterialRelevance())
	{
		// Iterate over 255x255x255 chunks of the bricks.
		static const uint32 MaxChunkSize = 255;
		const uint32 SizeX = Component->GetSizeX();
		const uint32 SizeY = Component->GetSizeY();
		const uint32 SizeZ = Component->GetSizeZ();
		for (uint32 ChunkBaseZ = 0; ChunkBaseZ < SizeZ; ChunkBaseZ += MaxChunkSize)
		{
			for (uint32 ChunkBaseY = 0; ChunkBaseY < SizeY; ChunkBaseY += MaxChunkSize)
			{
				for (uint32 ChunkBaseX = 0; ChunkBaseX < SizeX; ChunkBaseX += MaxChunkSize)
				{
					const uint32 ChunkSizeX = FMath::Min(SizeX - ChunkBaseX, MaxChunkSize);
					const uint32 ChunkSizeY = FMath::Min(SizeY - ChunkBaseY, MaxChunkSize);
					const uint32 ChunkSizeZ = FMath::Min(SizeZ - ChunkBaseZ, MaxChunkSize);

					// Create a new chunk transform.
					const int32 ChunkIndex = Chunks.Num();
					FChunk& Chunk = *new(Chunks)FChunk;
					Chunk.ChunkToLocal = FScaleMatrix(FVector(255, 255, 255)) * FTranslationMatrix(FVector(ChunkBaseX, ChunkBaseY, ChunkBaseZ));

					// Group the face by material and direction.
					struct FMaterialBatch
					{
						TArray<int32> Indices[6];
						FBox Bounds[6];
						FMaterialBatch()
						{
							for (uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
							{
								Bounds[FaceIndex].Init();
							}
						}
					};
					TArray<FMaterialBatch> MaterialBatches;
					MaterialBatches.Init(FMaterialBatch(),Component->GetNumBrickMaterials());
					for (uint32 ChunkZ = 0; ChunkZ < ChunkSizeZ; ++ChunkZ)
					{
						for (uint32 ChunkY = 0; ChunkY < ChunkSizeY; ++ChunkY)
						{
							for (uint32 ChunkX = 0; ChunkX < ChunkSizeX; ++ChunkX)
							{
								const int32 X = ChunkBaseX + ChunkX;
								const int32 Y = ChunkBaseY + ChunkY;
								const int32 Z = ChunkBaseZ + ChunkZ;
								const uint32 BrickMaterial = Component->Get(X,Y,Z);
								if (BrickMaterial != 0)
								{
									for (uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
									{
										// Only draw faces that face unoccupied bricks.
										const FIntVector FrontBrickXYZ = FIntVector(X, Y, Z) + FaceNormal[FaceIndex];
										if (Component->Get(FrontBrickXYZ) == 0)
										{
											const FVector FaceTangentX(BrickVertices[FaceVertices[FaceIndex][2]] - BrickVertices[FaceVertices[FaceIndex][0]]);
											const FVector FaceTangentY(BrickVertices[FaceVertices[FaceIndex][0]] - BrickVertices[FaceVertices[FaceIndex][1]]);
											const FVector FaceTangentZ = FaceTangentY ^ FaceTangentX;
											const FPackedNormal PackedFaceTangentX(FaceTangentX);
											const FPackedNormal PackedFaceTangentZ(FaceTangentZ);
											const uint32 BaseFaceVertexIndex = VertexBuffer.Vertices.Num();
											for (uint32 FaceVertexIndex = 0; FaceVertexIndex < 4; ++FaceVertexIndex)
											{
												const FIntVector Position = FIntVector(ChunkX, ChunkY, ChunkZ) + BrickVertices[FaceVertices[FaceIndex][FaceVertexIndex]];
												new(VertexBuffer.Vertices) FBrickVertex(
													Position,
													FaceUVs[FaceVertexIndex],
													PackedFaceTangentX,
													PackedFaceTangentZ
													);
												MaterialBatches[BrickMaterial - 1].Bounds[FaceIndex] += (FVector)Position;
											}
											MaterialBatches[BrickMaterial - 1].Indices[FaceIndex].Add(BaseFaceVertexIndex + 0);
											MaterialBatches[BrickMaterial - 1].Indices[FaceIndex].Add(BaseFaceVertexIndex + 1);
											MaterialBatches[BrickMaterial - 1].Indices[FaceIndex].Add(BaseFaceVertexIndex + 2);
											MaterialBatches[BrickMaterial - 1].Indices[FaceIndex].Add(BaseFaceVertexIndex + 0);
											MaterialBatches[BrickMaterial - 1].Indices[FaceIndex].Add(BaseFaceVertexIndex + 2);
											MaterialBatches[BrickMaterial - 1].Indices[FaceIndex].Add(BaseFaceVertexIndex + 3);
										}
									}
								}
							}
						}
					}

					for (int32 MaterialIndex = 0; MaterialIndex < MaterialBatches.Num(); ++MaterialIndex)
					{
						for (uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
						{
							const uint32 FirstIndex = IndexBuffer.Indices.Num();
							if (MaterialBatches[MaterialIndex].Indices[FaceIndex].Num() > 0)
							{
								IndexBuffer.Indices.Append(MaterialBatches[MaterialIndex].Indices[FaceIndex]);

								FElement& Element = *new(Elements)FElement;
								Element.FirstIndex = FirstIndex;
								Element.NumPrimitives = MaterialBatches[MaterialIndex].Indices[FaceIndex].Num() / 3;
								Element.ChunkIndex = ChunkIndex;
								Element.MaterialIndex = MaterialIndex;
								Element.LocalBounds = MaterialBatches[MaterialIndex].Bounds[FaceIndex];
								Element.LocalViewBoundingPlane = FPlane(
									(FVector)FaceNormal[FaceIndex],
									-FMath::Min(
										Element.LocalBounds.Min | (FVector)FaceNormal[FaceIndex],
										Element.LocalBounds.Max | (FVector)FaceNormal[FaceIndex]
										)
									);
							}
						}
					}
				}
			}
		}

		for (int32 MaterialIndex = 0; MaterialIndex < Component->GetNumMaterials(); ++MaterialIndex)
		{
			// Grab material
			UMaterialInterface* Material = Component->GetMaterial(MaterialIndex);
			if (Material == NULL)
			{
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}
			Materials.Add(Material);
		}

		// Init vertex factory
		VertexFactory.Init(&VertexBuffer);

		// Enqueue initialization of render resource
		BeginInitResource(&VertexBuffer);
		BeginInitResource(&IndexBuffer);
		BeginInitResource(&VertexFactory);
	}

	virtual ~FBrickGridSceneProxy()
	{
		VertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View)
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_BrickGridSceneProxy_DrawDynamicElements );

		const bool bWireframe = View->Family->EngineShowFlags.Wireframe;

		FColoredMaterialRenderProxy WireframeMaterialInstance(
			GEngine->WireframeMaterial->GetRenderProxy(IsSelected()),
			FLinearColor(0, 0.5f, 1.f)
			);

		for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
		{
			const FElement& Element = Elements[ElementIndex];

			FMaterialRenderProxy* MaterialProxy = NULL;
			if (bWireframe)
			{
				MaterialProxy = &WireframeMaterialInstance;
			}
			else
			{
				MaterialProxy = Materials[Element.MaterialIndex]->GetRenderProxy(IsSelected());
			}

			// Draw the mesh.
			FMeshBatch Mesh;
			Mesh.bWireframe = bWireframe;
			Mesh.VertexFactory = &VertexFactory;
			Mesh.MaterialRenderProxy = MaterialProxy;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.CastShadow = true;
			Mesh.Elements[0].FirstIndex = Element.FirstIndex;
			Mesh.Elements[0].NumPrimitives = Element.NumPrimitives;
			Mesh.Elements[0].MinVertexIndex = IndexBuffer.Indices[Element.FirstIndex];
			Mesh.Elements[0].MaxVertexIndex = IndexBuffer.Indices[Element.FirstIndex + Element.NumPrimitives * 3 - 1];
			Mesh.Elements[0].IndexBuffer = &IndexBuffer;
			Mesh.Elements[0].PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(Chunks[Element.ChunkIndex].ChunkToLocal * GetLocalToWorld(), GetBounds(), GetLocalBounds(), true);
			PDI->DrawMesh(Mesh);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
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

	FBrickGridVertexBuffer VertexBuffer;
	FBrickGridIndexBuffer IndexBuffer;
	FBrickGridVertexFactory VertexFactory;

	struct FChunk
	{
		FMatrix ChunkToLocal;
	};

	struct FElement
	{
		uint32 FirstIndex;
		uint32 NumPrimitives;
		uint32 ChunkIndex;
		uint32 MaterialIndex;
		FPlane LocalViewBoundingPlane;
		FBox LocalBounds;
	};

	TArray<FChunk> Chunks;
	TArray<UMaterialInterface*> Materials;
	TArray<FElement> Elements;

	FMaterialRelevance MaterialRelevance;
};

//////////////////////////////////////////////////////////////////////////

void UBrickGridComponent::Init(int32 InSizeX,int32 InSizeY,int32 InSizeZ,int32 InNumBrickMaterials,int32 InDefaultMaterialIndex)
{
	SizeX = (uint32)FMath::Clamp(InSizeX,0,255);
	SizeY = (uint32)FMath::Clamp(InSizeY, 0, 255);
	SizeZ = (uint32)FMath::Clamp(InSizeZ, 0, 255);
	NumBrickMaterials = (uint32)FMath::Clamp<int32>(InNumBrickMaterials,0,INT_MAX);
	DefaultMaterialIndex = FMath::Clamp<int32>(InDefaultMaterialIndex, 0, NumBrickMaterials + 1);
	// Compute the number of bits needed to store InNumBrickMaterials values, round it up to the next power of two, and clamp it between 1-32.
	// Rounding it to a power of two allows us to assume that the bricks for each bit end up in exactly one DWORD.
	BitsPerBrickLog2 = FMath::Clamp<uint32>(FMath::CeilLogTwo(FMath::CeilLogTwo(NumBrickMaterials + 1)), 1, 5);
	BitsPerBrick = 1 << BitsPerBrickLog2;
	BricksPerDWordLog2 = 5 - BitsPerBrickLog2;
	BrickContents.Init(0, FMath::DivideAndRoundUp<uint32>(SizeX * SizeY * SizeZ * BitsPerBrick, 32));
}

int32 UBrickGridComponent::Get(const FIntVector& XYZ) const
{
	return Get(XYZ.X,XYZ.Y,XYZ.Z);
}
int32 UBrickGridComponent::Get(int32 X,int32 Y,int32 Z) const
{
	if (X >= 0 && X < (int32)SizeX && Y >= 0 && Y < (int32)SizeY && Z >= 0 && Z < (int32)SizeZ)
	{
		uint32 DWordIndex;
		uint32 Shift;
		uint32 Mask;
		CalcIndexShiftMask((uint32)X,(uint32)Y,(uint32)Z,DWordIndex,Shift,Mask);
		return (int32)((BrickContents[DWordIndex] >> Shift) & Mask);
	}
	else
	{
		return DefaultMaterialIndex;
	}
}

void UBrickGridComponent::Set(const FIntVector& XYZ,int32 MaterialIndex)
{
	return Set(XYZ.X,XYZ.Y,XYZ.Z,MaterialIndex);
}
void UBrickGridComponent::Set(int32 X,int32 Y,int32 Z,int32 MaterialIndex)
{
	if (X >= 0 && X < (int32)SizeX && Y >= 0 && Y < (int32)SizeY && Z >= 0 && Z < (int32)SizeZ)
	{
		uint32 DWordIndex;
		uint32 Shift;
		uint32 Mask;
		CalcIndexShiftMask((uint32)X,(uint32)Y,(uint32)Z,DWordIndex,Shift,Mask);
		uint32& DWord = BrickContents[DWordIndex];
		DWord &= ~(Mask << Shift);
		DWord |= ((uint32)MaterialIndex & Mask) << Shift;
		MarkRenderStateDirty();
	}
}

void UBrickGridComponent::CalcIndexShiftMask(uint32 X,uint32 Y,uint32 Z,uint32& OutDWordIndex,uint32& OutShift,uint32& OutMask) const
{
	const uint32 BrickIndex = (Z * SizeY + Y) * SizeX + X;
	const uint32 SubDWordIndex = BrickIndex & ((1 << BricksPerDWordLog2) - 1);
	OutDWordIndex = BrickIndex >> BricksPerDWordLog2;
	OutShift = SubDWordIndex << BitsPerBrickLog2;
	OutMask = (1 << BitsPerBrick) - 1;
}

UBrickGridComponent::UBrickGridComponent( const FPostConstructInitializeProperties& PCIP )
	: Super( PCIP )
{
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
}

FPrimitiveSceneProxy* UBrickGridComponent::CreateSceneProxy()
{
	return new FBrickGridSceneProxy(this);
}

FBoxSphereBounds UBrickGridComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	FBoxSphereBounds NewBounds;
	NewBounds.Origin = NewBounds.BoxExtent = FVector(GetSizeX(),GetSizeY(),GetSizeZ()) / 2.0f;
	NewBounds.SphereRadius = NewBounds.BoxExtent.Size();
	return NewBounds.TransformBy(LocalToWorld);
}
