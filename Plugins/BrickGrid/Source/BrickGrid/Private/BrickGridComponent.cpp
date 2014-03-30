// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickGridPluginPrivatePCH.h"

#include "BrickGridComponent.h"

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

static FIntVector BrickVertices[8] =
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
static FIntVector FaceUVs[4] =
{
	FIntVector(0, 255, 0),
	FIntVector(0, 0, 0),
	FIntVector(255, 0, 0),
	FIntVector(255, 255, 0)
};
static uint32 FaceVertices[6][4] =
{
	{ 2, 3, 1, 0 },		// -X
	{ 4, 5, 7, 6 },		// +X
	{ 0, 1, 5, 4 },		// -Y
	{ 6, 7, 3, 2 },		// +Y
	{ 4, 6, 2, 0 },		// -Z
	{ 1, 3, 7, 5 }		// +Z
};
static FIntVector FaceNormal[6] =
{
	FIntVector(-1, 0, 0),
	FIntVector(+1, 0, 0),
	FIntVector(0, -1, 0),
	FIntVector(0, +1, 0),
	FIntVector(0, 0, -1),
	FIntVector(0, 0, +1)
};

/** Scene proxy */
class FBrickGridSceneProxy : public FPrimitiveSceneProxy
{
public:

	FBrickGridSceneProxy(UBrickGridComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, MaterialRelevance(Component->GetMaterialRelevance())
	{
		// Add each triangle to the vertex/index buffer
		static const int32 MaxChunkSize = 255;
		for (int32 ChunkBaseZ = 0; ChunkBaseZ < Component->BrickCountZ; ChunkBaseZ += MaxChunkSize)
		{
			for (int32 ChunkBaseY = 0; ChunkBaseY < Component->BrickCountY; ChunkBaseY += MaxChunkSize)
			{
				for (int32 ChunkBaseX = 0; ChunkBaseX < Component->BrickCountX; ChunkBaseX += MaxChunkSize)
				{
					const int32 ChunkSizeX = FMath::Min(Component->BrickCountX - ChunkBaseX, MaxChunkSize);
					const int32 ChunkSizeY = FMath::Min(Component->BrickCountY - ChunkBaseY, MaxChunkSize);
					const int32 ChunkSizeZ = FMath::Min(Component->BrickCountZ - ChunkBaseZ, MaxChunkSize);

					const int32 ChunkIndex = Chunks.Num();
					FChunk& Chunk = *new(Chunks)FChunk;
					Chunk.ChunkToLocal = FScaleMatrix(FVector(255, 255, 255)) * FTranslationMatrix(FVector(ChunkBaseX, ChunkBaseY, ChunkBaseZ));

					for (uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
					{
						TArray<int32> MaterialIndices[256];
						FBox LocalBounds[256];
						for (uint32 MaterialIndex = 0; MaterialIndex < 256; ++MaterialIndex)
						{
							LocalBounds[MaterialIndex].Init();
						}
						for (int32 ChunkZ = 0; ChunkZ < ChunkSizeZ; ++ChunkZ)
						{
							for (int32 ChunkY = 0; ChunkY < ChunkSizeY; ++ChunkY)
							{
								for (int32 ChunkX = 0; ChunkX < ChunkSizeX; ++ChunkX)
								{
									const int32 X = ChunkBaseX + ChunkX;
									const int32 Y = ChunkBaseY + ChunkY;
									const int32 Z = ChunkBaseZ + ChunkZ;
									const int32 BrickIndex = Z * Component->BrickCountX * Component->BrickCountY + Y * Component->BrickCountX + X;
									const uint8 BrickMaterial = Component->BrickContents[BrickIndex];
									if (BrickMaterial != 0)
									{
										const FIntVector FrontBrickXYZ = FIntVector(X, Y, Z) + FaceNormal[FaceIndex];
										const int32 FrontBrickIndex = FrontBrickXYZ.Z * Component->BrickCountX * Component->BrickCountY + FrontBrickXYZ.Y * Component->BrickCountX + FrontBrickXYZ.X;
										if (FrontBrickXYZ.X < 0 || FrontBrickXYZ.X >= Component->BrickCountX
											|| FrontBrickXYZ.Y < 0 || FrontBrickXYZ.Y >= Component->BrickCountY
											|| FrontBrickXYZ.Z < 0 || FrontBrickXYZ.Z >= Component->BrickCountZ
											|| Component->BrickContents[FrontBrickIndex] == 0)
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
												LocalBounds[BrickMaterial] += (FVector)Position;
											}
											MaterialIndices[BrickMaterial].Add(BaseFaceVertexIndex + 0);
											MaterialIndices[BrickMaterial].Add(BaseFaceVertexIndex + 1);
											MaterialIndices[BrickMaterial].Add(BaseFaceVertexIndex + 2);
											MaterialIndices[BrickMaterial].Add(BaseFaceVertexIndex + 0);
											MaterialIndices[BrickMaterial].Add(BaseFaceVertexIndex + 2);
											MaterialIndices[BrickMaterial].Add(BaseFaceVertexIndex + 3);
										}
									}
								}
							}
						}

						for (uint32 MaterialIndex = 0; MaterialIndex < 256; ++MaterialIndex)
						{
							const uint32 FirstIndex = IndexBuffer.Indices.Num();
							if (MaterialIndices[MaterialIndex].Num() > 0)
							{
								IndexBuffer.Indices.Append(MaterialIndices[MaterialIndex]);

								FElement& Element = *new(Elements)FElement;
								Element.FirstIndex = FirstIndex;
								Element.NumPrimitives = MaterialIndices[MaterialIndex].Num() / 3;
								Element.ChunkIndex = ChunkIndex;
								Element.MaterialIndex = MaterialIndex - 1;
								Element.LocalBounds = LocalBounds[MaterialIndex];
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

		for (uint32 MaterialIndex = 0; MaterialIndex < 256; ++MaterialIndex)
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

	virtual uint32 GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }

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

int32 UBrickGridComponent::GetNumMaterials() const
{
	return 255;
}

FBoxSphereBounds UBrickGridComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	FBoxSphereBounds NewBounds;
	NewBounds.Origin = FVector::ZeroVector;
	NewBounds.BoxExtent = FVector(HALF_WORLD_MAX,HALF_WORLD_MAX,HALF_WORLD_MAX);
	NewBounds.SphereRadius = FMath::Sqrt(3.0f * FMath::Square(HALF_WORLD_MAX));
	return NewBounds;
}

void UBrickGridComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == FName(TEXT("BrickCountX")) || PropertyName == FName(TEXT("BrickCountY")) || PropertyName == FName(TEXT("BrickCountZ")) || PropertyName == FName(TEXT("Layers")))
	{
		BrickCountX = FMath::Max<int32>(0, BrickCountX);
		BrickCountY = FMath::Max<int32>(0, BrickCountY);
		BrickCountZ = FMath::Max<int32>(0, BrickCountZ);
		InitBrickContents();
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

struct FNoiseParameter
{
	float	Base;
	float NoiseScale;
	float NoiseAmount;

	// Constructors.
	FNoiseParameter() {}
	FNoiseParameter(float InBase, float InScale, float InAmount) :
		Base(InBase),
		NoiseScale(InScale),
		NoiseAmount(InAmount)
	{}

	// Sample
	float Sample(int32 X, int32 Y) const
	{
		float	Noise = 0.0f;
		X = FMath::Abs(X);
		Y = FMath::Abs(Y);

		if (NoiseScale > DELTA)
		{
			for (uint32 Octave = 0; Octave < 4; Octave++)
			{
				float	OctaveShift = 1 << Octave;
				float	OctaveScale = OctaveShift / NoiseScale;
				Noise += PerlinNoise2D(X * OctaveScale, Y * OctaveScale) / OctaveShift;
			}
		}

		return Base + Noise * NoiseAmount;
	}

private:
	static const int32 Permutations[256];

	bool operator==(const FNoiseParameter& SrcNoise)
	{
		if ((Base == SrcNoise.Base) &&
			(NoiseScale == SrcNoise.NoiseScale) &&
			(NoiseAmount == SrcNoise.NoiseAmount))
		{
			return true;
		}

		return false;
	}

	void operator=(const FNoiseParameter& SrcNoise)
	{
		Base = SrcNoise.Base;
		NoiseScale = SrcNoise.NoiseScale;
		NoiseAmount = SrcNoise.NoiseAmount;
	}


	float Fade(float T) const
	{
		return T * T * T * (T * (T * 6 - 15) + 10);
	}


	float Grad(int32 Hash, float X, float Y) const
	{
		int32		H = Hash & 15;
		float	U = H < 8 || H == 12 || H == 13 ? X : Y,
			V = H < 4 || H == 12 || H == 13 ? Y : 0;
		return ((H & 1) == 0 ? U : -U) + ((H & 2) == 0 ? V : -V);
	}

	float PerlinNoise2D(float X, float Y) const
	{
		int32		TruncX = FMath::Trunc(X),
			TruncY = FMath::Trunc(Y),
			IntX = TruncX & 255,
			IntY = TruncY & 255;
		float	FracX = X - TruncX,
			FracY = Y - TruncY;

		float	U = Fade(FracX),
			V = Fade(FracY);

		int32	A = Permutations[IntX] + IntY,
			AA = Permutations[A & 255],
			AB = Permutations[(A + 1) & 255],
			B = Permutations[(IntX + 1) & 255] + IntY,
			BA = Permutations[B & 255],
			BB = Permutations[(B + 1) & 255];

		return	FMath::Lerp(FMath::Lerp(Grad(Permutations[AA], FracX, FracY),
			Grad(Permutations[BA], FracX - 1, FracY), U),
			FMath::Lerp(Grad(Permutations[AB], FracX, FracY - 1),
			Grad(Permutations[BB], FracX - 1, FracY - 1), U), V);
	}
};

const int32 FNoiseParameter::Permutations[256] =
{
	151, 160, 137, 91, 90, 15,
	131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23,
	190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
	88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166,
	77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244,
	102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
	135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123,
	5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42,
	223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
	129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228,
	251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107,
	49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
	138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
};

void UBrickGridComponent::InitBrickContents()
{
	BrickContents.Init(0, BrickCountX * BrickCountY * BrickCountZ);
	
	TArray<uint8> HeightMap;
	HeightMap.Init(0,BrickCountX * BrickCountY);

	for (int32 LayerIndex = 0; LayerIndex < Layers.Num(); ++LayerIndex)
	{
		const FBrickLayer& Layer = Layers[LayerIndex];
		const FNoiseParameter Noise((Layer.MinThickness + Layer.MaxThickness) * 0.5f, Layer.NoiseScale, (Layer.MaxThickness - Layer.MinThickness) * 0.5f);
		for (int32 Y = 0; Y < BrickCountY; ++Y)
		{
			for (int32 X = 0; X < BrickCountX; ++X)
			{
				uint8& BaseZ = HeightMap[Y * BrickCountX + X];
				const int32 Thickness = FMath::Round(Noise.Sample(X, Y));
				const uint8 ClampedThickness = FMath::Clamp((int32)BaseZ + Thickness, (int32)BaseZ, (int32)BrickCountZ) - (int32)BaseZ;
				for (int32 Depth = 0; Depth < ClampedThickness; ++Depth)
				{
					BrickContents[(BaseZ + Depth) * BrickCountX * BrickCountY + Y * BrickCountX + X] = Layer.MaterialIndex + 1;
				}
				BaseZ += ClampedThickness;
			}
		}
	}

	MarkRenderStateDirty();
}