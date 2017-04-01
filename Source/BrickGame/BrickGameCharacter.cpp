// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BrickGame.h"
#include "BrickGameCharacter.h"

//////////////////////////////////////////////////////////////////////////
// ABrickGameCharacter

ABrickGameCharacter::ABrickGameCharacter(const class FObjectInitializer& Initializer)
	: Super(Initializer)
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = Initializer.CreateDefaultSubobject<UCameraComponent>(this, TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->AttachToComponent(GetCapsuleComponent(),FAttachmentTransformRules::KeepRelativeTransform);
	FirstPersonCameraComponent->RelativeLocation = FVector(0, 0, 64.f); // Position the camera
}

//////////////////////////////////////////////////////////////////////////
// Input

void ABrickGameCharacter::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	// set up gameplay key bindings
	check(InputComponent);

	InputComponent->BindAxis("MoveForward", this, &ABrickGameCharacter::MoveForward);
	InputComponent->BindAxis("MoveRight", this, &ABrickGameCharacter::MoveRight);
	InputComponent->BindAxis("MoveUp", this, &ABrickGameCharacter::MoveUp);
	
	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	InputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	InputComponent->BindAxis("TurnRate", this, &ABrickGameCharacter::TurnAtRate);
	InputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	InputComponent->BindAxis("LookUpRate", this, &ABrickGameCharacter::LookUpAtRate);
}

void ABrickGameCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// find out which way is forward
		const FRotator Rotation = GetControlRotation();

		// Get forward vector
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::X);

		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

void ABrickGameCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// find out which way is right
		const FRotator Rotation = GetControlRotation();

		// Get right vector
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y);

		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

void ABrickGameCharacter::MoveUp(float Value)
{
	if(Value != 0.0f)
	{
		AddMovementInput(FVector(0,0,1), Value);
	}
}

void ABrickGameCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ABrickGameCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}
