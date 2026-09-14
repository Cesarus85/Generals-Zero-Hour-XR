# Touch parity matrix

## What this is

This document answers one question: what can a player do with a mouse (plus keyboard
modifiers) in *Command & Conquer: Generals Zero Hour* that the Android port's native touch
implementation currently cannot do at all, or does differently? It exists as input to the
design of a full on-screen control scheme intended to replace the mouse outright, so it
aims at completeness rather than brevity — including capabilities that are arguably niche,
and including ones that are already covered, so a reader can see the whole surface at once.

It was produced by walking the message-stream translator chain in priority order
(WindowTranslator 10, MetaEventTranslator 20, HotKeyTranslator 25, PlaceEventTranslator 30,
GUICommandTranslator 40, SelectionTranslator 50, LookAtTranslator 60, CommandTranslator 70,
HintSpyTranslator 100), reading every `MSG_RAW_MOUSE_*`, `MSG_MOUSE_*` and `MSG_META_*` case
and every place a keyboard modifier changes what a click means, then reading the touch code
that would have to provide the equivalent — `SDL3GameEngine.cpp`'s `handleTouchEvent()`
gesture state machine and `TouchInput.cpp`/`TouchInput.h`. Nothing here is inferred from
behaviour; every row cites the line that implements it. Where evidence could not be found in
this repository, the cell says "not found" rather than guessing.

Two notes on sourcing. First, the engine files listed above exist only in `Core/`; there is
no `GeneralsMD/` copy of any translator, so the `Core/` copy is what ships on Android
(`TouchInput.cpp`, by contrast, is a `GeneralsMD/` device-layer file). Second, the actual
key that produces a given `MSG_META_*` is data, not code: it comes from `CommandMap.ini`
inside the game's `.big` archives, which is **not in this repository (not found)**. Rows
therefore name the engine mode or message rather than a physical key, except where the
modifier is read directly from the message's modifier argument.

Paths are relative to the repository root. `SDL3GameEngine.cpp` means
`GeneralsMD/Code/GameEngineDevice/Source/SDL3GameEngine.cpp`; `TouchInput.cpp`/`.h` mean the
files under `GeneralsMD/Code/GameEngineDevice/{Source,Include}/SDL3Device/GameClient/`;
unqualified translator and GUI files live under `Core/GameEngine/Source/GameClient/`.

Status values: **COVERED** (a touch gesture reaches the same engine code), **PARTIAL** (the
capability exists but is narrower, less precise, or reachable only by a detour),
**MISSING** (no touch input path reaches it at all), **N-A-ON-TOUCH** (deliberately removed
because the concept does not exist without a pointer).

---

## Parity matrix

