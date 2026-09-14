// GeneralsX @test Codex 13/09/2026 Production placement/persistence regressions.
#include "XrLayout.h"
#include "XrOverlay.h"
#include <cstdlib>
#include <vector>
static int checks=0;
static void check(bool ok) { ++checks; if(!ok) { fprintf(stderr,"placement check %d failed\n",checks); exit(1); } }
static void near(float a,float b) { check(std::isfinite(a) && fabsf(a-b)<.0001f); }
static void vectorNear(XrVector3f a,XrVector3f b) { near(a.x,b.x);near(a.y,b.y);near(a.z,b.z); }
int main(int argc,char **argv)
{
	check(argc==2); // caller supplies a new temporary path for persistence fixtures
	const XrPosef anchor={xrAxisAngle({0,1,0},.7f),{1,1.5f,-2}};
	const XrPosef identity=xrPoseMul(xrPoseInverse(anchor),anchor);
	vectorNear(identity.position,{0,0,0});near(identity.orientation.w,1);
	XrLayout layout;
	for(int i=0;i<3;++i) {
		const auto pose=xrPoseMul(xrPoseInverse(anchor),xrPoseMul(anchor,layout.relative[i].pose));
		vectorNear(pose.position,layout.relative[i].pose.position);
	}
	XrSurface s=layout.relative[0]; s.width=1; s.pose.position={0,0,-1};
	XrPosef hands[2]={{{0,0,0,1},{-.25f,0,0}},{{0,0,0,1},{.25f,0,0}}};
	bool held[2]={false,false}, valid[2]={true,true};
	XrSurfaceGrab grab;
	check(!grab.update(s,hands,held,valid));check(grab.armed);
	held[1]=true;check(!grab.update(s,hands,held,valid));
	hands[1].position.x+=.2f;check(grab.update(s,hands,held,valid));vectorNear(s.pose.position,{.2f,0,-1});
	// Joining the second hand captures, never changes the existing surface.
	const auto before=s;held[0]=true;check(!grab.update(s,hands,held,valid));vectorNear(s.pose.position,before.pose.position);
	hands[0].position.x=-.6f;hands[1].position.x=.8f;
	check(grab.update(s,hands,held,valid));near(s.width,2);vectorNear(s.pose.position,{.3f,0,-2});
	// Hard scale bound also clamps the pivot displacement proportionally.
	hands[0].position.x=-2;hands[1].position.x=2.2f;check(grab.update(s,hands,held,valid));near(s.width,2.5f);near(s.pose.position.z,-2.5f);
	const auto two=s;held[0]=false;check(!grab.update(s,hands,held,valid));vectorNear(s.pose.position,two.pose.position);
	hands[1].position.y=.4f;check(grab.update(s,hands,held,valid));near(s.pose.position.y,.4f);
	// Tracking loss must not resume with the same held button.
	valid[1]=false;check(!grab.update(s,hands,held,valid));check(!grab.armed);
	valid[1]=true;hands[1].position.y=2;check(!grab.update(s,hands,held,valid));near(s.pose.position.y,.4f);
	held[1]=false;check(!grab.update(s,hands,held,valid));check(grab.armed);
	held[0]=held[1]=true;hands[0].position=hands[1].position;
	check(!grab.update(s,hands,held,valid));check(!grab.armed);
	// One-hand rotation preserves the captured hand-to-board rigid offset.
	held[0]=held[1]=false;grab.update(s,hands,held,valid);
	held[1]=true;grab.update(s,hands,held,valid);const auto offset=xrPoseMul(xrPoseInverse(hands[1]),s.pose);
	hands[1].orientation=xrAxisAngle({0,1,0},1);grab.update(s,hands,held,valid);
	vectorNear(s.pose.position,xrPoseMul(hands[1],offset).position);
	vectorNear(xrRotate(xrFromTo({1,0,0},{-1,0,0}),{1,0,0}),{-1,0,0});
	// Level snap retains yaw; restored physical matrix remains ray-pickable.
	for(int table=0;table<2;++table) {
		s=layout.relative[table];s.pose=xrPoseMul(anchor,s.pose);s.width=2.2f;
		s.pose.orientation=xrMul(s.pose.orientation,xrAxisAngle({0,0,1},.2f));
		snapSurface(s,table!=0);
		const auto normal=xrRotate(s.pose.orientation,{0,0,1});near(normal.y,table ? 1:0);
		float m[16],u=0,v=0;surfaceMatrix(s,m);
		const XrPosef aim={s.pose.orientation,xrAdd(s.pose.position,xrScale(normal,1.2f))};
		check(panelRayUV(m,9.0f/16,aim,&u,&v));near(u,.5f);near(v,.5f);
		layout.relative[table]=s;layout.relative[table].pose=xrPoseMul(xrPoseInverse(anchor),s.pose);
	}
	layout.relative[2].width=1.25f;layout.snap[2]=false;layout.worldZoom=2.4f;layout.relative[1].width=4;
	layout.startStereo=true;layout.healthBars=false;layout.unitRings=false;layout.boardFrame=false;layout.commandsVisible=false;layout.leftHanded=true;
	layout.snap[1]=false;check(layout.save(argv[1]));XrLayout restored;check(restored.load(argv[1]));
	check(restored.startStereo && !restored.healthBars && !restored.unitRings && !restored.boardFrame);
	check(!restored.commandsVisible);
	check(restored.leftHanded);
	near(restored.worldZoom,2.4f);near(restored.relative[1].width,4);
	near(restored.relative[2].width,1.25f);check(!restored.snap[2]);
	near(restored.relative[0].width,2.2f);check(!restored.snap[1]);
	for(int i=0;i<2;++i) vectorNear(restored.relative[i].pose.position,layout.relative[i].pose.position);
	FILE *bad=fopen(argv[1],"w");check(bad!=nullptr);fprintf(bad,"GENERALS_XR_LAYOUT 99\n");fclose(bad);
	check(!restored.load(argv[1]));near(restored.relative[0].width,2.2f);
	// Version 1 migration preserves the old two surfaces and defaults only the new UI.
	bad=fopen(argv[1],"w");check(bad!=nullptr);
	fprintf(bad,"GENERALS_XR_LAYOUT 1\n1.7 0 0 -1 0 0 0 1 0\n1.2 0 -0.4 -0.7 0 0 0 1 1\n");fclose(bad);
	check(restored.load(argv[1]));near(restored.relative[0].width,1.7f);near(restored.relative[1].width,1.2f);
	near(restored.relative[2].width,1.8f);check(!restored.snap[0] && !restored.snap[2]);
	check(!restored.startStereo && restored.healthBars && restored.unitRings && restored.boardFrame);
	check(restored.commandsVisible);
	check(!restored.leftHanded);
	// Invalid v4 flags and incomplete preferences never replace a good layout.
	for(const char *flags:{"1 0 1 2","1 0"}) {
		bad=fopen(argv[1],"w");check(bad!=nullptr);
		fprintf(bad,"GENERALS_XR_LAYOUT 4\n1 0 0 -1 0 0 0 1 0\n1 0 0 -1 0 0 0 1 0\n1 0 0 -1 0 0 0 1 0\n2\n%s\n",flags);fclose(bad);
		check(!restored.load(argv[1]));near(restored.relative[0].width,1.7f);check(!restored.startStereo);
	}
	// A truncated v2 must not partially replace a successfully loaded layout.
	bad=fopen(argv[1],"w");check(bad!=nullptr);
	fprintf(bad,"GENERALS_XR_LAYOUT 2\n1.7 0 0 -1 0 0 0 1 0\n1.2 0 -0.4 -0.7 0 0 0 1 1\n");fclose(bad);
	check(!restored.load(argv[1]));near(restored.relative[0].width,1.7f);
	bad=fopen(argv[1],"w");check(bad!=nullptr);fprintf(bad,"GENERALS_XR_LAYOUT 1\nnan 0 0 -1 0 0 0 1 1\n");fclose(bad);
	check(!restored.load(argv[1]));near(restored.relative[0].width,1.7f);
	const auto pixels=xrLegendPixels({"P3 PLAY","A ARRANGE","X RECENTER"},false);
	check(pixels.size()==kLegendWidth*kLegendHeight*4);
	check(remove(argv[1])==0);
	printf("PASS %d placement/layout/legend checks\n",checks);
}
