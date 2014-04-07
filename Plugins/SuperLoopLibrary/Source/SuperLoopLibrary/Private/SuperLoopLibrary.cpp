// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "SuperLoopLibrary.h"
#include "SuperLoopLibrary.generated.inl"

IMPLEMENT_MODULE( IModuleInterface, SuperLoopLibrary )

USuperLoopLibrary::USuperLoopLibrary(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP) {}

void USuperLoopLibrary::ResetRunawayLoopCounter()
{
	// Reset the runaway loop counter to 0.
	GInitRunaway();
}