| Capability | Mouse gesture (incl. modifier) | What it does | Engine entry point | Touch status | Proposed touch gesture | Priority |
|---|---|---|---|---|---|---|
| Select one object | Left click on it | Replaces the selection with that object and plays its voice/select sound | `SelectionXlat.cpp:737` `onMouseLeftClick`, group creation at `SelectionXlat.cpp:900` | COVERED | Single tap (`SDL3GameEngine.cpp:1289`, `TouchInput.cpp:258`–`275`; sound choice `TouchInput.cpp:102`–`119`) | — |
| Issue a context order (move / attack / enter / repair / capture / rally point …) | Left click on ground or on a target, default mouse setup | Runs `evaluateContextCommand` in `DO_COMMAND` and appends the resulting `MSG_DO_*` | `CommandXlat.cpp:3999`–`4056`; evaluator `CommandXlat.cpp:1551`; rally-point branch `CommandXlat.cpp:2336`–`2355` | COVERED | Single tap (`TouchInput.cpp:282`–`286`, `TouchInput.cpp:156`–`168`) | — |
| Disambiguate "select my unit" vs. "interact with my unit" | Left click on own unit while a compatible unit is selected | Engine decides between select and enter/repair/etc. | `CommandXlat.cpp:1551` (the whole evaluator) | COVERED | Tap asks the evaluator in `EVALUATE_ONLY` mode first (`TouchInput.cpp:137`–`154`, used at `TouchInput.cpp:266`–`275`) | — |
| Deselect everything | Right click on empty ground (default mouse setup) | `deselectAll()` | `SelectionXlat.cpp:1148`–`1152` | COVERED | Press-and-hold ≥600 ms then release (`SDL3GameEngine.cpp:1157`–`1176`) or two-finger tap (`SDL3GameEngine.cpp:1329`–`1338`), both into `TouchInput::cancelOrDeselect` (`TouchInput.cpp:397`–`419`) | — |
| Cancel an armed GUI command without losing the selection | Right click | Clears `getGUICommand()`, keeps units selected | `SelectionXlat.cpp:1105`–`1130` | COVERED | Same two gestures; `TouchInput.cpp:405`–`409`. Also a long press while already aiming (`SDL3GameEngine.cpp:1409`–`1414`) | — |
| Cancel a pending building placement | Right click | `placeBuildAvailable(nullptr, nullptr)` | `SelectionXlat.cpp:1134`–`1146`, also `CommandXlat.cpp:3879`–`3898` | COVERED | Two-finger tap (`TouchInput.cpp:411`–`415`); a second finger during the rotate drag also cancels (`SDL3GameEngine.cpp:969`–`984`). Long press is deliberately suppressed during placement (`SDL3GameEngine.cpp:1156`) | — |
| Area (box) select | Left press, drag, release on the battlefield | Builds a pixel region, selects everything in it | anchor `SelectionXlat.cpp:1026`–`1033`, box growth `SelectionXlat.cpp:551`–`580`, resolution `SelectionXlat.cpp:1039`–`1054` | COVERED | Hold ≥250 ms, then drag (`SDL3GameEngine.cpp:1041`–`1078`, release `SDL3GameEngine.cpp:1376`–`1388`) | — |
| Camera pan | Right button held and dragged (`SCROLL_RMB`) | Continuous camera scroll | `LookAtXlat.cpp:274`–`290`, applied at `LookAtXlat.cpp:455`+ | COVERED | One-finger drag inside 250 ms (`SDL3GameEngine.cpp:1079`–`1088`), applied by ground projection (`SDL3GameEngine.cpp:477`–`566`); two-finger centroid drag also pans (`SDL3GameEngine.cpp:1650`–`1664`) | — |
| Camera pan with inertia | — (no mouse equivalent) | — | — | Touch-only addition | Fast release of a one-finger pan coasts (`SDL3GameEngine.cpp:1302`–`1318`, `SDL3GameEngine.cpp:1665`–`1682`) | — |
| Camera zoom | Mouse wheel | `userZoom` by `View::ZoomHeightPerSecond` per tick | `LookAtXlat.cpp:433`–`441` | COVERED | Pinch; calibrated against the same constant (`SDL3GameEngine.cpp:468`, `SDL3GameEngine.cpp:572`–`586`) | — |
| Camera scroll at screen edge | Pointer parked within `edgeScrollSize` of an edge | Continuous scroll until the pointer returns to the safe zone | `LookAtXlat.cpp:364`–`380` | N-A-ON-TOUCH — a finger rests nowhere, so the mode's exit condition cannot occur; disabled at the source on touch platforms (`LookAtXlat.cpp:115`–`121`) | None needed; one-finger drag is the replacement | — |
| **Camera rotate** | Middle button held, drag horizontally | `userSetAngle` proportional to horizontal travel; snaps to 45° while force-attack is held | down `LookAtXlat.cpp:303`–`316`, rotate `LookAtXlat.cpp:390`–`400` | **MISSING** — the touch layer never emits `MSG_RAW_MOUSE_MIDDLE_*` and never calls `userSetAngle` (no match in `SDL3GameEngine.cpp`) | Two-finger twist. The `TWOFINGER` phase currently consumes only centroid translation and inter-finger distance (`SDL3GameEngine.cpp:1650`–`1664`), so the rotation component of the same two fingers is free and needs no new gesture. Alternative: on-screen rotate-left/rotate-right buttons feeding `MSG_META_ALT_CAMERA_ROTATE_*` (`CommandXlat.cpp:3406`, `CommandXlat.cpp:3418`) | High — the camera angle is fixed for the whole match, which no other RTS on the platform does |
| **Camera reset (angle, pitch, zoom, pivot)** | Middle click, short and without moving | `userResetPivotToGround` + set angle/pitch/zoom to default | `LookAtXlat.cpp:318`–`345` (conditions at `LookAtXlat.cpp:336`–`341`) | **MISSING** — no middle-button path on touch; `MSG_META_CAMERA_RESET` (`CommandXlat.cpp:3438`) is keyboard-only | An on-screen button, or a two-finger double tap (currently free — the two-finger release only tests for a single tap, `SDL3GameEngine.cpp:1330`–`1337`) | High if twist-rotate ships — a rotate gesture without a reset strands players at odd angles; Low otherwise |
| **Force attack / force fire** | Ctrl held while clicking (mode `isInForceAttackMode`) | Routes the click to `evaluateForceAttack` instead of the context evaluator: attack an object you would otherwise not target, or `MSG_DO_FORCE_ATTACK_GROUND` at a bare position | mode set `CommandXlat.cpp:3716`–`3723` and `SelectionXlat.cpp:529`–`541`; used `CommandXlat.cpp:3963`–`3966` and `CommandXlat.cpp:4044`–`4047`; evaluator `CommandXlat.cpp:1465`, ground branch `CommandXlat.cpp:1512`–`1533` | **MISSING** — the modes are only ever set from `MSG_META_*` (the only two call sites above), which only `MetaEventTranslator` produces from key events (`MetaEvent.cpp:572`–`605`); touch produces no key events. `TouchInput::pickForOrder` already consults `isInForceAttackMode()` for pick types (`TouchInput.cpp:60`–`62`), but `issueContextOrder` calls only `evaluateContextCommand` and has an explicit comment saying force attack is not handled (`TouchInput.cpp:162`–`168`) | A latching on-screen "Force fire" toggle that (a) sends `MSG_META_BEGIN_FORCEATTACK`/`END`, and (b) makes `TouchInput::issueContextOrder` branch to `evaluateForceAttack`, mirroring `CommandXlat.cpp:3963`. A toggle alone is not enough — the second half is the part that is missing. Conflicts: none; the tap gesture is unchanged, only its evaluator. Note before estimating: `TheGameClient` exposes only `evaluateContextCommand` (`GeneralsMD/Code/GameEngine/Include/GameClient/GameClient.h:113`); `evaluateForceAttack` lives on `CommandTranslator` alone (`CommandXlat.h:45`), so the second half needs either a matching `GameClient` wrapper or a direct route to the translator — `TouchInput` cannot reach it the way it reaches the context evaluator today | High — without it a player cannot shell a bridge, a wall, a neutral structure, garrisoned civilian buildings, or their own units, which is a large part of the game's tactical vocabulary |
| **Force move / crush** | Alt held while clicking (mode `isInForceMoveToMode`) | Nulls the picked object so the click is treated positionally, producing `MSG_DO_FORCEMOVETO`, and lets a vehicle deliberately crush what is in the way | mode `CommandXlat.cpp:3681`–`3689`; used `CommandXlat.cpp:1579`–`1583` and `CommandXlat.cpp:1010`–`1013`; crush voice `CommandXlat.cpp:567`–`576` | **MISSING** — same reason as force attack; nothing on touch sets the mode | Second state of the same on-screen modifier control (a three-way "normal / force fire / force move" toggle), or a separate latching button. No gesture conflict | Medium — deliberate crushing and "walk exactly here, do not interact" both become impossible, but both have clumsy workarounds |
| **Waypoint mode (queued move orders)** | Waypoint modifier held while clicking repeatedly (mode `isInWaypointMode`) | Each click appends `MSG_ADD_WAYPOINT` instead of replacing the order; the evaluator short-circuits every other interpretation | mode `CommandXlat.cpp:3693`–`3712`; used `CommandXlat.cpp:1002`–`1005`, `CommandXlat.cpp:1614`–`1633`, `CommandXlat.cpp:2435`; move-sound suppression `CommandXlat.cpp:551`–`558` | **MISSING** — nothing on touch sets the mode. Note the control-bar route is a dead end on every platform: `GUI_COMMAND_WAYPOINTS` is an empty case (`ControlBarCommandProcessing.cpp:667`–`668`) | A latching on-screen "Waypoint" toggle sending `MSG_META_BEGIN_WAYPOINTS` and, on untoggle, `MSG_META_END_WAYPOINTS`. While latched, each tap queues. Conflicts: none with existing gestures, but the toggle must clear itself on deselect or the player will strand themselves in it — the same failure mode already reported for armed targeting (`SDL3GameEngine.cpp:1391`–`1414`) | High — patrol routes and multi-leg movement are unavailable, and this is one of the few things with no substitute at all |
| **Add to selection / keep old selection** | Prefer-selection modifier (Shift) held while clicking or box-dragging | `addToGroup` stays true, so the new drawables are appended instead of replacing | `SelectionXlat.cpp:779`; double-click variant preserves the previous list `SelectionXlat.cpp:653`, restored `SelectionXlat.cpp:670`–`682` | **MISSING** — no touch path sets `PreferSelectionMode` (only `CommandXlat.cpp:3700`/`3705`), so every tap and every box drag replaces the selection outright | A latching on-screen "Add" toggle sending `MSG_META_BEGIN_PREFER_SELECTION`. Alternative without a mode: a double-tap-and-hold second tap, currently free (the tap counter resets after a completed double tap, `SDL3GameEngine.cpp:1294`–`1299`) | High — composing a mixed army from several taps is a basic RTS action and there is no workaround short of control groups |
| **Remove from selection** | Prefer-selection modifier + click on an already-selected unit | Sends `MSG_REMOVE_FROM_SELECTED_GROUP` and deselects those drawables | `SelectionXlat.cpp:867`–`891` | **MISSING** — depends on the same mode | Falls out of the "Add" toggle above for free: the engine branch already keys on prefer-selection plus "all clicked are already selected" | Medium — pruning a selection matters most in late-game mixed armies |
| **Select all of a type across the whole map** | Alt held during a double click | `selectMatchingAcrossMap()` instead of `selectMatchingAcrossScreen()` | modifier read `SelectionXlat.cpp:629`, branch `SelectionXlat.cpp:658`–`661`, implementation `GeneralsMD/.../InGameUI.cpp:5468` | **MISSING** — the touch double tap always calls the across-screen variant (`TouchInput.cpp:364`) | Triple tap, or a "whole map" state of the Add toggle above; the across-map call is a one-line swap in `TouchInput::doubleTap`. Conflicts: the double-tap counter currently refuses to chain (`SDL3GameEngine.cpp:1294`–`1299`), so a triple tap needs that reset relaxed | Low — the on-screen "select all units of this type" command button (`ControlBarCommandProcessing.cpp:634`–`663`) already covers most of the need |
| **Minimap: jump the camera** | Right click on the radar (default mouse setup) | `userLookAt` at the radar's world position, without ordering anything | `ControlBarCallback.cpp:262`–`269` | **MISSING when anything is selected.** A touch on the radar goes down the deferred path and its release is replayed to the window manager as a left down/up pair (`SDL3GameEngine.cpp:1230`–`1248`), which reaches `GWM_LEFT_DOWN` only. With an empty selection that still look-ats (`ControlBarCallback.cpp:262`, first clause), but with units selected the same tap issues a move order (`ControlBarCallback.cpp:300`–`305`) | **DONE 08/09/2026** — implemented as a long press on the radar, resolved as a direct `userLookAt` (`TouchInput.cpp` `lookAtRadarPoint`, wired at the head of the PENDING release path in `SDL3GameEngine.cpp` so neither the deselect branch nor the window-manager replay can claim it). Original note: It is free: the long-press cancel branch explicitly excludes points the window manager owns (`SDL3GameEngine.cpp:1157`), so the radar currently has no hold behaviour at all. Implement by calling `TheTacticalView->userLookAt` directly rather than by synthesizing a right click | High — with an army selected the player cannot move the camera from the minimap at all, and the attempt sends their units somewhere instead; this is the most destructive of the missing actions |
| Minimap: order units to a point | Left click on the radar (default mouse setup) | `MSG_DO_MOVETO` at the radar world position, with voice response | `ControlBarCallback.cpp:298`–`307` | COVERED | Tap, via the window-manager replay (`SDL3GameEngine.cpp:1230`–`1248`) | — |
| Minimap: fire an armed special power | Click on the radar with a `NEED_TARGET_POS` power armed | `evaluateContextCommand(nullptr, world, DO_COMMAND)` | `ControlBarCallback.cpp:271`–`280` | COVERED — an armed command only hijacks a finger that is *not* on UI (`SDL3GameEngine.cpp:893`), so a radar touch stays on the deferred path and reaches this callback | — | — |
| Minimap: attack-move to a point | Click on the radar with attack-move armed | `MSG_DO_ATTACKMOVETO` | `ControlBarCallback.cpp:282`–`290` | COVERED — same path | — | — |
| Press a control-bar button | Left click | Ordinary window-manager button press | `WindowXlat.cpp:96`–`102`, `WindowXlat.cpp:222`–`266` | COVERED | Immediate press at touch-down for `GWS_PUSH_BUTTON` leaves (`SDL3GameEngine.cpp:846`–`862`), release at the original anchor (`SDL3GameEngine.cpp:1448`–`1461`) | — |
| Press a non-button widget (dialog, list, panel) | Left click | Window manager routes it | `WindowXlat.cpp:222`–`266` | COVERED | Deferred, then replayed as hover + down + up at release, with the pointer withdrawn afterwards (`SDL3GameEngine.cpp:1207`–`1248`) | — |
| Read a build button's description | Hover the button | Popup description while the pointer rests on it | `ControlBarPopupDescription.cpp` (poll), fed by `ControlBar.h:998` `setTouchHoldPoint` | COVERED | Held press keeps the description alive, and the release un-arms whatever the hold armed (`SDL3GameEngine.cpp:1462`–`1468`, `TouchInput.cpp:390`–`394`) | — |
| **Right click on a GUI button (`GBM_SELECTED_RIGHT`)** | Right click on a push button or check box | A second, distinct action on the same widget | gadget side `GadgetPushButton.cpp:308` and `GadgetPushButton.cpp:344`, `GadgetCheckBox.cpp:170`; consumers `GroupPanel.cpp:399`–`408` (force-assign a control group), `ControlBarCallback.cpp:377`, `ControlBarObserver.cpp:190` | **MISSING** — the touch layer emits no `MSG_RAW_MOUSE_RIGHT_*` anywhere (no match in `SDL3GameEngine.cpp`); the two-finger tap that used to produce one now calls `TouchInput::cancelOrDeselect` instead (`SDL3GameEngine.cpp:1334`–`1337`). `GroupPanel.cpp:36`–`41` still documents the old behaviour | Either restore a right-click emission for touches that land on a widget only, or give the group panel its own second action. In practice the group panel already has a workaround — hold 600 ms clears, then tap assigns (`GroupPanel.cpp:386`–`395`) — so this is two steps instead of one | Medium — the only shipped consumer is control-group force-assign, but that is a frequent action |
| **Drag a slider thumb** | Left press on the thumb, drag | `GWM_LEFT_DRAG` moves the value continuously | `GadgetHorizontalSlider.cpp:133`–`139`, `GadgetVerticalSlider.cpp:113`; the drag message itself is produced only by the desktop mouse (`Mouse.cpp:751`) | **MISSING** — the touch layer never emits `MSG_RAW_MOUSE_LEFT_DRAG`, and a touch on the thumb (a `GWS_PUSH_BUTTON` child) enters `UI_PRESS`, which ignores all motion by design (`SDL3GameEngine.cpp:328`–`333`) | Let `UI_PRESS` re-publish positions (not just the frozen anchor) when the pressed widget's parent is a slider, and emit `MSG_RAW_MOUSE_LEFT_DRAG`. Partial workaround exists today: tapping the slider *track* pages the value (`GadgetHorizontalSlider.cpp:147`–`178`) | Low — options screens only, and paging works |
| **Mouse wheel over a list box or combo box** | Wheel up/down over the widget | Scrolls the list | `WindowXlat.cpp:137`–`142` and `WindowXlat.cpp:293`–`311`; gadget side `GadgetListBox.cpp:762`, `GadgetListBox.cpp:775`, `GadgetListBox.cpp:1175`, `GadgetListBox.cpp:1186` | **PARTIAL** — no wheel exists on touch, and a drag on a list box is claimed by the camera pan classifier (`SDL3GameEngine.cpp:1079`–`1088`) or, in the shell, discarded because the pan is blocked there (`SDL3GameEngine.cpp:487`–`503`). The list's own up/down arrow buttons are `GWS_PUSH_BUTTON` (`GadgetListBox.cpp:2360`, `GadgetListBox.cpp:2378`) and are tappable | Flick-to-scroll: when the deferred touch's owning window is a scrolling list box (its scroll slider is built at `GadgetListBox.cpp:2403`), route the drag to the list's slider rather than to `PANNING`. Conflicts with the pan classifier, which currently claims every drag that is not on a pending placement | Medium — map lists, replay lists and the multiplayer lobby are painful with arrow buttons alone |
| **Pre-commit feedback: what will this click do?** | Move the pointer over anything | `MSG_MOUSEOVER_DRAWABLE_HINT` / `MSG_MOUSEOVER_LOCATION_HINT` are emitted, the evaluator runs in `DO_HINT` mode and the cursor plus a hint text say "attack", "enter", "repair", "impossible attack", "add waypoint" and so on | hints emitted `SelectionXlat.cpp:582`–`606`; hinted `CommandXlat.cpp:3808`–`3858`; consumed `HintSpy.cpp:50`–`94`; drawn `GeneralsMD/.../InGameUI.cpp:2614` | **PARTIAL** — outside `SELECTING`, `PLACING` and UI presses the touch layer publishes no positions at all, and the mouseover branch only runs when the left button is up (`SelectionXlat.cpp:581`), so no hint is ever generated. What *is* native is armed-command validity: the reticle asks the same evaluator in `EVALUATE_ONLY` mode (`TouchInput.cpp:368`–`387`, fed at `SDL3GameEngine.cpp:804`–`809` and `SDL3GameEngine.cpp:1099`–`1109`) | Extend the same `EVALUATE_ONLY` answer to the unarmed case: while a finger is held on the battlefield before the gesture resolves, show the order that a release would produce. The information is already computed for the armed case; the gesture (press-and-hold before release) is already reserved by `PENDING` | High — a touchscreen has no hover, so the player commits blind on every order; this is the single largest usability difference from the mouse |
| Ability / special power aiming with radius | Arm from the control bar, move the pointer, click to fire | Radius decal follows the cursor, cursor art says valid/invalid, click fires | decal `InGameUI` `setRadiusCursor` via `createCommandHint`; dispatch `GUICommandTranslator.cpp:387`–`400`, context powers `CommandXlat.cpp:1712`–`1770` | COVERED | Dedicated `TARGETING` phase: finger is the reticle (`SDL3GameEngine.cpp:893`–`901`, `SDL3GameEngine.cpp:1099`–`1109`), decal created on first aim (`TouchInput.cpp:207`–`230`), commit is a real click because `GUICommandTranslator` acts on nothing else (`SDL3GameEngine.cpp:1416`–`1446`, rationale `TouchInput.h:105`–`112`) | — |
| Building placement, including rotation | Press, drag to set the angle, release | `setPlacementStart` on the down, `setPlacementEnd` on motion, `MSG_DOZER_CONSTRUCT` on the click | `PlaceEventTranslator.cpp:72`–`152`, `PlaceEventTranslator.cpp:324`–`352`, commit `PlaceEventTranslator.cpp:270`–`282` | COVERED | `PLACING` phase (`SDL3GameEngine.cpp:1019`–`1041`, `SDL3GameEngine.cpp:1110`–`1120`, release `SDL3GameEngine.cpp:1352`–`1375`); a plain tap places at the default angle (`SDL3GameEngine.cpp:1282`–`1285`) | — |
| Line build (walls) | Press at one end, drag to the other, release | `MSG_DOZER_CONSTRUCT_LINE` with both world points | `PlaceEventTranslator.cpp:270`–`282` (`isLineBuild` branch) | COVERED by the same `PLACING` drag | — | — |
| **Scroll the camera while a placement is pending** | Edge scroll or arrow keys while holding the ghost | Lets the player place beyond the current view; essential for long walls | `LookAtXlat.cpp:224`–`266` (keys), `LookAtXlat.cpp:364`–`380` (edge) | **PARTIAL/MISSING** — a drag during a pending placement is claimed by the rotate gesture (`SDL3GameEngine.cpp:1019`–`1041`) and a second finger cancels the placement outright (`SDL3GameEngine.cpp:969`–`984`), so the camera cannot move between arming a build and committing it | Allow the two-finger pan during `PLACING` instead of cancelling, and move the cancel to a two-finger *tap* (already the cancel gesture everywhere else). Conflicts: the current second-finger-cancels rule, which was added because `PLACING` had no other abort | Medium — mostly hurts walls and expansion buildings placed at the screen edge |
| Skip a movie | Left click anywhere during playback | `stopMovie()` on a raw button-down nothing else consumed | `WindowXlat.cpp:247`–`256` | COVERED | Handled ahead of everything else in the tap resolver (`TouchInput.cpp:186`–`204`) | — |
| Double click on own unit: select all of that type on screen | Left double click | `selectMatchingAcrossScreen()` | `SelectionXlat.cpp:613`–`683`, implementation `GeneralsMD/.../InGameUI.cpp:5407` | COVERED | Double tap within 350 ms / 40 px (`SDL3GameEngine.cpp:1259`–`1299`, `TouchInput.cpp:297`–`365`) | — |
| Double click on ground: attack-move ("Double Click Guard") | Left double click on bare ground with the option on | `MSG_DO_GUARD_POSITION` in `GUARDMODE_NORMAL`, plus academy stat and hint | `CommandXlat.cpp:3978`–`3997` | COVERED | Double tap that picks nothing (`TouchInput.cpp:338`–`357`); the option's alternate-mouse condition is deliberately dropped there | — |
| Control group: assign | Create-team modifier + digit | `MSG_CREATE_TEAM<n>` from the current selection | `SelectionXlat.cpp:1159`–`1180` | COVERED (not by a gesture, by an on-screen panel) | Tap an empty group button on the touch group panel (`GroupPanel.cpp:144`–`155`, `GroupPanel.cpp:386`–`395`; layout `android/app/src/main/assets/gamedata/Window/GroupPanel.wnd`) | — |
| Control group: recall, and double-press to jump the camera | Digit, pressed once or twice quickly | `MSG_META_SELECT_TEAM<n>`; a second press inside `m_doubleClickTimeMS` re-centres the view | `SelectionXlat.cpp:1181`–`1252` | COVERED | Tap an occupied group button; two quick taps send two messages and hit the same double-press branch | — |
| **Control group: re-assign an occupied group in one action** | Create-team modifier + digit | Replaces the group's contents | `SelectionXlat.cpp:1159`–`1180` | **PARTIAL** — a plain tap on an occupied group recalls rather than assigns (`GroupPanel.cpp:147`), and the force-assign path is the now-unreachable `GBM_SELECTED_RIGHT` (`GroupPanel.cpp:399`–`408`). The workaround is hold-to-clear then tap (`GroupPanel.cpp:388`–`390`) | Give the panel a modifier of its own — e.g. a small "assign" latch beside the row, or a second tap within the double-tap window meaning assign | Medium — frequent action, currently two deliberate steps |
| **Control group: add a group to the current selection** | Add-team modifier + digit | `MSG_ADD_TEAM<n>`: appends the group without clearing | `SelectionXlat.cpp:1253`+, body `SelectionXlat.cpp:1290`–`1322` | **MISSING** — the group panel only ever sends create or select (`GroupPanel.cpp:150`–`154`) | **TRIED AND WITHDRAWN 08/09/2026.** A one-shot "+" prefix button was added to the group row: press it, then press a group, and the panel sent `MSG_META_ADD_TEAM<n>` instead of `MSG_META_SELECT_TEAM<n>`. On a device the button armed correctly and the group press still REPLACED the selection. Reading the code does not explain it -- the panel picks the right message, `SelectionTranslator::onMetaAddTeam` adds without deselecting, and `Player::processAddTeamGameMessage` appends to `m_currentSelection` without clearing it -- so the cause is something only a device log will show, and it was pulled rather than left as a button that lights up and lies. Anyone picking this up again should start from a log, not from the source. Note also that two quick presses of one group are read by the engine as "centre the camera" (`SelectionXlat.cpp:1197`), which rules out a double tap as the gesture, and that the first tap of any double tap has already thrown the selection away | Medium — and note a partial workaround exists for adjacent units: box-select everything, hold the group button to clear, tap to reassign |
| **Control group: look at a group without selecting it** | View-team modifier + digit | `userLookAt` the group's position, selection untouched | `SelectionXlat.cpp:1331`–`1356` | **MISSING** | A short hold on the group button — but note 600 ms is already taken by clear (`GroupPanel.cpp:388`), so this needs a different affordance (e.g. a swipe up off the button) | Low — recall then camera-jump achieves nearly the same thing |
| Control group: clear a group | — (no mouse equivalent) | — | — | Touch-only addition | Hold a group button until the clock wipe completes (`GroupPanel.cpp:386`–`391`) | — |
| Attack-move | Control-bar button, then click a destination | Arms `GUI_COMMAND_ATTACK_MOVE`, the next click issues it | armed `ControlBarCommandProcessing.cpp:207`–`224`, dispatched `GUICommandTranslator.cpp:465`, mode toggle `CommandXlat.cpp:3394`–`3396` | COVERED | `TARGETING` phase, committed as a click (`SDL3GameEngine.cpp:1416`–`1446`) | — |
| Guard, evacuate, hijack, sabotage, combat drop, fire weapon | Control-bar button, then click a target | `GUICommandTranslator` dispatches and reports `COMMAND_COMPLETE`, which clears the mode | `GUICommandTranslator.cpp:387`–`400`, mode cleared `GUICommandTranslator.cpp:496` | COVERED | Same `TARGETING` commit; this is exactly why the commit is a click and not a hand-rolled dispatch (`SDL3GameEngine.cpp:1420`–`1440`) | — |
| Stop, sell, set rally point, exit container, upgrades, science, overcharge | Control-bar buttons | Ordinary command-bar processing | `ControlBarCommandProcessing.cpp:226`+ (`GUI_COMMAND_STOP` at `:627`) | COVERED | Ordinary `UI_PRESS` | — |
| Options / quit menu, diplomacy, beacons, idle worker, control-bar size | Control-bar buttons | `ToggleQuitMenu`, `ToggleDiplomacy`, beacon commands, `selectNextIdleWorker`, `toggleControlBarStage` | `ControlBarCallback.cpp:392`–`441` | COVERED | Ordinary `UI_PRESS` | — |
| "Alternate Mouse Setup" option | Options → Control → checkbox | Swaps left/right roles: right click orders, left click deselects; also changes which double click guards | `TheGlobalData->m_useAlternateMouse`, read at `CommandXlat.cpp:3903`, `CommandXlat.cpp:3926`, `CommandXlat.cpp:4022`, `SelectionXlat.cpp:1063`, `SelectionXlat.cpp:1148`, `ControlBarCallback.cpp:263`–`264` | **MISSING (known, out of scope)** — a finger has no second button, so the setting does not govern touch input; explicitly noted at `TouchInput.cpp:334`–`337` | None — the setting should be hidden or explained on touch rather than implemented | Low — listed for completeness only, per the audit brief |

