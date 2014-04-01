# Overview

This is a simple game (little modified from the first-person template that comes with UE4), meant to demonstrate the BrickGrid plugin (https://github.com/AndrewScheidecker/BrickGame/tree/master/Plugins/BrickGrid/Source/BrickGrid).

BrickGrid adds a PrimitiveComponent class that reproduces Minecraft-style voxel rendering. The grid's "bricks" are cubes that may contain some material.

Rendering works, and collision works (in a flaky way), but both are not set up for real-time updates to subsets of the bricks. The current code will recreate the rendering resources for the entire grid if only a single voxel changes, so it isn't quite capable of allowing players to modify it during gameplay.

All of the procedural terrain generation happens in BluePrint, but it uses a simplex noise function implemented in C++ (https://github.com/AndrewScheidecker/BrickGame/tree/master/Plugins/SimplexNoise).

The BrickGrid, SuperLoop, and SimplexNoise plugins don't have any dependencies apart from built-in UE4 modules, so it should be easy to drop them into your own game individually or as a group.

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