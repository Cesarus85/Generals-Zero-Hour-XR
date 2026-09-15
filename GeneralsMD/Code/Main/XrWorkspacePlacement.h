// GeneralsX @bugfix Codex 15/09/2026 One workspace frame for match entry/reset.
#pragma once
#include "XrLayout.h"

inline XrPosef xrWorkspaceHeading(const XrView *views) {
	float fx=0,fz=-1;yawForwardFromQuat(views[0].pose.orientation,&fx,&fz);
	return {xrAxisAngle({0,1,0},atan2f(-fx,-fz)),
		xrScale(xrAdd(views[0].pose.position,views[1].pose.position),.5f)};
}

// GeneralsX @bugfix Codex 15/09/2026 Place once per process, never at delayed
// match/camera readiness. Looking elsewhere during loading cannot move a board.
inline bool xrInitializeWorkspace(bool &known,const XrLayout &layout,
	XrSurface surfaces[3],XrPosef &anchor,const XrView *views) {
	if(known)return false;
	anchor=xrWorkspaceHeading(views);
	for(int i=0;i<3;++i) {
		surfaces[i]=layout.relative[i];
		surfaces[i].pose=xrPoseMul(anchor,surfaces[i].pose);
	}
	known=true;return true;
}

inline void xrRecenterWorkspace(XrSurface surfaces[3],XrPosef &anchor,
	XrSurface &menu,const XrView *views) {
	// Retain all relative distances, orientations and scales. In particular,
	// resetting while seated must not move only the board under a distant HUD.
	const auto head=xrWorkspaceHeading(views);const XrLayout defaults;
	const auto right=xrRotate(surfaces[1].pose.orientation,{1,0,0});
	const auto yaw=xrAxisAngle({0,1,0},atan2f(-right.z,right.x));
	const auto rotation=xrMul(head.orientation,xrConjugate(yaw));
	const auto target=xrPoseMul(head,defaults.relative[1].pose).position;
	const XrPosef delta={rotation,xrSub(target,xrRotate(rotation,surfaces[1].pose.position))};
	for(int i=0;i<3;++i)surfaces[i].pose=xrPoseMul(delta,surfaces[i].pose);
	anchor=xrPoseMul(delta,anchor);menu.pose=xrPoseMul(delta,menu.pose);
}