---

## Keyboard-only actions

An on-screen scheme has to replace these too. None of them is reachable from any touch
gesture: they are produced only by `MetaEventTranslator` from real key events
(`MetaEvent.cpp:572`–`605`), and the touch layer emits no key events. A physical keyboard
attached over USB or Bluetooth still works — SDL key events are forwarded normally
(`SDL3GameEngine.cpp:1996`–`2007`) — so these are "unreachable by touch", not "unreachable".

The key that triggers each one is data, not code: it comes from `CommandMap.ini` in the
game's `.big` archives, which is **not in this repository (not found)**. The engine side is
cited instead.

**Selection and camera navigation**

- Select matching units — `CommandXlat.cpp:2492`.
- Select next / previous unit — `CommandXlat.cpp:2502`, `CommandXlat.cpp:2612`.
- Select next / previous worker — `CommandXlat.cpp:2729`, `CommandXlat.cpp:2839`.
- Select next idle worker — `CommandXlat.cpp:2957`. *Also on the control bar* (`ControlBarCallback.cpp:437`–`440`), so this one is reachable.
- Select hero — `CommandXlat.cpp:2966`.
- Select all / select all aircraft — `CommandXlat.cpp:3024`–`3025`.
- View command centre — `CommandXlat.cpp:3002`.
- View last radar event — `CommandXlat.cpp:3008`.
- Save / recall camera view 1–8 — `LookAtXlat.cpp:679`–`719`.
- Camera rotate left/right (held and stepped), zoom in/out (held), reset, track selected drawable — `CommandXlat.cpp:3398`–`3443`.
- Keyboard edge-free camera scroll with the arrow keys — `LookAtXlat.cpp:224`–`266`. Covered in practice by drag panning.

