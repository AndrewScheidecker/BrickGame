// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "BrickGridPluginPrivatePCH.h"

#include "BrickGrid.generated.inl"


class FBrickGridPlugin : public IBrickGridPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() OVERRIDE;
	virtual void ShutdownModule() OVERRIDE;
};

IMPLEMENT_MODULE( FBrickGridPlugin, BrickGrid )



void FBrickGridPlugin::StartupModule()
{
	
}


void FBrickGridPlugin::ShutdownModule()
{
	
}



