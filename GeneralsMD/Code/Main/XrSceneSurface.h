// GeneralsX @feature Codex 14/09/2026 Bounded, explicit room-surface placement.
// Geometry only: no engine commands, room persistence, or continuously snapping.
#pragma once
#include "XrPlacement.h"
#include <vector>
#include <string>
struct XrSceneFace {
	XrPosef pose={{0,0,0,1},{0,0,0}}; // local XY, +Z points up
	std::vector<XrVector2f> boundary;
	bool floor=false;
};
inline bool xrSceneInside(const std::vector<XrVector2f> &p,XrVector2f v) {
	if(p.size()<3 || !std::isfinite(v.x) || !std::isfinite(v.y))return false;
	bool in=false;
	for(size_t i=0,j=p.size()-1;i<p.size();j=i++) {
		const auto a=p[i],b=p[j];
		const float cross=(v.x-a.x)*(b.y-a.y)-(v.y-a.y)*(b.x-a.x);
		if(fabsf(cross)<1e-5f && v.x>=std::min(a.x,b.x)-1e-5f && v.x<=std::max(a.x,b.x)+1e-5f &&
			v.y>=std::min(a.y,b.y)-1e-5f && v.y<=std::max(a.y,b.y)+1e-5f)return true;
		if((a.y>v.y)!=(b.y>v.y) && v.x<(b.x-a.x)*(v.y-a.y)/(b.y-a.y)+a.x)in=!in;
	}
	return in;
}
inline void xrSceneRectangle(XrSceneFace &f,float x,float y,float w,float h) {
	f.boundary={{x,y},{x+w,y},{x+w,y+h},{x,y+h}};
}
inline bool xrSceneTop(const XrPosef &pose,const XrRect3DfFB &box,XrSceneFace &face) {
	const float ext[]={box.extent.width,box.extent.height,box.extent.depth};
	if(!(ext[0]>.01f && ext[1]>.01f && ext[2]>.01f))return false;
	const XrVector3f axes[]={{1,0,0},{0,1,0},{0,0,1}};
	int k=0;float best=0,sign=1;
	for(int i=0;i<3;++i) {const float y=xrRotate(pose.orientation,axes[i]).y;
		if(fabsf(y)>best){best=fabsf(y);k=i;sign=y>=0 ? 1:-1;}}
	if(best<.985f)return false; // horizontal supporting faces, not walls/slopes
	XrVector3f c={box.offset.x+ext[0]*.5f,box.offset.y+ext[1]*.5f,box.offset.z+ext[2]*.5f};
	c=xrAdd(c,xrScale(axes[k],sign*ext[k]*.5f));
	const auto normal=xrScale(axes[k],sign);
	const auto q=xrFromTo({0,0,1},normal);
	face.pose=xrPoseMul(pose,{q,c});
	const auto inv=xrConjugate(q);
	float w=0,h=0;
	for(int i=0;i<3;++i)if(i!=k){const auto v=xrRotate(inv,xrScale(axes[i],ext[i]));
		w+=fabsf(v.x);h+=fabsf(v.y);}
	xrSceneRectangle(face,-w*.5f,-h*.5f,w,h);return true;
}
inline bool xrSceneHit(const XrSceneFace &f,const XrPosef &aim,XrVector3f &hit,float &distance) {
	if(xrRotate(f.pose.orientation,{0,0,1}).y<.985f)return false;
	const auto inv=xrPoseInverse(f.pose);
	const auto o=xrPoseMul(inv,aim).position;
	const auto d=xrRotate(xrMul(inv.orientation,aim.orientation),{0,0,-1});
	if(d.z>=-.0001f)return false;
	const float t=-o.z/d.z;
	if(!std::isfinite(t) || t<0 || t>5)return false;
	const XrVector2f p={o.x+t*d.x,o.y+t*d.y};
	if(!xrSceneInside(f.boundary,p))return false;
	hit=xrAdd(f.pose.position,xrRotate(f.pose.orientation,{p.x,p.y,0}));distance=t;return true;
}
inline XrQuaternionf xrSceneBoardYaw(const XrSurface &board) {
	const auto right=xrRotate(board.pose.orientation,{1,0,0});
	return xrAxisAngle({0,1,0},atan2f(-right.z,right.x));
}
inline XrSurface xrSceneBoard(const XrSurface &old,XrVector3f point,float width) {
	XrSurface result=old;result.width=width;
	result.pose.orientation=xrMul(xrSceneBoardYaw(old),xrAxisAngle({1,0,0},-1.5707963268f));
	// P18 plinth underside is -0.036 widths, NOT the terrain's variable center.
	result.pose.position=xrAdd(point,{0,.036f*width+.002f,0});return result;
}
inline XrSurface xrSceneSupportedBoard(const XrSceneFace &face,const XrSurface &old,XrVector3f point,float width,float aspect) {
	auto board=xrSceneBoard(old,point,width);
	const auto normal=xrRotate(face.pose.orientation,{0,0,1});
	float lift=0;
	for(int x:{-1,1})for(int y:{-1,1}) {
		const auto offset=xrRotate(board.pose.orientation,{x*.512f*width,y*(aspect*.5f+.012f)*width,0});
		lift=std::max(lift,-(normal.x*offset.x+normal.z*offset.z)/std::max(normal.y,.5f));
	}
	// A small slope in room estimation must not push a level plinth into it.
	board.pose.position.y+=lift;return board;
}
inline bool xrSceneFits(const XrSceneFace &f,const XrSurface &board,float aspect) {
	if(!(board.width>=.45f && board.width<=4 && aspect>0))return false;
	const auto inv=xrPoseInverse(f.pose);
	// Sample the complete perimeter, not just four corners of concave floors.
	for(int edge=0;edge<4;++edge)for(int i=0;i<=32;++i) {
		const float a=-.512f+1.024f*i/32;
		const XrVector3f local=edge<2 ? XrVector3f{a,edge==0 ? -.5f*aspect-.012f:.5f*aspect+.012f,0}:
			XrVector3f{edge==2 ? -.512f:.512f,a*(aspect+.024f)/1.024f,0};
		const auto p=xrPoseMul(inv,{board.pose.orientation,xrAdd(board.pose.position,xrRotate(board.pose.orientation,xrScale(local,board.width)))}).position;
		if(!xrSceneInside(f.boundary,{p.x,p.y}))return false;
	}
	return true;
}
inline void xrSceneMoveWorkspace(XrSurface surfaces[3],const XrSurface &next) {
	const auto old=surfaces[1];
	const auto rotation=xrMul(xrSceneBoardYaw(next),xrConjugate(xrSceneBoardYaw(old)));
	// Move companions rigidly, retaining their individual pitch and scale.
	surfaces[2].pose.position=xrAdd(next.pose.position,xrRotate(rotation,xrSub(surfaces[2].pose.position,old.pose.position)));
	surfaces[2].pose.orientation=xrMul(rotation,surfaces[2].pose.orientation);
	surfaces[1]=next;
}
