// GeneralsX @feature Codex 14/09/2026 Contextual build rotation owns both
// sticks until neutral; releasing Grip cannot suddenly turn the camera.
#pragma once
#include "XrMath.h"
#include <algorithm>
struct XrBuildRotation {
	bool captured=false,triggerHeld=false;
	float update(bool pending,bool modifier,XrVector2f stick,bool trigger,float dt) {
		if(pending && modifier)captured=true;
		if(!modifier && fabsf(stick.x)<=.25f && fabsf(stick.y)<=.25f)captured=false;
		const bool frozen=trigger || triggerHeld;triggerHeld=trigger;
		if(!pending || !modifier || frozen || !std::isfinite(stick.x) || !std::isfinite(dt))return 0;
		const float axis=fabsf(stick.x)>.25f ? (stick.x>0 ? stick.x-.25f:stick.x+.25f)/.75f:0;
		return -std::clamp(axis,-1.0f,1.0f)*1.570796327f*std::clamp(dt,0.0f,.05f);
	}
};
