# Startup order: shell map audible under the intro movie

## Symptom, as evidenced by a device log

The shell map (`Maps\ShellMapMD\map.ini`) loads and starts simulating **twice** at
startup. The first load is the bug:

```
[INI] load('Maps\ShellMapMD\map.ini') START        <- unwanted, happens during the movie
[GX-AUDIO] sample starts during movie: Amb_JungleDayDenseAmbientLoop
[GX-AUDIO] sample starts during movie: AmphibiousTransportMoveLoop
[GX-AUDIO] intro finished (30 frames quiet) -> loading shell map   <- correct, later load
```

The second load is produced by the guard already in
`GeneralsMD/Code/GameEngine/Source/GameClient/GameClient.cpp`, inside
`GameClient::update()`'s `m_afterIntro` block
(`GameClient.cpp:690-698`, calling `TheShell->showShellMap(TRUE); TheShell->showShell();`
once the screen has been free of a movie for 30 frames). That guard is correct and not
the problem. The problem is the *first* load, which happens well before it, while the
EA logo / sizzle movie is still on screen — hence the ambient loops and unit-move audio
under the video.

## Ordered call path that produces the early load

1. `GameEngine::init()` calls `TheShell->push( "Menus/MainMenu.wnd" )` at
   `GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp:847`. This runs during
   engine start-up, before the first call to `GameEngine::update()` — i.e. before a
   single frame has been drawn and before either intro-related flag
   (`m_playIntro`, `m_afterIntro`) has been touched by that frame's logic
   (those are only adjusted afterwards, at `GameEngine.cpp:886-887` and `:917-918`,
   and only when `m_playIntro` is FALSE to begin with).

2. `Shell::push()` (`GeneralsMD/Code/GameEngine/Source/GameClient/GUI/Shell/Shell.cpp:320-386`)
   marks the layout as a pending push, then, because the shell screen stack is empty at
   this point (`currentTop` is null), calls `shutdownComplete( nullptr )` directly
   (`Shell.cpp:378-379`) instead of waiting for an asynchronous shutdown. **This makes
   the push synchronous** in this particular case.

3. `Shell::shutdownComplete()` sees the pending push and calls `doPush()` immediately
   (`Shell.cpp:783-790`).

4. `Shell::doPush()` creates the window layout and, still synchronously, calls
   `newScreen->runInit( nullptr )` (`Shell.cpp:720`), which is the layout's registered
   init callback for `Menus/MainMenu.wnd` — `MainMenuInit()`.

5. `MainMenuInit()` (`GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/MainMenu.cpp:487-492`)
   unconditionally calls `TheShell->showShellMap(TRUE);` at **line 491**, immediately
   followed by `TheMouse->setVisibility(TRUE);` at line 492. No check on
   `m_playIntro` / `m_afterIntro` guards this call.

So the exact call site is confirmed: **`MainMenu.cpp:491`**, reached synchronously
from `GameEngine::init()` (`GameEngine.cpp:847`) via `Shell::push` → `Shell::doPush` →
`WindowLayout::runInit` → `MainMenuInit`, all before frame 1 and before the intro
sequencing in `GameClient::update()` has run even once.

## What `Shell::showShellMap()` actually does

`Shell::showShellMap()` (`Shell.cpp:535-586`) does **not** load or start the map
itself. For `useShellMap == TRUE` and `TheGlobalData->m_shellMapOn` true (the default —
`GlobalData.cpp:1058`, INI key `ShellMapOn`, `GlobalData.cpp:449`), it:

- sets `TheWritableGlobalData->m_pendingFile = TheGlobalData->m_shellMapName;`
- appends a `GameMessage::MSG_NEW_GAME` message (with argument `GAME_SHELL`) to
  `TheMessageStream` (`Shell.cpp:554-557`)
- sets its own `m_shellMapOn = TRUE`

and returns. Nothing about map loading happens inline. The load is deferred to
whatever consumes that message — which turns out to be a mechanism that is *also*
gated on the movie:

- `GameEngine::update()` (`GameEngine.cpp:1096-1175`) runs, per frame, in this order:
  `TheGameClient->UPDATE()` (`:1119`) → `TheMessageStream->propagateMessages()`
  (`:1121`, moves the queued message into `TheCommandList`) → `TheGameLogic->UPDATE()`
  (`:1147`).
