#pragma once

// GeneralsX @feature Android port 10/07/2026 entry point for MainMenuUtils.cpp's
// Online-button handler to hand off to GeneralsOnline instead of the dead
// GameSpy patch-check/DNS path. Not ported from upstream -- ours.

// Returns true if a GeneralsOnline session was found (written by the Android
// launcher's GeneralsOnlineActivity, generalsonline_session.txt) and the
// connect flow was started -- caller should skip the legacy GameSpy path
// entirely in that case. Returns false (does nothing) if there's no session
// yet, or on any non-Android build.
bool TryStartGeneralsOnline();
