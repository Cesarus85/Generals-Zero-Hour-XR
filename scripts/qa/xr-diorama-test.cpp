// GeneralsX @test Codex 13/09/2026 Stereo height, board anchoring and mesh checks.
// clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined
// -IGeneralsMD/Code/Main -I<openxr-include> this-file.cpp -o <temporary-test>
#include "XrDiorama.h"
#include <cstdio>
#include <cstdlib>
static int checks=0;
static void check(bool b) {++checks;if(!b){fprintf(stderr,"FAIL diorama check %d\n",checks);exit(1);}}
static void near(float a,float b) {check(std::isfinite(a)&&fabsf(a-b)<.00001f);}
static XrVector3f transform(const float *m,XrVector3f p) {
	const float w=m[3]*p.x+m[7]*p.y+m[11]*p.z+m[15];
	return {(m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12])/w,
		(m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13])/w,(m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14])/w};
}
static XrVector3f project(XrSurface board,XrPosef eye,XrVector3f p) {
	float m[16],v[16],projection[16],vm[16],mvp[16];
	xrDioramaMatrix(board,m);matViewFromPose(v,eye);
	matPerspectiveFromFov(projection,{-.75f,.70f,.72f,-.68f},.05f,100);
	matMultiply(vm,v,m);matMultiply(mvp,projection,vm);return transform(mvp,p);
}
int main() {
	XrSurface board;board.pose={xrAxisAngle({1,0,0},-1.570796327f),{0,-.45f,-.7f}};
	for(float width:{.45f,1.0f,1.1f,2.5f}) {
		board.width=width;float m[16];xrDioramaMatrix(board,m);
		const auto floor=transform(m,{0,0,0}),top=transform(m,{0,0,.1f});
		near(top.y-floor.y,width*.1f);near(top.x,floor.x);near(top.z,floor.z);
		near(xrLength(xrSub(transform(m,{.5f,0,0}),transform(m,{-.5f,0,0}))),width);
	}
	board.width=1;
	XrPosef eyes[2]={{xrAxisAngle({1,0,0},-.55f),{-.032f,0,0}},{xrAxisAngle({1,0,0},-.55f),{.032f,0,0}}};
	const auto f0=project(board,eyes[0],{}),f1=project(board,eyes[1],{});
	const auto t0=project(board,eyes[0],{0,0,.1f}),t1=project(board,eyes[1],{0,0,.1f});
	check(f0.x>f1.x);check(t0.x-t1.x>f0.x-f1.x);check(t0.y>f0.y);
	near(t0.y,t1.y);near(t0.z,t1.z);
	// Moving the common room anchor changes neither screen projection nor depth.
	const XrPosef anchor={xrMul(xrAxisAngle({0,1,0},.7f),xrAxisAngle({0,0,1},.15f)),{1,1.3f,-.4f}};
	XrSurface relocated=board;relocated.pose=xrPoseMul(anchor,board.pose);
	const auto invariant=project(relocated,xrPoseMul(anchor,eyes[0]),{0,0,.1f});
	near(invariant.x,t0.x);near(invariant.y,t0.y);near(invariant.z,t0.z);
	// Leaning changes the view of raised geometry, without moving the board.
	eyes[0].position.x+=.2f;const auto lean=project(board,eyes[0],{0,0,.1f});check(fabsf(lean.x-t0.x)>.1f);
	const auto mesh=xrBuildDiorama();check(mesh.vertices.size()>1000 && mesh.vertices.size()<20000);
	check(mesh.vertices.size()%3==0);
	bool valid=true;float maxHeight=0;
	for(const auto &v:mesh.vertices) {
		valid=valid && std::isfinite(xrLength(v.position)) && fabsf(v.position.x)<.53f && fabsf(v.position.y)<.33f &&
			v.position.z>=-.028f && v.position.z<.15f && fabsf(xrLength(v.normal)-1)<.00001f &&
			v.color.x>=0 && v.color.x<=1 && v.color.y>=0 && v.color.y<=1 && v.color.z>=0 && v.color.z<=1;
		maxHeight=std::max(maxHeight,v.position.z);
	}
	check(valid);check(maxHeight>.12f);
	// Only the sand, not the dark plinth, may have a full-board top at z=0.
	int fullGroundFaces=0;
	for(size_t i=0;i<mesh.vertices.size();i+=3) {
		const auto &a=mesh.vertices[i],&b=mesh.vertices[i+1],&c=mesh.vertices[i+2];
		const float area=xrLength(xrCross(xrSub(b.position,a.position),xrSub(c.position,a.position)))*.5f;
		if(a.normal.z>.99f && fabsf(a.position.z)<.000001f && area>.2f) ++fullGroundFaces;
	}
	check(fullGroundFaces==2);
	printf("PASS %d stereo diorama checks (%zu triangles validated)\n",checks,mesh.vertices.size()/3);
}
