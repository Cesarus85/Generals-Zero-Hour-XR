// GeneralsX @test Codex 13/09/2026 Actual game-coordinate to eye projection contract.
#include "XrWorld.h"
#include "XRStereoPolicy.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
static int checks=0;
static void check(bool b){++checks;if(!b){fprintf(stderr,"world check %d failed\n",checks);exit(1);}}
static void near(float a,float b){check(std::isfinite(a)&&fabsf(a-b)<.00002f);}
static XrVector3f project(const float *m,XrVector3f p){
	const float w=m[3]*p.x+m[7]*p.y+m[11]*p.z+m[15];
	return {(m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12])/w,(m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13])/w,(m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14])/w};
}
int main(){
	near(xrMapCoverage(1,1.1f),1);near(xrMapCoverage(1,2.2f),2);
	near(xrMapCoverage(2,2.2f),4);near(xrMapCoverage(9,1.1f),3);
	float bounds[16];check(xrWorldToBoard(bounds,{1000,500,10},{1,0,0},1000));
	check(xrBoardContainsSphere(bounds,.5f,{1000,500,10},0));
	check(xrBoardContainsSphere(bounds,.5f,{1510,500,10},20));
	check(!xrBoardContainsSphere(bounds,.5f,{1530,500,10},20));
	check(!xrBoardContainsSphere(bounds,.5f,{1000,800,10},20));
	check(xrBoardContainsSphere(bounds,.5f,{1000,770,10},25));
	int width=0,height=0;
	xrStereoExtent(2064,2160,width,height);check(width==1920 && height==2009);
	xrStereoExtent(2064,2160,width,height,false);check(width==1536 && height==1607);
	xrStereoExtent(2064,2160,width,height,2);check(width==2304 && height==2411);
	xrStereoExtent(1000,2000,width,height);check(width==1024 && height==2048);
	xrStereoExtent(0,0,width,height);check(width==1920 && height==1920);
	for(const XrVector3f end:{XrVector3f{0,0,-2},XrVector3f{0,2,0},XrVector3f{1,1,-1}}) {
		float ribbon[16];check(xrRayRibbon(ribbon,{},end,{0,0,0},.5625f));
		const auto a=xrTransformPoint(ribbon,{0,-.28125f,0}),b=xrTransformPoint(ribbon,{0,.28125f,0});
		near(xrLength(a),0);near(xrLength(xrSub(b,end)),0);
		near(xrLength({ribbon[0],ribbon[1],ribbon[2]}),.0025f);
	}
	for(unsigned mask=0;mask<16;++mask) {
		check(gxXrWorldColorPass(mask,false,false)==((mask&7)!=0));
		check(gxXrWorldColorPass(mask,false,true)==((mask&7)!=0));
		check(gxXrWorldColorPass(mask,true,true)==((mask&7)!=0));
		check(!gxXrWorldColorPass(mask,true,false));
	}
	float m[16];const XrVector3f center={1200,700,30};
	check(xrWorldToBoard(m,center,{3,4,0},500));const auto origin=project(m,center);near(origin.x,0);near(origin.y,0);near(origin.z,0);
	const auto right=project(m,xrAdd(center,{150,200,0}));near(right.x,.5f);near(right.y,0);
	const auto up=project(m,xrAdd(center,{-200,150,0}));near(up.x,0);near(up.y,.5f);
	near(project(m,xrAdd(center,{0,0,50})).z,.1f);
	check(!xrWorldToBoard(m,center,{},500));check(!xrWorldToBoard(m,center,{1,0,0},0));
	check(!xrWorldToBoard(m,center,{1,0,0},std::numeric_limits<float>::quiet_NaN()));
	XrWorldFrame f;f.board.pose={xrAxisAngle({1,0,0},-1.570796327f),{0,-.45f,-.7f}};
	check(xrWorldToBoard(m,center,{1,0,0},500));
	for(float width:{.45f,1.0f,2.5f}) {
		f.board.width=width;float clip[2][16];
		for(int eye=0;eye<2;++eye){f.eyes[eye]={xrAxisAngle({1,0,0},-.55f),{eye==0 ? -.032f:.032f,0,0}};
			f.fov[eye]={-.7f,.7f,.7f,-.7f};xrWorldEyeClip(clip[eye],f,eye,m);}
		const auto floor0=project(clip[0],center),floor1=project(clip[1],center);
		const auto top0=project(clip[0],xrAdd(center,{0,0,50})),top1=project(clip[1],xrAdd(center,{0,0,50}));
		check(top0.x-top1.x>floor0.x-floor1.x);check(top0.y>floor0.y);near(top0.y,top1.y);check(top0.z<floor0.z);
	}
	// Spatial rays use the same similarity transform as the renderer.
	for(float yaw:{0.0f,.8f,-1.3f}) for(float width:{.45f,1.1f,2.5f}) {
		XrSurface board;board.width=width;
		board.pose={xrMul(xrAxisAngle({0,1,0},yaw),xrAxisAngle({1,0,0},-1.570796327f)),{.3f,-.6f,-1.1f}};
		check(xrWorldToBoard(m,center,{3,4,0},700));
		const XrVector3f topLocal={.1f,-.1f,1};
		XrPosef aim={board.pose.orientation,xrAdd(board.pose.position,xrRotate(board.pose.orientation,xrScale(topLocal,width)))};
		XrVector3f a,b;check(xrWorldRay(board,.5f,m,aim,a,b));
		const auto localA=xrTransformPoint(m,a),localB=xrTransformPoint(m,b);
		near(localA.x,.1f);near(localA.y,-.1f);near(localA.z,gxXrBoardCeiling(m));near(localB.z,-.15f);
		const auto roundtrip=xrTransformPoint(m,xrInversePoint(m,{.2f,.1f,.15f}));
		near(roundtrip.x,.2f);near(roundtrip.y,.1f);near(roundtrip.z,.15f);
		aim.position=xrAdd(board.pose.position,xrRotate(board.pose.orientation,{2*width,0,width}));
		check(!xrWorldRay(board,.5f,m,aim,a,b));
		aim.position=xrAdd(board.pose.position,xrRotate(board.pose.orientation,{0,0,width}));
		aim.orientation=xrMul(board.pose.orientation,xrAxisAngle({0,1,0},3.14159265f));
		check(!xrWorldRay(board,.5f,m,aim,a,b));
	}
	printf("PASS %d P7 world projection/picking checks\n",checks);
}
