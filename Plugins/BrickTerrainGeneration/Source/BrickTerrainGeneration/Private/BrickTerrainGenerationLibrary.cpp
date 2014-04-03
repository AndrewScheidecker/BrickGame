// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickTerrainGenerationPluginPrivatePCH.h"
#include "BrickTerrainGenerationLibrary.h"
#include "BrickChunkComponent.h"

UBrickTerrainGenerationLibrary::UBrickTerrainGenerationLibrary( const FPostConstructInitializeProperties& PCIP )
: Super( PCIP )
{}

void UBrickTerrainGenerationLibrary::InitChunk(const FBrickTerrainGenerationParameters& Parameters,class UBrickChunkComponent* Chunk)
{
	for(int32 Y = 0;Y < Chunk->GetParameters().SizeY;++Y)
	{
		for(int32 X = 0;X < Chunk->GetParameters().SizeX;++X)
		{
			Chunk->SetBrick(X, Y, 0, Parameters.DirtMaterialIndex);
		}
	}
}
