// GeneralsX @feature Codex 13/09/2026 Shared crop and frame-delayed pointer routing.
#pragma once
#include <cmath>
#include <algorithm>
struct XrGameRect {
	float x=0,y=0,w=1,h=1; // GL bottom-up UVs
};
inline XrGameRect xrViewportRect(int x,int y,int w,int h,int width,int height) {
	if(width<=0 || height<=0 || x<0 || y<0 || w<=0 || h<=0 || x>width-w || y>height-h) return {};
	return {float(x)/width,1.0f-float(y+h)/height,float(w)/width,float(h)/height};
}
// GeneralsX @feature Codex 13/09/2026 Compact command band; the complement is
// still rendered as a transparent HUD/popover area, never silently discarded.
inline XrGameRect xrCommandRect(float topFromBottom) {
	const float height=std::isfinite(topFromBottom) && topFromBottom>.05f && topFromBottom<.65f ?
		topFromBottom:.32f;
	return {0,0,1,std::clamp(height+.025f,.18f,.65f)};
}
inline XrGameRect xrAboveCommandRect(XrGameRect bar) { return {0,bar.h,1,1-bar.h}; }
// GeneralsX @bugfix Codex 14/09/2026 Full native dialog canvas only while
// needed. Compact build bar and expanded UI share their physical bottom edge.
inline XrGameRect xrUIPieceRect(int piece,XrGameRect bar,bool expanded) {
	if(piece==2)return expanded ? XrGameRect{}:bar;
	return expanded ? XrGameRect{0,1,1,0}:xrAboveCommandRect(bar);
}
inline float xrUIBandOffset(XrGameRect band,XrGameRect bar,float fullAspect) {
	return (band.y+band.h*.5f-bar.h*.5f)*fullAspect;
}
// Events queued by the host are consumed by the next engine frame. Keep the
// OLD route for one neutral frame so button-up reaches its original recipient.
struct XrPointerRoute {
	int source=-1,candidate=-2;
	bool update(int desired,bool held) {
		if(desired==source) { candidate=source; return true; }
		if(!held && candidate==desired) source=desired;
		candidate=desired;
		return false;
	}
};

// GeneralsX @feature Codex 13/09/2026 World commands fire only on release
// after a press remained on the world. Crossing UI/missing tracking cancels.
struct XrWorldClick {
	bool held=false,armed=false,valid=false;
	bool update(bool down,bool world) {
		if(!down && !held) armed=true;
		if(down && !held) {valid=armed && world;armed=false;}
		if(!world) valid=false;
		const bool fire=held && !down && valid && world;
		held=down;if(!down) valid=false;return fire;
	}
	void cancel() {valid=false;armed=false;}
};
