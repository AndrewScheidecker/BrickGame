// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

#include "ConsoleCommand.h"
#include "ConsoleAPI.generated.inl"

IMPLEMENT_MODULE( IModuleInterface, ConsoleAPI )

UConsoleCommand::UConsoleCommand(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP), Registration(NULL) {}

void UConsoleCommand::PostLoad()
{
	Super::PostLoad();
	RegisterCommand();
}
void UConsoleCommand::BeginDestroy()
{
	Super::BeginDestroy();
	IConsoleManager::Get().UnregisterConsoleObject(Registration);
	Registration = NULL;
}

void UConsoleCommand::RegisterCommand()
{
	check(Registration == NULL);
	if(Command.Len() && !IsTemplate())
	{
		Registration = IConsoleManager::Get().RegisterConsoleCommand(
			*Command,
			*Description,
			FConsoleCommandDelegate::CreateUObject(this,&UConsoleCommand::Exec),
			ECVF_Default
			);
	}
}

void UConsoleCommand::Exec()
{
	OnCommand.Broadcast();
}