**Orders and modes**

- Force attack begin/end — `CommandXlat.cpp:3716`–`3723`.
- Force move begin/end — `CommandXlat.cpp:3681`–`3689`.
- Waypoint mode begin/end — `CommandXlat.cpp:3693`–`3712`.
- Prefer-selection begin/end — `CommandXlat.cpp:3699`–`3707`.
- Toggle attack-move — `CommandXlat.cpp:3394`. *Also on the control bar.*
- Stop — `CommandXlat.cpp:3135`. *Also on the control bar* (`ControlBarCommandProcessing.cpp:627`).
- Scatter — `CommandXlat.cpp:3128`. No on-screen equivalent.
- Create formation — `CommandXlat.cpp:3143`. No on-screen equivalent.
- Deploy and Follow — `CommandXlat.cpp:3151`, `CommandXlat.cpp:3160`. Both are unimplemented stubs in the engine itself, so nothing is lost.
- All cheer — `CommandXlat.cpp:3726`.

**Control groups**

- Create team 0–9 — `SelectionXlat.cpp:1159`. *Covered by the touch group panel.*
- Select team 0–9 — `SelectionXlat.cpp:1181`. *Covered.*
- Add team 0–9 — `SelectionXlat.cpp:1253`. **Not covered.**
- View team 0–9 — `SelectionXlat.cpp:1331`. **Not covered.**

