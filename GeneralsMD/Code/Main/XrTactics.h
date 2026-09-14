// GeneralsX @feature Codex 13/09/2026 Controller tactics use native orders.
#pragma once
#include "XrWorld.h"
#include "XrStrings.h"
// GeneralsX @bugfix Codex 14/09/2026 Measure intent on a plane facing the
// press ray, not grazing terrain. Tiny angular jitter must not become a box.
struct XrRayDrag {
	XrVector3f normal={0,0,-1},center={};
	void begin(const XrPosef &aim,XrVector3f hit) {
		normal=xrRotate(aim.orientation,{0,0,-1});
		const float depth=std::clamp(xrDot(xrSub(hit,aim.position),normal),.25f,1.25f);
		center=xrAdd(aim.position,xrScale(normal,depth));
	}
	XrVector3f point(const XrPosef &aim) const {
		const auto direction=xrRotate(aim.orientation,{0,0,-1});
		const float denominator=xrDot(direction,normal);
		if(!std::isfinite(denominator) || denominator<.1f)return {NAN,NAN,NAN};
		const float distance=xrDot(xrSub(center,aim.position),normal)/denominator;
		if(distance<0)return {NAN,NAN,NAN};
		return xrSub(xrAdd(aim.position,xrScale(direction,distance)),center);
	}
};
// A half-board per second at full stick; independent of render resolution.
inline XrVector3f xrWorldPan(const float *mapping,float span,float right,float forward) {
	if(!std::isfinite(span+right+forward) || span<=0)return {};
	const float n=sqrtf(mapping[0]*mapping[0]+mapping[4]*mapping[4]);
	if(!std::isfinite(n) || n<1e-8f)return {};
	right=std::clamp(right,-.05f,.05f);forward=std::clamp(forward,-.05f,.05f);
	// Clamp the diagonal so it is not faster than a cardinal direction.
	const float length=sqrtf(right*right+forward*forward);
	if(length>.05f){right*=.05f/length;forward*=.05f/length;}
	return {(mapping[0]*right+mapping[1]*forward)/n*span*.5f,
		(mapping[4]*right+mapping[5]*forward)/n*span*.5f,0};
}
// GeneralsX @feature Codex 13/09/2026 Deferred trigger intent: never issue
// an order at press; cancel cannot turn into a click on reacquisition.
enum class XrTriggerEvent {None,Begin,Click,Drag,Drop,Cancel};
struct XrTriggerGesture {
	bool held=false,armed=false,active=false,dragging=false,canDrag=false,add=false;
	XrVector3f start={};
	XrTriggerEvent update(bool down,bool available,XrVector3f point,bool allowDrag,bool additive) {
		if(!available || !std::isfinite(point.x+point.y+point.z)) {
			const bool pending=active;held=down;active=false;armed=false;dragging=false;
			return pending ? XrTriggerEvent::Cancel:XrTriggerEvent::None;
		}
		if(down && !held) {
			held=true;if(!armed)return XrTriggerEvent::None;
			active=true;dragging=false;canDrag=allowDrag;add=additive;start=point;
			return XrTriggerEvent::Begin;
		}
		if(down && active) {
			const auto delta=xrSub(point,start);
			if(canDrag && xrDot(delta,delta)>=.02f*.02f) dragging=true;
			return dragging ? XrTriggerEvent::Drag:XrTriggerEvent::None;
		}
		if(!down) {
			// Release may be the first sample beyond the drag threshold.
			const auto delta=xrSub(point,start);
			if(active && canDrag && xrDot(delta,delta)>=.02f*.02f)dragging=true;
			const auto result=active ? (dragging ? XrTriggerEvent::Drop:XrTriggerEvent::Click):XrTriggerEvent::None;
			held=false;active=false;armed=true;dragging=false;return result;
		}
		return XrTriggerEvent::None;
	}
};
enum class XrOrderMode {Context,Select,Add,Box,BoxAdd,Move,AttackMove,ForceAttack,Guard,ForceMove,GuardHold};
// GeneralsX @feature Codex 14/09/2026 Make the pending target step explicit.
inline const char *xrOrderHint(XrOrderMode mode,int selected) {
	if(mode==XrOrderMode::Context)return "";
	if(int(mode)>=int(XrOrderMode::Move) && selected<=0)return "Zuerst eine eigene Einheit auswählen";
	switch(mode) {
	case XrOrderMode::Guard:return "Bewachen: Boden oder verbündetes Objekt anklicken";
	case XrOrderMode::GuardHold:return "Ohne Verfolgung: Bodenposition anklicken";
	case XrOrderMode::ForceMove:return "Zwangsbewegung: Bodenposition anklicken";
	case XrOrderMode::Move:return "Bewegen: Ziel auf dem Tisch anklicken";
	case XrOrderMode::AttackMove:return "Angriffsmarsch: Zielposition anklicken";
	case XrOrderMode::ForceAttack:return "Zwangsangriff: Ziel auf dem Tisch anklicken";
	case XrOrderMode::Box:case XrOrderMode::BoxAdd:return "Bereich: zwei Ecken auf dem Tisch anklicken";
	default:return "Einheit auf dem Tisch anklicken";
	}
}
struct XrTactics {
	XrOrderMode mode=XrOrderMode::Context;
	bool queue=false,cornerKnown=false;
	XrVector3f corner={};
	int group=0;
	void setMode(XrOrderMode value){mode=value;cornerKnown=false;}
	void cancel(){setMode(XrOrderMode::Context);queue=false;}
};
inline bool xrSelectionHasRoom(int selected,int limit) {return limit<=0 || selected<limit;}
inline bool xrSelectionContains(XrVector3f first,XrVector3f second,XrVector3f point) {
	return std::isfinite(first.x) && std::isfinite(first.y) && std::isfinite(second.x) && std::isfinite(second.y) &&
		std::isfinite(point.x) && std::isfinite(point.y) &&
		point.x>=std::min(first.x,second.x) && point.x<=std::max(first.x,second.x) &&
		point.y>=std::min(first.y,second.y) && point.y<=std::max(first.y,second.y);
}
