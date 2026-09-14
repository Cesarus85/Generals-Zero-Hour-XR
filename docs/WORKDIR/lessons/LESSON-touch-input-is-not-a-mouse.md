# Lesson: touch input is not a mouse

**Date:** 06/09/2026
**Applies to:** Android and iOS builds of the Zero Hour port
**Code:** `GeneralsMD/Code/GameEngineDevice/{Include,Source}/SDL3Device/GameClient/TouchInput.{h,cpp}`,
`GeneralsMD/Code/GameEngineDevice/Source/SDL3GameEngine.cpp`

---

## The short version

For a long time this port turned every touch into a stream of
`MSG_RAW_MOUSE_*` messages and fed them to the engine's translator chain.
It seemed obviously right: the engine is mouse-driven, so give it a mouse.
It produced a year of control bugs that each looked unrelated and each got
its own fix, and the fixes kept generating the next report.

They were one bug. **A pointer has properties a finger does not have**, and
the translator chain depends on every one of them:

| A pointer… | A finger… |
|---|---|
| has a position when nothing is pressed | exists only while touching |
| hovers — it can be *at* something without acting on it | cannot |
| can rest near a screen edge indefinitely | cannot rest anywhere |
| always sends a release for every press | can be cancelled by the OS mid-gesture |
| is one thing | can be two things at once |

Every control bug on this port traces to one of those five rows. Some
examples, all real:

- The **placement ghost drew on the button that armed it** — because the
  synthesized cursor was left wherever the last touch happened, and
  `handleBuildPlacements()` reads "the mouse position" each frame. Row 1.
- The **camera scrolled on its own until the pause menu was opened** — a
  `SCROLL_RMB` started by a right-button-down whose matching up was
  legitimately destroyed by a higher-priority translator (`SelectionXlat`
  consumes it to cancel a GUI command). Row 4.
- **Screen-edge scrolling latched forever** — it begins when a pointer is
  within 3px of an edge and ends only when a *later pointer event* reports a
  position back inside the safe zone. Rows 1 and 3. No amount of guarding the
  entry and exit fixes a mode whose exit condition cannot occur.
- The **ability radius froze mid-drag** — panning publishes no position
  messages at all, so the hint that positions the decal was never recomputed.
  Row 1.
- **Descriptions could not be read by holding a button** — the engine
  deliberately *suppresses* tooltips while a button is held
  (`winProcessMouseEvent` wipes the tooltip text and returns from the grab
  branch before reaching the tooltip block). Tooltips are a hover affordance,
  and a press is not a hover. Row 2.

Each of these was patched individually first. Every patch worked and every
patch was followed by a new report, because the premise was wrong rather than
the patches.

---

## What the engine actually requires

Here is the part that makes the fix small: **the engine's decision-making
never needed a pointer.** It is already reachable from a screen point and a
world position, and it is already used that way by code that has nothing to
do with the mouse — a click on the radar minimap issues real orders through
this path without a single `MSG_RAW_MOUSE_*`
(`Core/GameEngine/Source/GameClient/GUI/GUICallbacks/ControlBarCallback.cpp`).

The whole native input surface is four calls:

```cpp
TheTacticalView->pickDrawable(&screenPoint, forceAttack, pickType);   // what is under the finger
TheTacticalView->screenToTerrain(&screenPoint, &worldPos);            // where on the map that is
TheGameClient->evaluateContextCommand(draw, &worldPos, cmdType);      // what order that means
TheInGameUI->selectDrawable(draw);                                    // selection
```

`evaluateContextCommand`'s third argument is the interesting one:

- `CommandTranslator::DO_COMMAND` — issue the order.
- `CommandTranslator::EVALUATE_ONLY` — return the order that *would* be
  issued, **without issuing it**.

`EVALUATE_ONLY` is the engine answering questions about itself, and it
replaces two things that used to be inferred badly:

1. **Target validity.** The old code read the answer back out of *which
   cursor bitmap the engine had chosen* (valid art vs. invalid art), because
   on a mouse the cursor is the only place that decision surfaces. Asking
   directly means the on-screen feedback cannot disagree with what releasing
   actually does — it is literally the same evaluation.
2. **The one genuinely ambiguous tap.** Tapping something you own, while
   something else is selected: select it, or enter/repair it? On a mouse
   those are different buttons. We do not guess — if `EVALUATE_ONLY` returns
   a plain move, the tap carries no special meaning and is a selection;
   anything else means the engine has already decided the two objects
   interact.

---

## The layering, and the one line you must not cross

```
  SDL touch events
        │
        ▼
  SDL3GameEngine.cpp   gesture state machine (IDLE/PENDING/PANNING/TARGETING/…)
        │                       │
        │                       └── camera pan & zoom → TheTacticalView directly
        ▼
  TouchInput.cpp       what the gesture MEANS, resolved by the engine's own rules
        │
        ▼
  MSG_DO_MOVETO, MSG_DO_ATTACK_OBJECT, MSG_DO_SPECIAL_POWER_AT_LOCATION, …
```

**Everything in the range `MSG_BEGIN_NETWORK_MESSAGES` (1000) …
`MSG_END_NETWORK_MESSAGES` (1999) is the multiplayer and replay protocol.**
`GameClientDispatch.cpp` destroys everything outside it. Native touch input
produces *exactly* the same `MSG_DO_*` messages the mouse path produced,
issued by the same engine code — that is what makes it safe. If you ever find
yourself wanting to invent a message for a gesture, stop: the gesture is not
finished being translated into an existing one.

---

## What deliberately still uses clicks

