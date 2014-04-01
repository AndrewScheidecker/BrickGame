// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "SuperLoop.h"
#include "SuperLoopLibrary.h"
#include "SuperLoop.generated.inl"

IMPLEMENT_MODULE( ISuperLoop, SuperLoop )

USuperLoopLibrary::USuperLoopLibrary(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP) {}

void USuperLoopLibrary::ResetRunawayLoopCounter()
{
	GInitRunaway();
}