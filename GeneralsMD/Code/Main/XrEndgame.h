// GeneralsX @feature Muse 16/09/2026 Read-only match-result latch for XR.
// A complete offline Skirmish win reached the statistics screen with no
// visible victory hint: the retail end window (Menus/Victorious.wnd) is not
// modal, so the XR tabletop shows it only in the small blended overlay above
// the detached build window, and the quick end path omits the window
// entirely. This header owns the result state machine (host-testable, no
// engine types): it latches the first terminal result observed while a match
// is interactive and keeps it through the direct transition to statistics.
// Victory conditions, simulation, network state and retail data are untouched;
// all inputs are read-only engine queries made by the XrGameBoot adapter.
#pragma once
#include <cmath>

enum class XrEndgameResult { None, Victory, Defeat, MatchOver };

// Card placement: a head-yaw billboard, impossible to miss, dismissed by the
// first controller press or by starting a new match. Deliberately no timeout:
// a player returning after match end must still see the result.
constexpr float kXrEndgameCardDistanceM = 1.2f;
constexpr float kXrEndgameCardDropM = 0.05f;
constexpr float kXrEndgameCardWidthM = 1.0f;

// One poll of read-only engine state. The adapter must only set vcValid when
// the VictoryConditions singleton is safe to query (interactive match, map
// loaded, multiplayer recorder state); the connection trio indexes a cached
// player slot and is undefined before sides are cached.
struct XrEndgameInput {
	bool interactive = false;
	unsigned frame = 0;
	bool ending = false; // ScriptEngine::isGameEnding(): an end timer runs
	bool vcValid = false; // skirmish/LAN/internet/replay result state usable
	bool observer = false;
	bool localVictory = false; // TheVictoryConditions->isLocalAlliedVictory()
	bool alliedDefeat = false; // TheVictoryConditions->isLocalAlliedDefeat()
	bool localDefeat = false; // TheVictoryConditions->isLocalDefeat()
	bool endActionValid = false; // CampaignManager result usable while end timer runs
	bool victorious = false; // TheCampaignManager->isVictorious()
};

struct XrEndgameState {
	XrEndgameResult latch = XrEndgameResult::None;
	unsigned latchFrame = 0;
	bool dismissed = false;
	bool wasInteractive = false;
};

inline void xrEndgameClear(XrEndgameState &state) {
	state.latch = XrEndgameResult::None;
	state.latchFrame = 0;
	state.dismissed = false;
}

// First terminal signal wins for the match. A locally eliminated player keeps
// DEFEAT even if allies later win, mirroring the retail LocalDefeat window
// taking precedence; observers get the neutral MATCH_OVER card instead of a
// wrong defeat. Campaign defeat needs the visible end timer (there is no
// quick-defeat action); a stale CampaignManager flag after quitting
// mid-mission must never produce a card, so post-hoc recovery is rejected.
inline void xrEndgamePoll(XrEndgameState &state, const XrEndgameInput &in) {
	if (in.interactive && (!state.wasInteractive ||
		(state.latch != XrEndgameResult::None && in.frame < state.latchFrame))) {
		xrEndgameClear(state);
	}
	state.wasInteractive = in.interactive;
	if (state.latch != XrEndgameResult::None || !in.interactive) return;
	XrEndgameResult result = XrEndgameResult::None;
	if (in.vcValid) {
		if (in.observer) {
			if (in.alliedDefeat) result = XrEndgameResult::MatchOver;
		} else if (in.localDefeat || in.alliedDefeat) {
			result = XrEndgameResult::Defeat;
		} else if (in.localVictory) {
			result = XrEndgameResult::Victory;
		}
	}
	// A scripted quick victory can end a Skirmish before VictoryConditions
	// produces a terminal flag. Its end action sets CampaignManager's result.
	if (result == XrEndgameResult::None && in.endActionValid && in.ending) {
		result = in.victorious ? XrEndgameResult::Victory : XrEndgameResult::Defeat;
	}
	if (result != XrEndgameResult::None) {
		state.latch = result;
		state.latchFrame = in.frame;
		state.dismissed = false; // A fresh result is always shown, even when
		// the dismissing press and the latch land on the same frame.
	}
}

inline void xrEndgameDismiss(XrEndgameState &state) {
	if (state.latch != XrEndgameResult::None) state.dismissed = true;
}

inline bool xrEndgameVisible(const XrEndgameState &state) {
	return state.latch != XrEndgameResult::None && !state.dismissed;
}

// Plain-float card pose: head position plus yaw-forward offset, gravity
// upright. The host converts yaw with the same xrAxisAngle({0,1,0}, yaw)
// convention placePanel uses for head-relative surfaces.
struct XrEndgameCardPose {
	float x = 0, y = 0, z = 0, yaw = 0, width = kXrEndgameCardWidthM;
};

inline XrEndgameCardPose xrEndgameCardPose(float hx, float hy, float hz,
	float fx, float fz, float distance = kXrEndgameCardDistanceM,
	float drop = kXrEndgameCardDropM, float width = kXrEndgameCardWidthM) {
	XrEndgameCardPose out;
	out.x = hx + fx * distance;
	out.y = hy - drop;
	out.z = hz + fz * distance;
	// Match the workspace yaw convention: forward (fx,fz) maps to the Y
	// rotation whose local -Z looks along it.
	out.yaw = atan2f(-fx, -fz);
	out.width = width;
	return out;
}
