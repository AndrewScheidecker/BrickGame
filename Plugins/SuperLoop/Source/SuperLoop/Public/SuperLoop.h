// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once

#include "ModuleManager.h"

/** The module's public interface. */
class ISuperLoop : public IModuleInterface
{
public:
	static inline ISuperLoop& Get()
	{
		return FModuleManager::LoadModuleChecked< ISuperLoop >( "SuperLoop" );
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "SuperLoop" );
	}
};