- `GameLogic::update()` (`GeneralsMD/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp:3763-3877`)
  checks, near the top, **before** processing that frame's commands:
  ```
  /// @todo remove this hack
  if ( m_startNewGame && !TheDisplay->isMoviePlaying())   // GameLogic.cpp:3775-3776
      startNewGame( FALSE );                              // GameLogic.cpp:3788 — the real, heavy load
  ```
  and only later in the same call, at `GameLogic.cpp:3877`, processes
  `TheCommandList` (`processCommandList` → `logicMessageDispatcher` →
  `onNewGame()`, defined in `Core/GameEngine/Source/GameLogic/System/GameLogicDispatch.cpp:866-905`),
  which calls `prepareNewGame()` then `startNewGame( FALSE )` again
  (`GameLogicDispatch.cpp:901,904`). The first time through,
  `GameLogic::tryStartNewGame()` (`GameLogic.cpp:1181-1246`) does no real work: because
  `m_startNewGame` is still FALSE, it just sets `m_startNewGame = TRUE` and returns
  (`GameLogic.cpp:1220-1245`). The actual, expensive load (INI parsing, object
  creation — the thing the log's `map.ini` line is timing) only happens the *next*
  time `startNewGame()` runs with `m_startNewGame` already TRUE, i.e. from the
  `GameLogic.cpp:3776` check above, on a later frame.
- There is a sibling accessor, `GameLogic::isIntroMoviePlaying()`
  (`GameLogic.cpp:2788-2792`, also `/// @todo remove this hack`), that returns
  `m_startNewGame && TheDisplay->isMoviePlaying()` — confirming this mechanism exists
  specifically to hold the "start the shell game" logic back until the intro movie is
  off screen.

So the real answer to "does it load immediately, or defer to something else": **it
defers**, via a message and a two-stage `m_startNewGame` flag, to a gate in
`GameLogic::update()` that is supposed to hold the load until
`TheDisplay->isMoviePlaying()` is false. That gate is original engine code (present in
the EA source drop, `git log` shows it authored by EA contributors, not the port), not
something introduced by this port.

## Why the original PC code doesn't show this bug

`MainMenuInit()`'s unconditional `showShellMap(TRUE)` call is not itself something the
port added — nothing in `Shell::push`/`doPush`/`MainMenuInit` is gated on
`m_playIntro` in the upstream code either. What makes it harmless on PC is that the
actual load is deferred, as shown above, to `GameLogic.cpp:3776`'s
`!TheDisplay->isMoviePlaying()` check, which is reliable on PC: `Display::playLogoMovie()`
/ `playMovie()` open the video stream synchronously
(`Core/GameEngine/Source/GameClient/Display.cpp:209-268`,
`Display::isMoviePlaying()` at `:368-371` is simply
`m_videoStream != nullptr && m_videoBuffer != nullptr`), so on PC the flag flips true
in the same frame the movie starts and stays reliably true for the movie's duration.

This port's own code already documents, independently, that the same flag is *not*
reliable here. The comment directly above the `GameClient.cpp` guard
(`GameClient.cpp:601-618`) — added while fixing the visible/audible symptom of this
exact class of bug for `GameClient::update()`'s own movie-start logic — states plainly:

> "this port's video path is asynchronous, so that flag is still false in the frame
> that starts a movie and can drop briefly between the logo and sizzle. Either window
> lets the shell through."

That fix added a 30-frames-of-quiet debounce (`s_framesSinceMoviePlaying`,
`GameClient.cpp:614-618,690`) around `GameClient.cpp`'s *own* use of
`isMoviePlaying()` (the code that starts the logo/sizzle movies and, afterwards, calls
`TheShell->showShellMap(TRUE); TheShell->showShell();` a second time at
`GameClient.cpp:695-696`). But `GameLogic.cpp:3776`'s **independent**, undebounced use
of the exact same unreliable flag was not touched by that fix, and it is the one that
actually triggers the real, heavy `startNewGame()` load. Once `MainMenuInit()` has
queued the `MSG_NEW_GAME` message at start-up (before frame 1) and `m_startNewGame`
flips true (end of frame 1, per the two-stage mechanism above), `GameLogic.cpp:3776`
only needs `isMoviePlaying()` to read false for a single frame — exactly the
false-negative window this port's video path is documented to produce — for the real
map load to run while the movie is still visually on screen. This also explains the
double load in the log: the early, unguarded `GameLogic.cpp:3776` path fires first,
and the debounced `GameClient.cpp:695` path fires again ~30 frames later once it is
confident the movie is actually finished.

One more piece of corroborating evidence for why this wasn't caught visually before:
the shell map's own 3D render is separately gated by `m_breakTheMovie`
(`GameClient.cpp` sets it, `GlobalData.h:574` — "The user has hit escape!" — default
FALSE, `GlobalData.cpp:1032`) via
`GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp:2266`:
`WW3D::Begin_Render(...)` (the shell map's 3D scene) only runs when
`m_breakTheMovie == FALSE`. So even with the map loaded early, nothing about it is
visually drawn during the movie — only the *audio* side effects of loading and
simulating the map (ambient loops, unit move loops fired by object/module updates)
leak through, because nothing gates those on the movie at all. That matches the
tester's report exactly: audible, not visible, overlap.

## Options considered

### Option A — gate `MainMenuInit`'s `showShellMap(TRUE)` call on the intro being done

Change `MainMenu.cpp:491` to only call `TheShell->showShellMap(TRUE)` when
`!TheGlobalData->m_playIntro && !TheGlobalData->m_afterIntro` — i.e. the same
condition already used to decide the intro is over in `GameClient.cpp`.

- **At the startup push (`GameEngine.cpp:847`)**: if the intro is enabled
  (`m_playIntro` defaults TRUE, `GlobalData.cpp:1059`), the condition is false, so the
  call is skipped — no `MSG_NEW_GAME` is queued, no early `m_startNewGame` flip, and
  the `GameLogic.cpp:3776` gate never gets a chance to race the unreliable
  `isMoviePlaying()` flag. The already-existing `GameClient.cpp:695-696` call
  (`TheShell->showShellMap(TRUE); TheShell->showShell();`) becomes the *only* source of
  the shell map load, exactly once, after the movie is confirmed done.
- **If the intro is disabled from the start** (`m_playIntro` FALSE via INI/command
  line, e.g. `CommandLine.cpp:449,785`, or a "skip intro" option): at push-time
  `m_afterIntro` is still FALSE (it's only set at `GameEngine.cpp:886-887`, which runs
  *after* the push at `:847`), but `m_playIntro` is already FALSE, so
  `!m_playIntro && !m_afterIntro` is `TRUE && TRUE` = true — the call goes through
  immediately, matching today's (non-buggy, for this case) behavior.
- **Every later call to `MainMenuInit`** (returning to the main menu after a match,
  from a lobby, etc.) happens with both flags FALSE (the intro sequencing only runs
  once, at boot), so the condition is always true then — no behavior change for the
  normal "back to main menu" case.
- `TheMouse->setVisibility(TRUE)` at `MainMenu.cpp:492` is left untouched/unconditional
  under this option. It already runs unconditionally in the upstream engine at the
  same call site and timing; making the cursor visible a frame before the logo movie
  starts is a pre-existing, cosmetic-only PC behavior, not something the tester
  reported, and out of scope for this fix.
- Scope: one call site, one added condition. Everything else `MainMenuInit` does
  (window ID lookups, GameSpy overlay teardown check, resetting local button-state
  variables) still runs exactly when it always has, since only the `showShellMap` call
  is conditioned.

### Option B — delay `TheShell->push("Menus/MainMenu.wnd")` itself until after the intro

Move the call at `GameEngine.cpp:847` out of `GameEngine::init()` and have something
in the `m_afterIntro` path of `GameClient::update()` perform it instead.

- This delays **all** of `MainMenuInit`'s work, not just `showShellMap` — including
  `TheMouse->setVisibility(TRUE)` and, more importantly, the window layout itself:
  `Shell::doPush()` is what actually calls `TheWindowManager->winCreateLayout(...)`
  (`Shell.cpp:694`) and creates the `MainMenu.wnd` windows and their name-key IDs. With
  no push, there is no top-of-stack screen at all until the intro finishes.
- `Shell::showShell()` already has its own fallback push
  (`Shell.cpp:524-531`: `if (!TheGlobalData->m_shellMapOn && m_screenCount == 0) push("Menus/MainMenu.wnd");`),
  and the existing `GameClient.cpp:695-696` guard already calls
  `TheShell->showShellMap(TRUE); TheShell->showShell();` after the intro — so a second,
  separate push mechanism would be layered on top of a fallback that already exists
  for a different case, doubling up the logic this fix needs to reason about.
- It touches `GameEngine::init()`'s startup ordering, which other subsystems may
  implicitly depend on having a shell screen present very early (error dialogs,
  IME/GameSpy overlay handling, general window-manager bookkeeping) — verifying no such
  dependency breaks would require auditing every caller of `TheShell->top()` /
  `TheShell->isShellActive()` during the pre-intro window, which is a much larger
  surface than the one line this bug is actually caused by.
- Net effect for this bug would be equivalent to Option A once done, at strictly
  higher risk and larger diff.

### Recommendation

**Option A.** It is a one-line, narrowly-scoped condition change at the exact call
site already identified as the root cause, it mirrors a condition this codebase
already uses for the identical purpose (`GameClient.cpp:622,637`), it does not touch
`GameEngine::init()`'s startup ordering or any other subsystem's assumptions about the
shell stack being non-empty early, and it collapses the double-load in the log down to
the single, already-correct, debounced load path in `GameClient.cpp:690-698`.

## Other callers of `showShellMap` — confirmed unaffected

All other call sites pass through menu button/back-button handlers or menu `Init`
callbacks reached only via user interaction, long after start-up (`m_playIntro` and
`m_afterIntro` are both FALSE by then, so Option A's added condition is a no-op there):

- `GameClient.cpp:695` — the existing, correct, post-intro guard itself.
- `WOLQuickMatchMenu.cpp:1110`, `WOLLadderScreen.cpp:58`, `WOLWelcomeMenu.cpp:640`,
  `WOLLobbyMenu.cpp:1436` — GameSpy/WOL online-menu back/init handlers.
- `ReplayMenu.cpp:400`, `PopupSaveLoad.cpp:191`, `SinglePlayerMenu.cpp:64`,
  `MapSelectMenu.cpp:158`, `SkirmishGameOptionsMenu.cpp:1379`, `ChallengeMenu.cpp:338`,
  `NetworkDirectConnect.cpp:264`, `LanLobbyMenu.cpp:498` — single-player/skirmish/LAN
  menu back handlers.
- `CreditsMenu.cpp:82` (`showShellMap(FALSE)`, turning the shell map *off* while
  credits play) and `CreditsMenu.cpp:117` (`showShellMap(TRUE)`, restoring it when
  credits close) — both fire only from the Credits menu, reachable only from the
  already-up main menu.

None of these run during `GameEngine::init()`, and none are reached while
`m_playIntro`/`m_afterIntro` are in their start-up states, so Option A does not need to
touch, and should not break, any of them.

## What must be verified on a device after the fix

- Capture a fresh device log across start-up and confirm `map.ini` for
  `Maps\ShellMapMD` loads **exactly once**, and that its `START` line appears only
  after a `[GX-AUDIO] intro finished (... frames quiet) -> loading shell map` line (or
  equivalent), never before.
- Confirm no `[GX-AUDIO] sample starts during movie: ...` lines appear at all during
  the EA logo and sizzle movies (ambient loops, unit move loops, or any other shell-map
  audio).
- Confirm the "intro disabled" path (whatever flag/INI/command-line option sets
  `m_playIntro` FALSE, e.g. `CommandLine.cpp:449,785`) still shows the shell map
  immediately on start-up with no regression — this is the case where Option A's
  condition evaluates true at push-time and preserves today's behavior exactly.
- Confirm returning to the main menu after a skirmish/replay/credits/online session
  still shows the shell map immediately (no added delay), since `MainMenuInit` runs
  again in all of those flows with both intro flags already FALSE.
- Watch total start-up time to the shell menu becoming interactive; Option A should not
  change it (the map load moves earlier/later within the same intro window, it is not
  removed), but confirm the 30-frame debounce in `GameClient.cpp:690` isn't perceptibly
  adding delay now that it is the sole trigger.
