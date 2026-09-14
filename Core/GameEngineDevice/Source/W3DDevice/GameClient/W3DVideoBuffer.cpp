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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//----------------------------------------------------------------------------
//
// Project:   Generals
//
// Module:    Video
//
// File name: W3DDevice/GameClient/W3DVideoBuffer.cpp
//
// Created:   10/23/01 TR
//
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//         Includes
//----------------------------------------------------------------------------

#include <cstdio>
#include <cstring>

#include "Common/GameMemory.h"
#include "WW3D2/texture.h"
#include "WW3D2/textureloader.h"
#include "W3DDevice/GameClient/W3DVideoBuffer.h"

//----------------------------------------------------------------------------
//         Externals
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Defines
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Types
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Data
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Public Data
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Prototypes
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Functions
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Public Functions
//----------------------------------------------------------------------------


//============================================================================
// W3DVideoBuffer::W3DVideoBuffer
//============================================================================

W3DVideoBuffer::W3DVideoBuffer( VideoBuffer::Type format )
: VideoBuffer(format),
	m_texture(nullptr),
	m_surface(nullptr)
{

}


//============================================================================
// W3DVideoBuffer::SetBuffer
//============================================================================

Bool W3DVideoBuffer::allocate( UnsignedInt width, UnsignedInt height )
{
	free();

	m_width = width;
	m_height = height;
	m_textureWidth = width;
	m_textureHeight = height;
#if !defined(__ANDROID__)
	// Pads to power-of-two for ancient D3D hardware caps. Skipped on Android
	// (DXVK/Vulkan guarantees NPOT): the movie texture was the ONLY per-frame
	// lock/write/draw texture with a big padded tail (800x600 stretched to
	// 1024x1024), and the Mali-G57 driver deterministically segfaults on a
	// worker thread (fault_addr=0x50, mmtrt's build-175/176 logs, issue #9)
	// right when the first movie frame goes through it. An exact-size buffer
	// removes both the padded-garbage region and the size class entirely,
	// and the UV math below (m_width/m_textureWidth) becomes 1:1.
	unsigned int temp_depth=1;
	TextureLoader::Validate_Texture_Size( m_textureWidth, m_textureHeight, temp_depth);
#endif

	WW3DFormat w3dFormat = TypeToW3DFormat(  m_format );

	if ( w3dFormat == WW3D_FORMAT_UNKNOWN )
	{
		return FALSE;
	}

	m_texture  = MSGNEW("TextureClass") TextureClass ( m_textureWidth, m_textureHeight, w3dFormat, MIP_LEVELS_1 );

	if ( m_texture == nullptr )
	{
		return FALSE;
	}

	void *initialPixels = lock();
	if ( initialPixels == nullptr )
	{
		free();
		return FALSE;
	}

	// GeneralsX @bugfix Android port 18/07/2026 issue #9 follow-up: the
	// surface starts out with whatever the allocator left in it, gets
	// marked dirty by this very lock, and is then DRAWN for several frames
	// before the first movie frame is ever blitted in -- uploading and
	// displaying uninitialized memory. Zero it (black) while it's locked.
	memset( initialPixels, 0, (size_t)m_pitch * m_textureHeight );

	fprintf(stderr, "[GX-VIDBUF] allocate visible=%ux%u texture=%ux%u pitch=%u format=%d\n",
		m_width, m_height, m_textureWidth, m_textureHeight, m_pitch, (int)m_format);
	fflush(stderr);

	unlock();


	return TRUE;
}

//============================================================================
// W3DVideoBuffer::~W3DVideoBuffer
//============================================================================

W3DVideoBuffer::~W3DVideoBuffer()
{
	free();
}

//============================================================================
// W3DVideoBuffer::lock
//============================================================================

void*		W3DVideoBuffer::lock()
{
	void *mem = nullptr;

	if ( m_surface != nullptr )
	{
		unlock();
	}

	m_surface = m_texture->Get_Surface_Level();

	if ( m_surface )
	{
		mem = m_surface->Lock( (Int*) &m_pitch );
	}

	return mem;
}

//============================================================================
// W3DVideoBuffer::unlock
//============================================================================

void		W3DVideoBuffer::unlock()
{
	if ( m_surface != nullptr )
	{
		m_surface->Unlock();
		m_surface->Release_Ref();
		m_surface = nullptr;
	}
}

//============================================================================
// W3DVideoBuffer::valid
//============================================================================

Bool		W3DVideoBuffer::valid()
{
	return m_texture != nullptr;
}

//============================================================================
// W3DVideoBuffer::reset
//============================================================================

void	W3DVideoBuffer::free()
{
	unlock();

	if ( m_texture )
	{
		unlock();
		m_texture->Release_Ref();
		m_texture = nullptr;
	}
	m_surface = nullptr;

	VideoBuffer::free();
}


//============================================================================
// W3DVideoBuffer::TypeToW3DFormat
//============================================================================

WW3DFormat W3DVideoBuffer::TypeToW3DFormat( VideoBuffer::Type format )
{
	WW3DFormat w3dFormat = WW3D_FORMAT_UNKNOWN;
	switch ( format )
	{
		case TYPE_X8R8G8B8:
			w3dFormat = WW3D_FORMAT_X8R8G8B8;
			break;

 		case TYPE_R8G8B8:
			w3dFormat = WW3D_FORMAT_R8G8B8;
			break;

 		case TYPE_R5G6B5:
			w3dFormat = WW3D_FORMAT_R5G6B5;
			break;

 		case TYPE_X1R5G5B5:
			w3dFormat = WW3D_FORMAT_X1R5G5B5;
			break;
	}

	return w3dFormat;
}

//============================================================================
// W3DFormatToType
//============================================================================

VideoBuffer::Type W3DVideoBuffer::W3DFormatToType( WW3DFormat w3dFormat )
{
	Type format = TYPE_UNKNOWN;
	switch ( w3dFormat )
	{
		case WW3D_FORMAT_X8R8G8B8:
				format = VideoBuffer::TYPE_X8R8G8B8;
				break;
		case WW3D_FORMAT_R8G8B8:
				format = VideoBuffer::TYPE_R8G8B8;
				break;
		case WW3D_FORMAT_R5G6B5:
				format = VideoBuffer::TYPE_R5G6B5;
				break;
		case WW3D_FORMAT_X1R5G5B5:
				format = VideoBuffer::TYPE_X1R5G5B5;
				break;
	}

	return format;
}