**Shell, chat and session**

- Options / quit menu — `CommandXlat.cpp:3249`. *Also on the control bar* (`ControlBarCallback.cpp:433`–`435`).
- Chat to allies / everyone — `CommandXlat.cpp:3181`, `CommandXlat.cpp:3195`. Reaching the chat entry field also needs the on-screen keyboard, which the port re-arms on finger events (`SDL3GameEngine.cpp:2049`–`2052`).
- Diplomacy screen — `CommandXlat.cpp:3210`. *Also on the control bar* (`ControlBarCallback.cpp:392`).
- Place / remove beacon — `CommandXlat.cpp:3221`, `CommandXlat.cpp:3240`. *Also on the control bar* (`ControlBarCallback.cpp:396`–`406`).
- Toggle control bar — `CommandXlat.cpp:3342`. *Also on the control bar* (`ControlBarCallback.cpp:429`).
- Toggle player/observer view — `CommandXlat.cpp:3372`.
- Pause, step frame, fast-forward replay — `CommandXlat.cpp:3465`, `CommandXlat.cpp:3485`, `CommandXlat.cpp:3445`.
- Screenshot — `CommandXlat.cpp:3738`.
- In-game help — `CommandXlat.cpp:4166`.
- Lower details toggle, render-FPS and logic-time-scale steps — `CommandXlat.cpp:3296`, `CommandXlat.cpp:3256`–`3286`.
- Widget hot keys (the `&`-marked letter on a button) — `HotKey.cpp:69`–`107`. Irrelevant on touch: the buttons themselves are tappable.

