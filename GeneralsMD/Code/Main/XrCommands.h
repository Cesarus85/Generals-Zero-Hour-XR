// GeneralsX @feature Codex 13/09/2026 Direct command console, shared hit layout.
#pragma once
#include "XrMenu.h"
#include "XrPanelLayout.h"
struct XrCommandState {XrMenuState input;int groupOperation=0;bool help=false;int helpPage=0;bool tactics=false,bookmarkSave=false;};
// GeneralsX @feature Codex 14/09/2026 Foldout grows downward; old controls
// retain their size and top anchor. Render, picking and Canvas share height.
inline int xrCommandHeight(const XrCommandState &s){return s.tactics && !s.help ? kXrPanelHeightTall:kXrPanelHeight;}
// GeneralsX @refactor Ultron 15/09/2026 P21 hit testing now reads the single
// shared control table (XrPanelLayout.h); Canvas pixels cannot drift from UV
// geometry because both are generated from the same rects.
inline int xrCommandHit(float u,float v,bool help=false,bool tactics=false) {
	XrPanelControl table[64];
	const int count=xrCommandLayout(help,tactics,table,64);
	return xrPanelHit(table,count,u,v,tactics && !help ? kXrPanelHeightTall:kXrPanelHeight);
}
inline int xrCommandAction(int hit) {
	const int actions[]={0,5,6,7,8,10,11,12,9,30,31,28,29,26,27,13};
	return hit>=0 && hit<16 ? actions[hit]:-1;
}
