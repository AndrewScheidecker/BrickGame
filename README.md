# Overview

This is a simple game (little modified from the first-person template that comes with UE4), meant to demonstrate the BrickGrid plugin (https://github.com/AndrewScheidecker/BrickGame/tree/master/Plugins/BrickGrid/Source/BrickGrid).

The BrickGrid plugin adds a component that imitates Minecraft's bricks. The bricks are cubes that may contain some material, arranged in a 3D grid.

Most of the functionality is implemented in plugins, so you can use those plugins from your own game project without modifying their source code.

![ScreenShot](https://raw.githubusercontent.com/AndrewScheidecker/BrickGame/master/Screenshot.jpg)

# Building the source code

1. Install UE4.1 (or newer). Thus far I've been using the UE4.1 preview build, built from source.
2. Right click on the BrickGame.uproject, and select "Generate Visual Studio Projects".
3. Open the BrickGame.sln that is created, and run the "Development Editor" configuration of the BrickGame project.

# Keys

Left Mouse Button: Add a brick of the selected material
Right Mouse Button: Remove a brick
Middle Mouse Button: Select the brick material you're aiming at
Mouse wheel: Change selected brick material
L: Toggle flashlight
F6: Save the game
F7: Load the game

# License

Copyright (c) 2014, Andrew Scheidecker
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of BrickGame nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere

Unreal® Engine, Copyright 1998 – 2014, Epic Games, Inc.  All rights reserved.