Debug and cheat metas (`MSG_META_DEMO_*`, `MSG_CHEAT_*`, roughly `CommandXlat.cpp:3498`–`5518`)
are excluded deliberately; they are not player-facing in a release build.

---

## Gesture budget

### Already assigned

| Gesture | Bound to | Where |
|---|---|---|
| Single tap, battlefield | Context-resolved select or order | `SDL3GameEngine.cpp:1289`, `TouchInput.cpp:233` |
| Single tap on a `GWS_PUSH_BUTTON` leaf | Immediate button press at touch-down | `SDL3GameEngine.cpp:846`–`862` (test at `SDL3GameEngine.cpp:626`) |
| Single tap on any other window | Deferred, replayed to the window manager at release | `SDL3GameEngine.cpp:1207`–`1248` (test at `SDL3GameEngine.cpp:643`) |
| Double tap on own mass-selectable unit | Select all of that type on screen | `SDL3GameEngine.cpp:1287`, `TouchInput.cpp:363`–`364` |
| Double tap on bare ground | Attack-move guard, when "Double Click Guard" is on | `TouchInput.cpp:338`–`357` |
| Press and hold ≥600 ms, release without moving, battlefield | Cancel armed command / cancel placement / deselect | `SDL3GameEngine.cpp:1157`–`1176` |
| Press and hold ≥600 ms on a control-bar button | Keep the description open; the release then un-arms what it armed | `SDL3GameEngine.cpp:1462`–`1468` |
| Press and hold ≥600 ms while aiming an armed command | Escape targeting mode | `SDL3GameEngine.cpp:1409`–`1414` |
| Hold ≥250 ms, then drag | Selection box | `SDL3GameEngine.cpp:1041`–`1078` |
| Drag before 250 ms have elapsed | Camera pan | `SDL3GameEngine.cpp:1079`–`1088` |
| Fast release of a pan | Momentum coast | `SDL3GameEngine.cpp:1302`–`1318` |
| Drag while a placement is pending (any timing) | Anchor the building and set its angle / line-build end | `SDL3GameEngine.cpp:1019`–`1041` |
| Any battlefield touch while a GUI command is armed | Aim; release fires | `SDL3GameEngine.cpp:893`–`901`, `SDL3GameEngine.cpp:1390`–`1446` |
| Two fingers, moving | Pan (centroid) and zoom (spread), both every frame | `SDL3GameEngine.cpp:946`–`968`, `SDL3GameEngine.cpp:1650`–`1664` |
| Two-finger tap (neither finger moves more than 24 px) | Cancel armed command / cancel placement / deselect | `SDL3GameEngine.cpp:1329`–`1338` |
| Second finger during a placement rotate | Cancel the placement | `SDL3GameEngine.cpp:969`–`984` |
| Second finger during a selection box | Finalize the box as drawn | `SDL3GameEngine.cpp:985`–`997` |
| Lifting one finger of two while still moving | Continue as a one-finger pan | `SDL3GameEngine.cpp:1338`–`1349` |
| Tap a group-panel button | Recall the group, or assign it if empty | `GroupPanel.cpp:386`–`395`, `GroupPanel.cpp:144`–`155` |
| Hold a group-panel button 600 ms (clock wipe) | Clear the group | `GroupPanel.cpp:388`–`390` |