Not everything should be native, and the distinction is not "how much work
was it":

| Path | Why |
|---|---|
| Taps on the control bar and menus | A tap on a button **is** a click. The window manager's handling of it is correct and was never the problem. |
| Building placement | Not a context order at all — `PlaceEventTranslator` is its own press/drag/release state machine with the angle and line-build rules in it. A press-and-release on a spot is exactly what a click faithfully represents. |
| Selection box | Works, and `SelectionXlat`'s drag-lock state has no clean external entry point. |

**The window manager gets first refusal on every battlefield tap**, before
native resolution:

```cpp
TheWindowManager->winProcessMouseEvent(GWM_LEFT_DOWN, &point, nullptr);
TheWindowManager->winProcessMouseEvent(GWM_LEFT_UP,   &point, nullptr);
// if either returned WIN_INPUT_USED, the UI took it — do not also act on the map
```

This is not a hack: it is the same question `WindowXlat` (priority 10) asks
on the engine's behalf, asked directly with no message and no phantom
pointer. Skipping it is what let a tap inside the generals-powers panel close
the panel **and** set a rally point on the ground underneath.

---

## Two patterns you will need again

### 1. Anything that previews "where the player is pointing" must be told, not asked

`handleRadiusCursor()` and `handleBuildPlacements()` both read
`TheMouse->getMouseStatus()->pos` every frame. On a touchscreen that value is
a leftover from the last gesture, not an answer. Both now read an explicit
aim point that the touch layer reports (`InGameUI::setTouchAimPoint`), and
both draw **nothing** until `m_touchAimKnown` — arming an ability or picking
a building clears it, so no preview can appear at the previous attempt's
target.

If you add another preview, do the same. Do not read the mouse.

### 2. Engine state that a mouse would refresh constantly must be re-asserted

Two symptoms came from the same shape:

- The ability radius decal is created once and then destroyed by any of
  **seven** `setRadiusCursorNone()` call sites (`createCommandHint()` calls it
  unconditionally on every hint; `ControlBar::switchToContext()` calls it
  whenever the selection context is rebuilt).
- The held description is not hidden but **deleted** —
  `ControlBarPopupDescriptionUpdateFunc()` calls
  `deleteBuildTooltipLayout()` on any frame where
  `getShowBuildTooltipLayout()` is false, and that flag is cleared at the
  bottom of every `ControlBar::update()`.

On the mouse path both are harmless, because the next mouse position
recreates them a frame later. Native input sends no such stream, so the first
one to fire wins permanently.

**The fix in both cases is to re-assert the invariant once a frame rather
than to hunt the culprit**: while a command is armed and a finger is aiming
it, its radius decal exists; while a finger is held on a command button, the
show flag is true. Cheap (only reached on the frame after something cleared
it), and immune to an eighth call site appearing later.

A related trap: the *first* attempt at the tooltip fix called
`showBuildTooltipLayout(nullptr)` every frame to reset that static state
machine — which also resets its wait timer, so the tooltip delay could never
elapse and the description could never come back at all. A recovery that runs
every frame is not a recovery.

### 3. Widget pointers do not survive; points do

The control bar rebuilds its command windows whenever the selection context
changes, which can happen *while a finger is held on one of them*. Anything
keyed on the `GameWindow*` captured at touch-down, or on that window's
`WIN_STATE_SELECTED`, silently stops matching. The touch layer therefore
reports the **hold point** (`ControlBar::setTouchHoldPoint`), and the control
bar hit-tests it again each frame.

---

## How to add a new gesture

1. Add the phase to `TouchState::Phase` in `SDL3GameEngine.cpp` and classify
   into it in the `SDL_EVENT_FINGER_DOWN` handler. Classify on **state**, not
   on what the finger might do next, wherever state removes the ambiguity —
   e.g. an armed GUI command means a battlefield touch can only be aiming, so
   `TARGETING` is entered on that fact and never has to guess. `PENDING`
   exists for the genuinely ambiguous case and defers all output until intent
   is clear.
2. Decide what the gesture *means* and put that in `TouchInput`, expressed
   through the four calls above. If you cannot express it that way, check
   whether it belongs on the click path (see the table above) before writing
   anything new.
3. Do not publish `MSG_RAW_MOUSE_POSITION` for it. That message drives GUI
   hilite, the selection anchor and the edge-scroll latch; publishing one
   without a finger behind it is the origin of most of this document.
4. Handle `SDL_EVENT_FINGER_CANCELED` explicitly. The OS takes the finger
   away (a call, the notification shade, palm rejection) with no release, and
   the safe answer is almost always "commit nothing".
5. Test with the overlay on (Setup → Diagnostics → **Touch input overlay**),
   which draws the gesture phase, the finger, the engine's pointer position
   and the camera scroll anchor at once, and writes `[gxtouch]` lines on every
   phase or camera-mode change. Those four values agreeing is the normal case;
   their disagreement is every bug in this document.

---

## The general lesson

When a platform lacks a capability the code assumes, there are two moves:
**emulate the capability**, or **find the layer below it that never needed
the capability in the first place**.

Emulation is faster to start and it reuses the rules for free — which is why
it was the right call to get this port playable at all. But it is only ever
as good as the fidelity of the emulation, and the gap shows up as a stream of
individually plausible bugs that resist individual fixes.

The engine here had a seam the whole time: game logic that speaks in world
positions and object IDs, sitting under an input layer that speaks in
pointers. Finding that seam turned an open-ended stream of control bugs into
one ~300-line module. Before writing the fifth patch for a class of bug, it
is worth asking whether the thing being patched should exist.
