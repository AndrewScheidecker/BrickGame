// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once

#include "ModuleManager.h"

/** The module's public interface. */
class ISimplexNoise : public IModuleInterface
{
public:
	static inline ISimplexNoise& Get()
	{
		return FModuleManager::LoadModuleChecked< ISimplexNoise >( "SimplexNoise" );
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "SimplexNoise" );
	}

	virtual float Sample(float X) = 0;
	virtual float Sample2D(float X,float Y) = 0;
	virtual float Sample3D(float X,float Y,float Z) = 0;
};

namespace SimplexNoise
{
	SIMPLEXNOISE_API float Sample(float X);
	SIMPLEXNOISE_API float Sample2D(float X,float Y);
	SIMPLEXNOISE_API float Sample3D(float X,float Y,float Z);
}