### Still free

- **Two-finger twist.** `TWOFINGER` reads only the centroid and the inter-finger distance (`SDL3GameEngine.cpp:1650`–`1664`); the relative angle is untouched. The obvious home for camera rotation.
- **Three or more fingers.** Explicitly ignored today (`SDL3GameEngine.cpp:998`).
- **Two-finger double tap.** The two-finger release tests only for a single tap (`SDL3GameEngine.cpp:1330`–`1337`).
- **Triple tap / double-tap-and-hold.** The tap counter resets after a completed double tap (`SDL3GameEngine.cpp:1294`–`1299`), so a third tap currently just becomes a fresh single tap; the reset would have to be relaxed.
- **Long press on UI.** The long-press cancel excludes any point the window manager owns (`SDL3GameEngine.cpp:1157`), so holding on the radar, a dialog, a list or a panel has no meaning at all today. This is where a minimap look-at belongs.
- **Edge swipes and swipes off a widget.** Nothing consumes them.
- **New on-screen controls.** The group panel establishes the pattern: a `.wnd` layout shipped as an Android asset outside the game's own `.big` archives, driven by an ordinary window callback (`GroupPanel.cpp:19`–`30`, `android/app/src/main/assets/gamedata/Window/GroupPanel.wnd`). Every latching modifier proposed above (force fire, force move, waypoint, add-to-selection) fits this pattern and costs no gesture at all.

