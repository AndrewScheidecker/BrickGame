// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "SuperLoopLibrary.h"
#include "SuperLoop.generated.inl"

IMPLEMENT_MODULE( IModuleInterface, SuperLoop )

USuperLoopLibrary::USuperLoopLibrary(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP) {}

void USuperLoopLibrary::ResetRunawayLoopCounter()
{
	// Reset the runaway loop counter to 0.
	GInitRunaway();
}