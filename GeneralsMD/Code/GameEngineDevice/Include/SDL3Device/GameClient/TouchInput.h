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

/*
** TouchInput.h
**
** GeneralsX @feature Android port 06/09/2026 Native touch input: battlefield
** gestures expressed as what the player meant, resolved by the engine's own rules,
** with no synthesized pointer anywhere.
**
** WHY THIS EXISTS
**
** Until now a touch was turned into a stream of MSG_RAW_MOUSE_POSITION /
** MSG_RAW_MOUSE_*_BUTTON_* messages and handed to the translator chain, which is
** written around a pointer: something that has a position even when nothing is
** pressed, that hovers, that can be parked near a screen edge, and whose button
** releases are guaranteed to arrive. A finger has none of those properties. Every
** control bug reported on this port came out of that gap -- a cursor left sitting on
** a button so the placement ghost drew there, a camera scroll latched by a
** right-button-up that a higher-priority translator legitimately destroyed, an
** ability radius that froze because a pan publishes no positions. Each was fixed
** individually; each fix produced the next report. The premise was wrong, not the
** patches.
**
** WHAT REPLACES IT
**
** The engine's decision-making does NOT require a pointer. It is already reachable
** from a screen point and a world position, and one part of the engine already uses
** it that way: a click on the radar minimap issues real orders through
** evaluateContextCommand() without a single MSG_RAW_MOUSE_* (ControlBarCallback.cpp).
** This module is that same path, for the battlefield:
**
**   TheTacticalView->pickDrawable()          screen point -> object under the finger
**   TheTacticalView->screenToTerrain()       screen point -> world position
**   TheGameClient->evaluateContextCommand()  (object, world) -> the actual order
**   TheInGameUI->selectDrawable() et al      selection
**
** Note especially evaluateContextCommand's EVALUATE_ONLY mode: it answers "would this
** be a legal order" WITHOUT issuing it. That is the real source of truth for target
** feedback, which previously had to be read back out of which cursor bitmap the engine
** had chosen.
**
** WHAT IS DELIBERATELY NOT HERE
**
** Taps on the control bar and menus still go through the ordinary button messages.
** That is not a leftover: a tap on a button IS a click, the window manager's handling
** of it is correct, and nothing about it was ever the problem.
**
** Nothing here invents a message. Everything below MSG_BEGIN_NETWORK_MESSAGES is the
** multiplayer and replay protocol, and the orders this module produces are the same
** MSG_DO_* the mouse path produced, issued by the same engine code.
*/

#pragma once

#include "Lib/BaseType.h"

class Drawable;

namespace TouchInput
{

	/// TRUE while a GUI command (ability, special power) is armed and waiting for a target.
	Bool hasArmedCommand();

	/// TRUE if the current selection can actually be given orders by the local player.
	Bool hasControllableSelection();

	/**
		A tap on the battlefield, in logical display coordinates.

		Resolution order, and why:
		 1. A command armed -> the tap is its target. Nothing else can be meant.
		 2. Something the local player owns under the finger -> select it, UNLESS the
				engine says the current selection has a more specific interaction with it
				than "walk over there" (enter it, repair it, ...), in which case that is
				obviously what was meant. EVALUATE_ONLY answers that; we do not guess.
		 3. Anything else, with a controllable selection -> a context order there.
		 4. Otherwise -> clear the selection.
	*/
	// Spatial callers already resolved detached panels; ordinary touch keeps GUI refusal.
	void tap(Int x, Int y, Bool ignoreScreenUI = FALSE);

	/// Second tap in the same spot: select every unit of that type on screen.
	void doubleTap(Int x, Int y);

	/// A finger has started aiming an armed command: create its radius decal and record
	/// the first aim point. Sending no position messages means nothing else will.
	void beginAiming(Int x, Int y);

	/// Would the armed command accept this point? Does NOT issue it. For target feedback.
	///
	/// There is deliberately no "fire it" counterpart here. An armed command is committed
	/// as a real click by the TARGETING phase, because GUICommandTranslator -- which owns
	/// guard, evacuate and the rest, and which is what clears the mode on completion --
	/// acts on MSG_MOUSE_LEFT_CLICK and nothing else. Dispatching those by hand would mean
	/// reimplementing the engine's own command rules, which is what this module exists to
	/// avoid. The aiming is still native; only the commit is a click.
	Bool armedTargetValid(Int x, Int y);

	/// Tell the control bar that a finger is held at this point, so it can keep the held
	/// button's description alive (ControlBar::update). Routed through here rather than
	/// called directly because ControlBar.h does not compile standalone from the device
	/// layer's include order.
	void reportUiHold(Int x, Int y, Bool held);

	/// Back out of an armed command or a pending building placement. Deselects otherwise.
	/// Returns TRUE when anything was backed out of or cleared (a no-selection,
	/// nothing-armed call is a no-op FALSE) so the BACK key knows whether the
	/// press did something or should fall through to the keyboard (menu).
	Bool cancelOrDeselect();

	/// Back out of an armed command or a pending building placement ONLY.
	/// Unlike cancelOrDeselect(), the selection is never touched. Returns TRUE
	/// when something was backed out of. Quest port: the BACK key (every Quest
	/// controller button arrives as one -- they are indistinguishable) uses this
	/// so a button press backs out of pending states without ever clearing the
	/// selection and without opening the menu; the key is passed through to the
	/// keyboard (menu) only when there is nothing to back out of.
	Bool backOutOfArmedState();

	/// A held finger on the minimap moves the camera there, and nothing else.
	///
	/// The mouse has two answers for a click on the radar and touch had only one. A left
	/// click orders the selected army to that point; a right click just looks at it
	/// (ControlBarCallback.cpp:262). A tap can only be the left one, so with anything
	/// selected there was no way to look at the map at all -- worse, the attempt sent the
	/// army across it. A press-and-hold is the natural second answer, and it is free: the
	/// long-press gesture explicitly ignores points the window manager owns.
	///
	/// Returns TRUE when the point was on the radar and the camera was moved, so the
	/// caller knows to consume the touch instead of replaying it as a click.
	Bool lookAtRadarPoint(Int x, Int y);

}  // namespace TouchInput
