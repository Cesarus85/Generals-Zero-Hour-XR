// GeneralsX @feature Codex 13/09/2026 Shared menu layout/input contract.
#pragma once
#include "XrLayers.h"
#include "XrPlacement.h"
#include "XrControllerHelp.h"
constexpr int kXrMenuWidth=768,kXrMenuHeight=1024;
constexpr int kXrMenuRows=9,kXrMenuButtons=18;
// GeneralsX @feature Codex 14/09/2026 Dedicated sequential setup, no inert tabs.
inline int xrSceneMenuHit(float u,float v) {
	if(!std::isfinite(u) || !std::isfinite(v))return -1;
	const float x=u*kXrMenuWidth,y=(1-v)*kXrMenuHeight;
	if(x<32 || x>=736)return -1;
	if(y>=900 && y<972)return x<376 ? 16:x>=392 ? 17:-1;
	if(y<300 || y>=792)return -1;
	const int row=int((y-300)/82);
	return y<300+row*82+68 ? row:-1;
}
inline int xrMenuHit(float u,float v) {
	if(!std::isfinite(u) || !std::isfinite(v)) return -1;
	const float x=u*kXrMenuWidth,y=(1-v)*kXrMenuHeight;
	if(x>=664 && x<736 && y>=18 && y<54)return 24; // Controller guide.
	if(y>=108 && y<146 && x>=32 && x<734) {
		const int tab=int((x-32)/178);return x<32+tab*178+168 ? 20+tab:-1;
	}
	if(x<32 || x>=736 || y<160 || y>=970) return -1;
	const int row=int((y-160)/90),column=x>=392 ? 1:0;
	if((column==0 && x>=376) || y>=160+row*90+72) return -1;
	return row*2+column;
}
struct XrMenuState {
	bool open=false;int hover=-1,pressed=-1,target=1,page=0,helpPage=0;
	XrWorldClick click;XrSurface surface;
	bool update(bool down,int hit,bool available=true) {
		if(!available) {click.update(false,false);click.cancel();pressed=-1;return false;}
		if(down && !click.held) pressed=hit;
		return click.update(down,hit>=0 && (pressed==hit || (!down && !click.held)));
	}
};
// GeneralsX @feature Codex 14/09/2026 Single selection contract for button
// artwork and room-space outline; no highlight outside window editing.
template<class Workspace> int xrEditTarget(const Workspace &x) {
	if(x.arranging)return x.splitVisible ? x.arrangeSlot:0;
	return x.menu.open && x.menu.page==0 ? (x.splitVisible ? x.menu.target:0):-1;
}
inline XrSurface xrEditOutline(XrSurface s,float &aspect,bool board) {
	if(board) {
		s.pose.position=xrAdd(s.pose.position,xrRotate(s.pose.orientation,{0,0,-.017f*s.width}));
		aspect=(aspect+.024f)/1.024f;s.width*=1.024f;
	} else s.pose.position=xrAdd(s.pose.position,xrRotate(s.pose.orientation,{0,0,.003f}));
	return s;
}
