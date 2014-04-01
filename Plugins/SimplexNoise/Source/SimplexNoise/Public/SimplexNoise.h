// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once

#include "ModuleManager.h"

/** The dynamically-loaded module interface. */
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

	// Samples a 1D simplex noise function, returning a value between 0-1.
	virtual float Sample(float X) = 0;
	// Samples a 2D simplex noise function, returning a value between 0-1.
	virtual float Sample2D(float X,float Y) = 0;
	// Samples a 3D simplex noise function, returning a value between 0-1.
	virtual float Sample3D(float X,float Y,float Z) = 0;
};

/** The statically-loaded module interface. */
namespace SimplexNoise
{
	// Samples a 1D simplex noise function, returning a value between 0-1.
	SIMPLEXNOISE_API float Sample(float X);
	// Samples a 2D simplex noise function, returning a value between 0-1.
	SIMPLEXNOISE_API float Sample2D(float X,float Y);
	// Samples a 3D simplex noise function, returning a value between 0-1.
	SIMPLEXNOISE_API float Sample3D(float X,float Y,float Z);
}
