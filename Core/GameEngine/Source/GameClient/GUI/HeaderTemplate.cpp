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

// FILE: HeaderTemplate.cpp /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	Aug 2002
//
//	Filename: 	HeaderTemplate.cpp
//
//	author:		Chris Huybregts
//
//	purpose:	The header template system is used to maintain a unified look across
//						windows.  It also allows Localization to customize the looks based
//						on language fonts.
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#include "PreRTS.h"
#include <cstdio>

#include "Common/INI.h"
#include "Common/FileSystem.h"
#include "Common/Registry.h"
#include "GameClient/HeaderTemplate.h"
#include "GameClient/GameFont.h"
#include "GameClient/GlobalLanguage.h"
//-----------------------------------------------------------------------------
// DEFINES ////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
const FieldParse HeaderTemplateManager::m_headerFieldParseTable[] =
{
	{ "Font",								INI::parseQuotedAsciiString,						nullptr, offsetof( HeaderTemplate, m_fontName ) },
	{ "Point",							INI::parseInt,										nullptr, offsetof( HeaderTemplate, m_point) },
	{ "Bold",								INI::parseBool,										nullptr, offsetof( HeaderTemplate, m_bold ) },
	{ nullptr, nullptr, nullptr, 0 },
};

HeaderTemplateManager *TheHeaderTemplateManager = nullptr;
//-----------------------------------------------------------------------------
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
void INI::parseHeaderTemplateDefinition( INI *ini )
{
	AsciiString name;
	HeaderTemplate *hTemplate;

	// read the name
	const char* c = ini->getNextToken();
	name.set( c );

	// find existing item if present
	hTemplate = TheHeaderTemplateManager->findHeaderTemplate( name );
	if( hTemplate == nullptr )
	{

		// allocate a new item
		hTemplate = TheHeaderTemplateManager->newHeaderTemplate( name );

	}
	else
	{
		DEBUG_CRASH(( "[LINE: %d in '%s'] Duplicate header Template %s found!", ini->getLineNum(), ini->getFilename().str(), name.str() ));
	}
	// parse the ini definition
	ini->initFromINI( hTemplate, TheHeaderTemplateManager->getFieldParse() );

}

HeaderTemplate::HeaderTemplate() :
m_font(nullptr),
m_point(0),
m_bold(FALSE)
{
	m_fontName.clear();
	m_name.clear();
}

HeaderTemplate::~HeaderTemplate(){}

HeaderTemplateManager::HeaderTemplateManager()
{}

HeaderTemplateManager::~HeaderTemplateManager()
{
	HeaderTemplateListIt it = m_headerTemplateList.begin();
	while(it != m_headerTemplateList.end())
	{
		HeaderTemplate *hTemplate = *it;
		delete hTemplate;
		it = m_headerTemplateList.erase(it);

	}
}

void HeaderTemplateManager::init()
{
	{
		AsciiString fname;
		fname.format("Data\\%s\\HeaderTemplate", GetRegistryLanguage().str());

		// GeneralsX @bugfix Android port 09/09/2026 Same guard as GlobalLanguage::init().
		// loadFileDirectory() throws when it reads zero files, and Data\<language>\ only
		// exists for the SKUs EA shipped -- a translation is text, and has no reason to carry
		// header-template configuration. Without this, selecting any unofficial language ends
		// the process during startup. Skipping the load leaves the templates at their
		// defaults, which is what a language with no overrides should get.
		AsciiString fnameWithExt = fname;
		fnameWithExt.concat(".ini");
		INI ini;
		if (TheFileSystem->doesFileExist(fnameWithExt.str()))
		{
			ini.loadFileDirectory( fname, INI_LOAD_OVERWRITE, nullptr );
		}
		else if (TheFileSystem->doesFileExist("Data\\English\\HeaderTemplate.ini"))
		{
			// GeneralsX @bugfix Android port 09/09/2026 Fall back to English, do not just skip.
			//
			// Skipping left the header templates EMPTY, and they are where the menus get their
			// fonts and their header art from -- so selecting Russian gave tiny text and the
			// plain Generals branding instead of Zero Hour's. A language that ships only
			// translated strings wants English's layout, not no layout: the templates are
			// sizes and image names, and a translation has no reason to restate them.
			fprintf(stderr, "[GX-LANG] no %s; using the English header templates\n",
				fnameWithExt.str());
			fflush(stderr);
			ini.loadFileDirectory( AsciiString("Data\\English\\HeaderTemplate"), INI_LOAD_OVERWRITE, nullptr );
		}
	}

	populateGameFonts();
}

HeaderTemplate *HeaderTemplateManager::findHeaderTemplate( AsciiString name )
{
	HeaderTemplateListIt it = m_headerTemplateList.begin();
	while(it != m_headerTemplateList.end())
	{
		HeaderTemplate *hTemplate = *it;
		if(hTemplate->m_name.compare(name) == 0)
			return hTemplate;
		++it;
	}
	return nullptr;
}

HeaderTemplate *HeaderTemplateManager::newHeaderTemplate( AsciiString name )
{
	HeaderTemplate *newHTemplate = NEW HeaderTemplate;
	DEBUG_ASSERTCRASH(newHTemplate, ("Unable to create a new Header Template in HeaderTemplateManager::newHeaderTemplate"));
	if(!newHTemplate)
		return nullptr;

	newHTemplate->m_name = name;
	m_headerTemplateList.push_front(newHTemplate);
	return newHTemplate;

}

GameFont *HeaderTemplateManager::getFontFromTemplate( AsciiString name )
{
	HeaderTemplate *ht = findHeaderTemplate( name );
	if(!ht)
	{
		//DEBUG_LOG(("HeaderTemplateManager::getFontFromTemplate - Could not find header %s", name.str()));
		return nullptr;
	}

	return ht->m_font;
}

HeaderTemplate *HeaderTemplateManager::getFirstHeader()
{
	HeaderTemplateListIt it = m_headerTemplateList.begin();
	if( it == m_headerTemplateList.end())
		return nullptr;

	return *it;
}

HeaderTemplate *HeaderTemplateManager::getNextHeader( HeaderTemplate *ht )
{
	HeaderTemplateListIt it = m_headerTemplateList.begin();
	while(it != m_headerTemplateList.end())
	{
		if(*it == ht)
		{
			++it;
			if( it == m_headerTemplateList.end())
				return nullptr;
			return *it;
		}
		++it;
	}
	return nullptr;

}

void HeaderTemplateManager::onResolutionChanged()
{
	populateGameFonts();
}
//-----------------------------------------------------------------------------
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

void HeaderTemplateManager::populateGameFonts()
{
	HeaderTemplateListIt it = m_headerTemplateList.begin();
	while(it != m_headerTemplateList.end())
	{
		HeaderTemplate *hTemplate = *it;
		Real pointSize = TheGlobalLanguageData->adjustFontSize(hTemplate->m_point);
		GameFont *font = TheFontLibrary->getFont(hTemplate->m_fontName, pointSize,hTemplate->m_bold);
		DEBUG_ASSERTCRASH(font,("HeaderTemplateManager::populateGameFonts - Could not find font %s %d",hTemplate->m_fontName.str(), hTemplate->m_point));

		hTemplate->m_font = font;

		++it;
	}
}
