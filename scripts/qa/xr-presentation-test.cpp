// GeneralsX @test Codex 13/09/2026 Compact UI mapping, readable legend and color transfer.
#include "XrPlacement.h"
#include "XrLayers.h"
#include "XrColor.h"
#include "XrViewMode.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
static int checks=0;
static void check(bool b){++checks;if(!b){fprintf(stderr,"presentation check %d failed\n",checks);exit(1);}}
static void near(float a,float b){check(std::isfinite(a) && fabsf(a-b)<.00001f);}
int main() {
	// P14: every combination of intent, fresh split/eyes and planar validity.
	struct Capture {bool splitVisible,stereoVisible,stereoWorld;};
	for(int mask=0;mask<32;++mask) {
		const bool initial=mask&1,requested=mask&2,split=mask&4,eyes=mask&8,planar=mask&16;
		Capture c{initial,false,requested};xrResolveCapturedView(c,split,eyes,planar);
		check(c.stereoVisible==(requested && split && eyes));
		check(c.splitVisible==(initial && split && (c.stereoVisible || planar)));
	}
	Capture transition{true,true,true};xrResolveCapturedView(transition,true,false,false);
	// P17: every combination also checks whether the composed image is usable.
	for(int mask=0;mask<64;++mask) {
		const bool initial=mask&1,requested=mask&2,split=mask&4,eyes=mask&8,planar=mask&16,composed=mask&32;
		Capture c{initial,false,requested};const bool recovery=xrResolveCapturedView(c,split,eyes,planar,composed);
		check(recovery==(!c.stereoVisible && !c.splitVisible && !composed));
		if(recovery)check(!c.stereoVisible && !c.splitVisible);
	}
	check(!transition.stereoVisible && !transition.splitVisible); // current composed fallback, never stale planar
	transition={true,false,false};xrResolveCapturedView(transition,true,false,true);
	check(transition.splitVisible && !transition.stereoVisible); // next frame recovered planar copy
	const float full=9.0f/16,legendAspect=.12f;
	for(float boundary : {.1f,.25f,.4f,.6f,0.0f,std::numeric_limits<float>::quiet_NaN()}) {
		const auto bar=xrCommandRect(boundary),hud=xrAboveCommandRect(bar);
		check(bar.h>=.18f && bar.h<=.65f);near(bar.h+hud.h,1);near(hud.y,bar.h);
		near(xrUIBandOffset(bar,bar,full),0);
		// No hidden content or discontinuity: both UV and physical seams coincide.
		near(xrUIBandOffset(hud,bar,full)-hud.h*full*.5f,bar.h*full*.5f);
		for(const auto band : {bar,hud}) {
			XrSurface s;s.width=.95f;s.pose.position={0,xrUIBandOffset(band,bar,full)*s.width,-1};
			float m[16],u=0,v=0;surfaceMatrix(s,m);
			XrPosef aim={{0,0,0,1},{0,s.pose.position.y,0}};
			check(panelRayUV(m,full*band.h,aim,&u,&v));near(u,.5f);near(v,.5f);
			near(1-band.y-v*band.h,1-band.y-band.h*.5f);
		}
	}
	XrSurface parent;parent.width=1.4f;parent.pose.position={.3f,.8f,-1};
	parent.pose.orientation=xrAxisAngle({1,0,0},-1.570796327f);
	const auto legend=xrLegendSurface(parent,.45f,legendAspect);
	const auto n=xrRotate(legend.pose.orientation,{0,0,1});
	near(n.y,cosf(.959931089f));near(n.z,sinf(.959931089f));
	check(legend.pose.position.y>parent.pose.position.y);near(legend.width,parent.width);
	float m[16],u=0,v=0;surfaceMatrix(legend,m);
	const XrPosef aim={legend.pose.orientation,xrAdd(legend.pose.position,n)};
	check(panelRayUV(m,legendAspect,aim,&u,&v));near(u,.5f);near(v,.5f);
	// Rigidly turning the table changes its heading, not the hinge elevation.
	parent.pose.orientation=xrMul(xrAxisAngle({0,1,0},1),parent.pose.orientation);
	near(xrRotate(xrLegendSurface(parent,.45f,legendAspect).pose.orientation,{0,0,1}).y,n.y);
	parent.pose.orientation={0,0,0,1};const auto upright=xrLegendSurface(parent,.3f,legendAspect);
	near(upright.pose.orientation.x,0);near(upright.pose.orientation.w,1);
	near(xrDisplayToLinear(0),0);near(xrDisplayToLinear(1),1);
	near(xrDisplayToLinear(.5f),.21404114f);near(xrDisplayToLinear(.04045f),.003130805f);
	float last=-1;
	for(int i=0;i<=255;++i){const float value=xrDisplayToLinear(float(i)/255);check(value>last);last=value;}
	printf("PASS %d P5.1 presentation checks\n",checks);
}
