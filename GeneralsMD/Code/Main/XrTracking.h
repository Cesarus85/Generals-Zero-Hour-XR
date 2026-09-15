// GeneralsX @bugfix Codex 15/09/2026 Valid estimates are not tracked poses.
#pragma once
#include <openxr/openxr.h>
inline bool xrTrackedViews(XrViewStateFlags flags) {
	constexpr auto required=XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT |
		XR_VIEW_STATE_POSITION_TRACKED_BIT | XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
	return (flags & required)==required;
}
inline bool xrTrackedSpace(XrSpaceLocationFlags flags) {
	constexpr auto required=XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
		XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
	return (flags & required)==required;
}
