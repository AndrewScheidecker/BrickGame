// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#pragma once
#include "CoreUObject.h"
#include "Engine.h"
#include "ConsoleCommand.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMultiCastConsoleCommandDelegate);

// A struct that encapsulates a persistent console command registration.
UCLASS(hidecategories=(Object), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Utility)
class UConsoleCommand : public UActorComponent
{
public:

	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Console)
	FString Command;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Console)
	FString Description;

	UPROPERTY(BlueprintAssignable,Category=Console)
	FMultiCastConsoleCommandDelegate OnCommand;

	IConsoleObject* Registration;

	// UObject interface.
	virtual void OnRegister() OVERRIDE;
	virtual void OnUnregister() OVERRIDE;

private:

	void Exec();
};
