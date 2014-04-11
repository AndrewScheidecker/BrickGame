// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "SimplexNoise.h"
#include "SimplexNoiseLibrary.h"
#include "SimplexNoise.generated.inl"
#include "PublicDomainSimplexNoise.inl"

static FPublicDomainSimplexNoiseImplementation Implementation;

// Dynamically-loaded module interface
class FSimplexNoise : public ISimplexNoise
{
	virtual float Sample(float X) OVERRIDE
	{
		return Implementation.Generate(X);
	}
	virtual float Sample2D(float X,float Y) OVERRIDE
	{
		return Implementation.Generate(X,Y);
	}
	virtual float Sample3D(float X,float Y,float Z) OVERRIDE
	{
		return Implementation.Generate(X,Y,Z);
	}
};
IMPLEMENT_MODULE( FSimplexNoise, SimplexNoise )

// BluePrint-accessible interface
USimplexNoiseLibrary::USimplexNoiseLibrary(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP) {}
float USimplexNoiseLibrary::Sample(float X)
{
	return Implementation.Generate(X);
}
float USimplexNoiseLibrary::Sample2D(float X,float Y)
{
	return Implementation.Generate(X,Y);
}
float USimplexNoiseLibrary::Sample3D(float X,float Y,float Z)
{
	return Implementation.Generate(X,Y,Z);
}

// Statically-loaded module interface
namespace SimplexNoise
{
	float Sample(float X)
	{
		return Implementation.Generate(X);
	}
	float Sample2D(float X,float Y)
	{
		return Implementation.Generate(X,Y);
	}
	float Sample3D(float X,float Y,float Z)
	{
		return Implementation.Generate(X,Y,Z);
	}
};
