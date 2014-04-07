// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "SaveWorldLibrary.h"
#include "Engine.h"
#include "SaveWorldLibrary.generated.inl"

IMPLEMENT_MODULE( IModuleInterface, SaveWorldLibrary )

USaveWorldLibrary::USaveWorldLibrary(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP) {}

static FString GetSaveFilePath(const TCHAR* SaveFilename)
{
	return FString::Printf(TEXT("%s/SaveGames/%s.umap"), *FPaths::GameSavedDir(), SaveFilename);
}

bool USaveWorldLibrary::SaveGameWorld(const FString& Filename)
{
	return UPackage::SavePackage(GWorld->GetOutermost(),GWorld,(EObjectFlags)0,*GetSaveFilePath(*Filename));
}

bool USaveWorldLibrary::LoadGameWorld(const FString& Filename)
{
	GEngine->SetClientTravel(GWorld, *GetSaveFilePath(*Filename), TRAVEL_Absolute);
	return true;
}
