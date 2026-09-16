// GeneralsX @feature Codex 13/09/2026 Shared menu layout/input contract.
#pragma once
#include "XrLayers.h"
#include "XrPlacement.h"
#include "XrBoardGeometry.h"
#include "XrControllerHelp.h"
#include "XrPanelLayout.h"
constexpr int kXrMenuWidth=kXrPanelWidth,kXrMenuHeight=kXrPanelHeight;
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
// GeneralsX @refactor Ultron 15/09/2026 P21 menu hit testing reads the
// single shared control table for the active page (XrPanelLayout.h).
inline int xrMenuHit(float u,float v,int page=0) {
	XrPanelControl table[80];
	const int count=xrMenuLayout(page,table,80);
	return xrPanelHit(table,count,u,v,kXrMenuHeight);
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
		// GeneralsX @tweak Codex 16/09/2026 P20.1 follows the shared physical
		// underside instead of retaining a separate P18 depth literal.
		s.pose.position=xrAdd(s.pose.position,xrRotate(s.pose.orientation,
			{0,0,(kXrBoardUnderside+kXrBoardOutlineClearance)*s.width}));
		aspect=(aspect+.024f)/1.024f;s.width*=1.024f;
	} else s.pose.position=xrAdd(s.pose.position,xrRotate(s.pose.orientation,{0,0,.003f}));
	return s;
}
