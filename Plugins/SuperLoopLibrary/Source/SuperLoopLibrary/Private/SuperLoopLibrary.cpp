// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "SuperLoopLibrary.h"

IMPLEMENT_MODULE( IModuleInterface, SuperLoopLibrary )

USuperLoopLibrary::USuperLoopLibrary(const class FObjectInitializer& Initializer) : Super(Initializer) {}

void USuperLoopLibrary::ResetRunawayLoopCounter()
{
	// Reset the runaway loop counter to 0.
	GInitRunaway();
}