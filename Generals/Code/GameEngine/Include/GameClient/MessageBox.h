/*
**	Command & Conquer Generals(tm)
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

// FILE: MessageBox.h /////////////////////////////////////////////////////////////////////////////
// Author: Chris Huybregts, November 2001
// Description: Message Box file containing user aliases
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameClient/GameWindowManager.h"

GameWindow *MessageBoxYesNo(UnicodeString titleString,UnicodeString bodyString,GameWinMsgBoxFunc yesCallback,GameWinMsgBoxFunc noCallback);  ///< convenience function for displaying a Message box with Yes and No buttons
GameWindow *QuitMessageBoxYesNo(UnicodeString titleString,UnicodeString bodyString,GameWinMsgBoxFunc yesCallback,GameWinMsgBoxFunc noCallback);  ///< convenience function for displaying a Message box with Yes and No buttons


GameWindow *MessageBoxYesNoCancel(UnicodeString titleString,UnicodeString bodyString, GameWinMsgBoxFunc yesCallback, GameWinMsgBoxFunc noCallback, GameWinMsgBoxFunc cancelCallback);///< convenience function for displaying a Message box with Yes,No and Cancel buttons

// GeneralsX @feature Android port 10/07/2026 mirrors the GeneralsMD copy of
// this file -- GameSpyOverlay.cpp (Core/GameEngine, shared) now calls this.
GameWindow *MessageBoxNoButtons(UnicodeString titleString, UnicodeString bodyString, bool bShowLogo);  ///< convenience function for displaying a Message box without any buttons

GameWindow *MessageBoxOkCancel(UnicodeString titleString,UnicodeString bodyString,GameWinMsgBoxFunc okCallback,GameWinMsgBoxFunc cancelCallback);///< convenience function for displaying a Message box with Ok and Cancel buttons

GameWindow *MessageBoxOk(UnicodeString titleString,UnicodeString bodyString,GameWinMsgBoxFunc okCallback);///< convenience function for displaying a Message box with Ok button


GameWindow *MessageBoxCancel(UnicodeString titleString,UnicodeString bodyString,GameWinMsgBoxFunc cancelCallback);///< convenience function for displaying a Message box with Cancel button
