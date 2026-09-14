// GeneralsX @feature Codex 14/09/2026 Physical hands become stable input roles.
#pragma once
#include "XrMath.h"
struct XrPhysicalHand {
	XrPosef aim={{0,0,0,1},{0,0,0}},pose={{0,0,0,1},{0,0,0}};
	bool aimValid=false,poseValid=false,trigger=false,grip=false;
	bool lower=false,upper=false,stickClick=false,lowerEdge=false,upperEdge=false,stickEdge=false;
	XrVector2f stick={};
};
struct XrControllerState {
	XrPosef aim={{0,0,0,1},{0,0,0}};
	bool aimValid=false,select=false,secondary=false,back=false,recenter=false,upright=false;
	XrVector2f pan={},zoom={};
	bool arrange=false,preset=false,tilt=false,buttonsHeld=false;
	// Index 0 = supporting hand; index 1 = pointing hand, in BOTH modes.
	XrPosef hands[2]={{{0,0,0,1},{0,0,0}},{{0,0,0,1},{0,0,0}}};
	bool grip[2]={},handValid[2]={};
};
inline XrControllerState xrMapHands(const XrPhysicalHand (&hands)[2],bool leftHanded,bool menu) {
	const auto &dominant=hands[leftHanded ? 0:1],&support=hands[leftHanded ? 1:0];
	XrControllerState out;
	out.aim=dominant.aim;out.aimValid=dominant.aimValid;out.select=dominant.trigger;
	out.secondary=dominant.grip;out.back=dominant.upper || menu;
	out.arrange=dominant.lowerEdge;out.preset=dominant.stickEdge;
	out.tilt=support.trigger;out.recenter=support.lowerEdge;out.upright=support.upperEdge;
	out.pan=support.stick;out.zoom=dominant.stick;
	out.hands[0]=support.pose;out.hands[1]=dominant.pose;
	out.grip[0]=support.grip;out.grip[1]=dominant.grip;
	out.handValid[0]=support.poseValid;out.handValid[1]=dominant.poseValid;
	out.buttonsHeld=menu;
	for(const auto &h:hands)out.buttonsHeld=out.buttonsHeld || h.lower || h.upper || h.stickClick;
	return out;
}
