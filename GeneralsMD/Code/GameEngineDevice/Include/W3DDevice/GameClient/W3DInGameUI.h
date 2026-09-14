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

// FILE: W3DInGameUI.h ////////////////////////////////////////////////////////
//
// W3D implementation of the in game user interface, the in game ui is
// responsible for
//
// Author: Colin Day, April 2001
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

// SYSTEM INCLUDES ////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "GameClient/InGameUI.h"
#include "GameClient/View.h"
#include "W3DDevice/GameClient/W3DView.h"
#include "WW3D2/render2d.h"
#include "WW3D2/rendobj.h"
#include "WW3D2/line3d.h"

class HAnimClass;

// W3DInGameUI ----------------------------------------------------------------
/** Implementation for the W3D game user interface.  This singleton is
  * responsible for all user interaction and feedback with as part of the
	* user interface */
//-----------------------------------------------------------------------------
class W3DInGameUI : public InGameUI
{

public:

	W3DInGameUI();
	virtual ~W3DInGameUI() override;

	// Inherited from subsystem interface -----------------------------------------------------------
	virtual	void init() override;		///< Initialize the in-game user interface
	virtual void update() override;	//< Update the UI by calling preDraw(), draw(), and postDraw()
	virtual void reset() override;		///< Reset
	//-----------------------------------------------------------------------------------------------

	virtual void draw() override; ///< Render the in-game user interface
#ifdef __ANDROID__
	// GeneralsX @feature Codex 14/09/2026 Rotate only the native preview;
	// the original placement translator validates and commits its angle.
	bool rotateXrPlacement(float radians);
	// GeneralsX @feature Codex 13/09/2026 Actual command-stream feedback,
	// render-only and XR-gated; never a synthetic attack or simulation change.
	void createAttackHint(const GameMessage *msg) override;
	void createForceAttackHint(const GameMessage *msg) override;
#endif

protected:

	/// factory for views
	virtual View *createView(bool dummy) override
	{
		if (dummy)
			return NEW ViewDummy;
		return NEW W3DView;
	}

	virtual void drawSelectionRegion();			///< draw the selection region on screen
	virtual void drawMoveHints( View *view );			///< draw move hint visual feedback
	virtual void drawAttackHints( View *view );		///< draw attack hint visual feedback
	virtual void drawPlaceAngle( View *view ); 		///< draw place building angle if needed

	RenderObjClass *m_moveHintRenderObj[ MAX_MOVE_HINTS ];
	HAnimClass		 *m_moveHintAnim[ MAX_MOVE_HINTS ];
	RenderObjClass *m_buildingPlacementAnchor;
	RenderObjClass *m_buildingPlacementArrow;
#ifdef __ANDROID__
	Line3DClass *m_xrAttackRing[16]={};
	Coord3D m_xrAttackPosition={};
	Int m_xrAttackFrame=-1000;
	void clearXrAttackHint();
#endif

};