### Notes on conflicts

- A "hold then drag" gesture cannot be added: 250 ms of hold followed by a drag is already the selection box, and any earlier drag is already the pan. The two exhaust the one-finger drag space.
- Anything routed through a synthesized right click is now dead weight: nothing in the touch layer emits `MSG_RAW_MOUSE_RIGHT_*`, and re-introducing one revives the latched-scroll hazard that `TouchInput.h:28`–`38` and `SelectionXlat.cpp:1116`–`1130` describe. New capabilities should call the engine directly, the way `TouchInput` already does.
- Latching modes need an explicit exit. Two shipped bugs came from a mode with no way out (`SDL3GameEngine.cpp:1391`–`1414` for targeting, `SDL3GameEngine.cpp:1462`–`1468` for a held build button); any new toggle should clear on deselect and show its state.

---

## Open requests from device testing, 09/09/2026

Raised together after a full campaign-mission playthrough on device. Ordered by
how much they cost the player, not by how hard they are.

### 1. Cursor feedback lives under the finger, not on a cursor

The mouse showed intent before you committed: a red arrow over an enemy, a flag
over a capturable building, a distinct cursor for attack-move. Touch has none of
that, and the one thing it does have -- the radius decal for abilities -- is a
green square regardless of what the ability actually is.

What is wanted: while a finger is down on a target (and while it hovers, in the
sense of "held before release"), draw the ability's real icon at the finger.
Red arrow for a force-attack on an enemy object, the capture flag for capture,
the attack-move icon for attack-move. Plain movement stays as it is -- the green
ground marker already says everything, and an icon there would be noise.

The radius-cursor machinery already exists (`InGameUI::setRadiusCursor`,
`RADIUSCURSOR_*`), and `triggerTouchAttackMoveGuardHint` shows the pattern for
driving it from a touch gesture. The icons are the game's own command-button
images.

### 2. Health bars on tap

The mouse showed a unit's or building's health on hover. A tap should do the
same for a moment -- and for as long as the finger stays down if it stays down.

### 3. Two-finger camera rotation

Still free, as noted above: `TWOFINGER` reads the centroid and the distance and
throws the angle away (`SDL3GameEngine.cpp:1650`-`1664`). Rotation is the
obvious use for it and the last camera control the mouse+keyboard build has that
touch does not.

### 4. The camera can be dragged during scripted cinematics

The engine hands the camera to the script during a cutscene; the touch pan does
not know that and fights it, breaking the intended shot. The pan needs to fold
while the script owns the camera.

### 5. Cinematic stills render in colour

Where the game is supposed to show its black-and-white mission photographs, they
come out in colour. Some filter or material state that the D3D8 path applied is
not surviving the GLES translation.

### 6. Main-menu buttons drawn over each other

Seen once, in one session: the Solo Play submenu (`CAMPAIGN`, `SKIRMISH`, and a
`MISSING: 'GUI:CustomMission'` string) painted through the main list while both
were visible. A transition that did not finish hiding the layer underneath.
Not yet reproducible.
