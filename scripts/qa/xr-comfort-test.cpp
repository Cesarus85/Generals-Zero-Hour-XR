// GeneralsX @test Codex 14/09/2026 Hand roles, shallow-ray intent and board pan.
#include "XrHandedness.h"
#include "XrTactics.h"
#include "XrLayout.h"
#include <cstdio>
#include <cstdlib>
static int checks=0;
static void check(bool value){++checks;if(!value){fprintf(stderr,"comfort check %d failed\n",checks);exit(1);}}
static bool near(float a,float b){return fabsf(a-b)<.0001f;}
int main(int argc,char **argv) {
	check(argc==2);
	for(bool left:{false,true}) for(int physical=0;physical<2;++physical) {
		XrPhysicalHand hands[2];auto &h=hands[physical];
		h.aim.position.x=10+physical;h.pose.position.x=20+physical;
		h.aimValid=h.poseValid=h.trigger=h.grip=true;h.stick={.7f,-.4f};
		h.lower=h.upper=h.stickClick=h.lowerEdge=h.upperEdge=h.stickEdge=true;
		const auto c=xrMapHands(hands,left,false);const bool dominant=physical==(left ? 0:1);
		check(c.aimValid==dominant && c.select==dominant && c.secondary==dominant);
		check(c.back==dominant && c.arrange==dominant && c.preset==dominant);
		check(c.tilt!=dominant && c.recenter!=dominant && c.upright!=dominant);
		check(near(c.pan.x,dominant ? 0:.7f) && near(c.zoom.y,dominant ? -.4f:0));
		check(c.grip[1]==dominant && c.grip[0]!=dominant);
		check(c.handValid[1]==dominant && c.handValid[0]!=dominant && c.buttonsHeld);
		check(near(c.hands[dominant ? 1:0].position.x,20+physical));
		if(dominant)check(near(c.aim.position.x,10+physical));
		h.lowerEdge=h.upperEdge=h.stickEdge=false;
		const auto held=xrMapHands(hands,left,false);check(held.buttonsHeld && !held.arrange && !held.preset && !held.recenter && !held.upright);
	}
	XrPhysicalHand neutral[2];check(xrMapHands(neutral,true,true).back);check(!xrMapHands(neutral,false,false).buttonsHeld);
	// Identical intent for near-horizontal and downward rays, any board tilt.
	for(float angle:{0.0f,.1f,.7f,1.5f}) for(float depth:{.4f,1.0f,3.0f}) {
		XrPosef aim={xrAxisAngle({1,0,0},angle),{0,0,1}};
		const auto hit=xrAdd(aim.position,xrRotate(aim.orientation,{0,0,-depth}));
		XrRayDrag metric;metric.begin(aim,hit);XrTriggerGesture gesture;
		check(gesture.update(false,true,metric.point(aim),true,false)==XrTriggerEvent::None);
		check(gesture.update(true,true,metric.point(aim),true,false)==XrTriggerEvent::Begin);
		auto jitter=aim;jitter.orientation=xrMul(aim.orientation,xrAxisAngle({1,0,0},.006f));
		check(gesture.update(true,true,metric.point(jitter),true,false)==XrTriggerEvent::None);
		check(gesture.update(false,true,metric.point(jitter),true,false)==XrTriggerEvent::Click);
		gesture.update(false,true,metric.point(aim),true,false);gesture.update(true,true,metric.point(aim),true,false);
		auto drag=aim;drag.position=xrAdd(aim.position,xrRotate(aim.orientation,{0,.04f,0}));
		check(gesture.update(true,true,metric.point(drag),true,false)==XrTriggerEvent::Drag);
		check(gesture.update(false,true,metric.point(drag),true,false)==XrTriggerEvent::Drop);
	}
	float mapping[16];
	for(float yaw:{0.0f,.7f,1.57079633f,3.14159265f}) for(float span:{200.0f,600.0f,3000.0f}) {
		const XrVector3f axis={cosf(yaw),sinf(yaw),0};check(xrWorldToBoard(mapping,{0,0,0},axis,span));
		const auto right=xrWorldPan(mapping,span,.02f,0),forward=xrWorldPan(mapping,span,0,.02f);
		check(near(xrLength(right)/span,.01f));check(near(xrLength(forward)/span,.01f));
		check(fabsf(xrDot(right,forward))/(span*span)<.00001f);
		check(xrDot(right,axis)>0);check(near(xrTransformPoint(mapping,right).x,.01f));
		check(near(xrTransformPoint(mapping,forward).y,.01f));
		check(xrLength(xrWorldPan(mapping,span,10,10))<=span*.0251f);
	}
	check(xrLength(xrWorldPan(mapping,600,NAN,0))==0);
	// Atomic v6 parse; every older version defaults to right-handed roles.
	for(int version=1;version<=6;++version)for(int flag:{0,1,2}) {
		FILE *f=fopen(argv[1],"w");check(f!=nullptr);fprintf(f,"GENERALS_XR_LAYOUT %d\n",version);
		for(int i=0;i<(version==1 ? 2:3);++i)fprintf(f,"1 0 0 -1 0 0 0 1 0\n");
		if(version>=3)fprintf(f,"2\n");if(version>=4)fprintf(f,"1 1 0 1\n");
		if(version>=5)fprintf(f,"0\n");if(version>=6)fprintf(f,"%d\n",flag);fclose(f);
		XrLayout layout;layout.relative[0].width=1.9f;
		const bool valid=version<6 || flag<2;check(layout.load(argv[1])==valid);
		check(valid ? layout.leftHanded==(version==6 && flag==1):layout.relative[0].width==1.9f);
	}
	remove(argv[1]);printf("PASS %d comfort/handedness checks\n",checks);
}
