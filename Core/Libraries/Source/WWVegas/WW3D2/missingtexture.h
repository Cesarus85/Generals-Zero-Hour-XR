/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "always.h"

struct IDirect3DTexture8;
struct IDirect3DSurface8;

class MissingTexture
{
public:
	static void _Init();
	static void _Deinit();

	static IDirect3DTexture8* _Get_Missing_Texture();		// Return a reference to missing texture
	// GeneralsX @diag Android port Called from the places that actually
	// SUBSTITUTE the magenta placeholder for a texture that failed to load,
	// naming the file and why. Deliberately not called from
	// _Get_Missing_Texture(): TextureBaseClass::Is_Missing_Texture() calls
	// that merely to compare pointers, so counting there counts queries, not
	// failures, and inflates the number that is supposed to say how much of
	// the scene is broken.
	static void _Note_Substitution(const char* reason, const char* name);
	static IDirect3DSurface8* _Create_Missing_Surface();	// Create new surface which contain missing texture image
};
