// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

static void ComputeChunkAO(
	const UBrickGridComponent* Grid,
	const FInt3 MinLocalBrickCoordinates,
	const FInt3 LocalBrickExpansion,
	const FInt3 LocalBricksDim,
	const FInt3 LocalVertexDim,
	const TArray<uint16>& LocalBrickMaterials,
	TArray<uint8>& OutLocalVertexAmbientFactors
	)
{
	// Allocate a binary visibility mask for whether each brick in the region can directly see the sky.
	const uint32 BlurRadius = Grid->Parameters.AmbientOcclusionBlurRadius;
	const uint32 BlurDiameter = BlurRadius * 2;
	const uint32 FixedBlurDenominator = (255ul << 24) / FMath::Square(BlurDiameter + 1);
	check(LocalVertexDim == LocalBricksDim - LocalBrickExpansion * FInt3::Scalar(2) + FInt3::Scalar(1));

	// For each XY in the chunk, find the highest non-empty brick between the bottom of the chunk and the top of the grid.
	TArray<int8> MaxNonEmptyBrickLocalZs;
	MaxNonEmptyBrickLocalZs.Init(LocalBricksDim.X * LocalBricksDim.Y);
	Grid->GetMaxNonEmptyBrickZ(MinLocalBrickCoordinates,MinLocalBrickCoordinates + LocalBricksDim - FInt3::Scalar(1),MaxNonEmptyBrickLocalZs);

	// Allocate filtered ambient occlusion factors for each brick adjacent to the output vertices.
	TArray<uint8> LocalBrickAmbientFactors;
	const FInt3 LocalBrickAmbientFactorsDim = LocalBricksDim - FInt3(BlurDiameter,BlurDiameter,0);
	check(LocalBrickAmbientFactorsDim == LocalVertexDim + FInt3::Scalar(1));
	LocalBrickAmbientFactors.Init(LocalBrickAmbientFactorsDim.X * LocalBrickAmbientFactorsDim.Y * LocalBrickAmbientFactorsDim.Z);

	// Allocate a buffer for the result of applying the X half of a separable blur to a single Z slice of bricks.
	const int32 BricksInHalfFilteredBufferX = LocalBricksDim.X - BlurDiameter;
	TArray<uint8> HalfFilteredVisibility;
	HalfFilteredVisibility.Init(BricksInHalfFilteredBufferX * LocalBricksDim.Y);

	for(int32 AmbientBrickZ = 0;AmbientBrickZ < LocalBrickAmbientFactorsDim.Z;++AmbientBrickZ)
	{
		// Apply the X half of the separable blur to this Z slice.
		for(int32 HalfFilteredX = 0;HalfFilteredX < BricksInHalfFilteredBufferX;++HalfFilteredX)
		{
			for(int32 LocalBrickY = 0;LocalBrickY < LocalBricksDim.Y;++LocalBrickY)
			{
				uint32 SummedVisibility = 0;
				for(uint32 FilterX = 0;FilterX < BlurDiameter + 1;++FilterX)
				{
					const int8 MaxNonEmptyBrickLocalZ = MaxNonEmptyBrickLocalZs[LocalBrickY * LocalBricksDim.X + HalfFilteredX + FilterX];
					SummedVisibility += MaxNonEmptyBrickLocalZ >= AmbientBrickZ ? 0 : 1;
				}

				const uint32 HalfFilteredBrickIndex = HalfFilteredX * LocalBricksDim.Y + LocalBrickY;
				HalfFilteredVisibility[HalfFilteredBrickIndex] = (uint8)SummedVisibility;
			}
		}

		// Apply the Y half of the separable blur to this Z slice.
		for(int32 AmbientBrickY = 0;AmbientBrickY < LocalBrickAmbientFactorsDim.Y;++AmbientBrickY)
		{
			for(int32 AmbientBrickX = 0;AmbientBrickX < LocalBrickAmbientFactorsDim.X;++AmbientBrickX)
			{
				uint32 FilteredVisibility = 0;
				for(uint32 FilterY = 0;FilterY < BlurDiameter + 1;++FilterY)
				{
					FilteredVisibility += HalfFilteredVisibility[AmbientBrickX * LocalBricksDim.Y + AmbientBrickY + FilterY];
				}
				FilteredVisibility *= FixedBlurDenominator;
				FilteredVisibility >>= 24;

				const uint32 AmbientBrickIndex = (AmbientBrickY * LocalBrickAmbientFactorsDim.X + AmbientBrickX) * LocalBrickAmbientFactorsDim.Z + AmbientBrickZ;
				LocalBrickAmbientFactors[AmbientBrickIndex] = (uint8)FilteredVisibility;
			}
		}
	}

	// Compute a filtered per-vertex ambient occlusion factor.
	for(int32 LocalVertexY = 0; LocalVertexY < LocalVertexDim.Y; ++LocalVertexY)
	{
		for(int32 LocalVertexX = 0; LocalVertexX < LocalVertexDim.X; ++LocalVertexX)
		{
			for(int32 LocalVertexZ = 0; LocalVertexZ < LocalVertexDim.Z; ++LocalVertexZ)
			{
				uint32 AdjacentAmbientFactorSum = 0;
				for(uint32 AdjacentIndex = 0;AdjacentIndex < 8;++AdjacentIndex)
				{
					const uint32 AmbientBrickX = LocalVertexX + ((AdjacentIndex >> 0) & 1);
					const uint32 AmbientBrickY = LocalVertexY + ((AdjacentIndex >> 1) & 1);
					const uint32 AmbientBrickZ = LocalVertexZ + (AdjacentIndex >> 2);
					const uint32 AmbientBrickIndex = (AmbientBrickY * LocalBrickAmbientFactorsDim.X + AmbientBrickX) * LocalBrickAmbientFactorsDim.Z + AmbientBrickZ;
					AdjacentAmbientFactorSum += LocalBrickAmbientFactors[AmbientBrickIndex];
				}

				// Average the ambient factor from all 8 bricks adjacent to the vertex.
				// Normalize it with an implicit factor of 2 since we'll treat the result as a hemisphere percent.
				const uint8 AverageAdjacentAmbientFactor = (uint8)FMath::Min<uint32>(255,AdjacentAmbientFactorSum / 4);

				const uint32 LocalVertexIndex = (LocalVertexY * LocalVertexDim.X + LocalVertexX) * LocalVertexDim.Z + LocalVertexZ;
				OutLocalVertexAmbientFactors[LocalVertexIndex] = AverageAdjacentAmbientFactor;
			}
		}
	}
}
