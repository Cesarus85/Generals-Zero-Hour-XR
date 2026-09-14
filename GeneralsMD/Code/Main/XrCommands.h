// GeneralsX @feature Codex 13/09/2026 Direct command console, shared hit layout.
#pragma once
#include "XrMenu.h"
struct XrCommandState {XrMenuState input;int groupOperation=0;bool help=false;int helpPage=0;bool tactics=false,bookmarkSave=false;};
// GeneralsX @feature Codex 14/09/2026 Foldout grows downward; old controls
// retain their size and top anchor. Render, picking and Canvas share height.
inline int xrCommandHeight(const XrCommandState &s){return s.tactics && !s.help ? 1280:1024;}
inline int xrCommandHit(float u,float v,bool help=false,bool tactics=false) {
	if(!std::isfinite(u+v))return -1;
	const float x=u*768,y=(1-v)*(tactics && !help ? 1280:1024);
	if(x>=664 && x<736 && y>=36 && y<92)return 33;
	if(x>=32 && x<736 && y>=930 && y<994) {
		if(x<376)return help ? 34:35;
		if(help && x>=392)return 36;
		if(x>=392 && x<556)return help ? 36:37;
		if(x>=572)return help ? 36:34;
	}
	if(help)return -1;
	if(tactics && x>=32 && x<736 && y>=1024 && y<1152) {
		const int row=int((y-1024)/64),col=x>=392 ? 1:0;
		if((col==0 && x>=376) || y>=1024+row*64+56)return -1;
		return 40+row*2+col;
	}
	if(tactics && x>=32 && x<736 && y>=1160 && y<1216) {
		const int col=int((x-32)/178);return col<4 && x<32+col*178+168 ? 44+col:-1;
	}
	if(x>=32 && x<736 && y>=144 && y<656) {
		const int row=int((y-144)/64),col=x>=392 ? 1:0;
		if((col==0 && x>=376) || y>=144+row*64+56)return -1;
		return row*2+col;
	}
	if(x>=32 && x<736 && y>=704 && y<832) {
		const int row=int((y-704)/64),col=int((x-32)/142);
		if(col>=5 || x>=32+col*142+134 || y>=704+row*64+56)return -1;
		return 20+row*5+col;
	}
	if(x>=32 && x<736 && y>=850 && y<910) {
		const int col=int((x-32)/238);return col<3 && x<32+col*238+228 ? 30+col:-1;
	}
	return -1;
}
inline int xrCommandAction(int hit) {
	const int actions[]={0,5,6,7,8,10,11,12,9,30,31,28,29,26,27,13};
	return hit>=0 && hit<16 ? actions[hit]:-1;
}
