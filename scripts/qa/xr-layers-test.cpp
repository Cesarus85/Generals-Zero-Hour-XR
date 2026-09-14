// GeneralsX @test Codex 13/09/2026 Cropped viewport and queued-input handoff.
#include "XrLayers.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
static int checks=0;
static void check(bool b){++checks;if(!b){fprintf(stderr,"layer check %d failed\n",checks);exit(1);}}
static void near(float a,float b){check(std::isfinite(a) && fabsf(a-b)<.00001f);}
int main() {
	XrWorldClick click;
	check(!click.update(true,true));check(!click.update(false,true));
	check(!click.update(false,true));check(!click.update(true,true));check(click.update(false,true));
	check(!click.update(false,true));check(!click.update(true,true));
	check(!click.update(true,false));check(!click.update(true,true));check(!click.update(false,true));
	check(!click.update(false,true));check(!click.update(true,true));click.cancel();check(!click.update(false,true));
	check(!click.update(false,false));check(!click.update(true,false));check(!click.update(false,true));
	auto r=xrViewportRect(0,0,1280,540,1280,720);
	near(r.x,0);near(r.y,.25f);near(r.w,1);near(r.h,.75f);
	near(1-r.y-r.h,0);near(1-r.y,.75f); // top/bottom map to original engine coordinates
	r=xrViewportRect(100,50,800,400,1280,720);
	near(r.x,100.0f/1280);near(1-r.y-r.h,50.0f/720);
	for(auto bad:{xrViewportRect(-1,0,10,10,1280,720),xrViewportRect(0,0,1281,10,1280,720),
		xrViewportRect(0,0,0,10,1280,720),xrViewportRect(0,0,10,10,0,0)}) {
		near(bad.x,0);near(bad.y,0);near(bad.w,1);near(bad.h,1);
	}
	XrPointerRoute route;
	check(!route.update(1,false));check(route.source==-1);
	check(!route.update(1,false));check(route.source==1);check(route.update(1,false));
	check(route.update(1,true));
	check(!route.update(2,true));check(route.source==1); // old recipient receives release
	check(!route.update(2,true));check(route.source==1); // held trigger cannot begin UI click
	check(!route.update(2,false));check(route.source==2);check(route.update(2,false));
	check(!route.update(0,false));check(route.source==2); // modal/full view still drains old UI release
	check(!route.update(0,false));check(route.source==0);check(route.update(0,false));
	check(!route.update(-1,true));check(route.source==0);
	check(!route.update(1,true));check(route.source==0);
	check(!route.update(1,false));check(route.source==1);
	printf("PASS %d layer crop/input-handoff checks\n",checks);
}
