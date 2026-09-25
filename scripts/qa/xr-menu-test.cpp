// GeneralsX @test Codex 13/09/2026 Menu geometry and release-only capture.
// GeneralsX @test Ultron 15/09/2026 P21 geometry checks are table-driven:
// every hittable control rect centers to its own id, gaps stay dead.
#include "XrMenu.h"
#include "XrCommands.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
static int checks=0;
static void check(bool ok){++checks;if(!ok){fprintf(stderr,"menu check %d failed\n",checks);exit(1);}}
static void checkTable(const XrPanelControl *t,int n,int height,int hitPage) {
	for(int i=0;i<n;++i) {
		const auto &c=t[i];
		check(c.x>=0 && c.y>=0 && c.w>0 && c.h>0 && c.x+c.w<=kXrPanelWidth && c.y+c.h<=height);
		if(!xrPanelRoleHittable(c.role))continue;
		check(c.id>=0);
		const float u=(c.x+c.w*.5f)/kXrPanelWidth,v=1-(c.y+c.h*.5f)/height;
		if(hitPage>=0) check(xrMenuHit(u,v,hitPage)==c.id);
		// A point one pixel outside each edge must not hit this control.
		if(c.x>0) {
			const float ul=(c.x-1.5f)/kXrPanelWidth;
			if(hitPage>=0) check(xrMenuHit(ul,v,hitPage)!=c.id);
		}
	}
	// No two hittable rects may overlap: gaps and borders stay unambiguous.
	for(int i=0;i<n;++i) for(int j=i+1;j<n;++j) {
		const auto &a=t[i],&b=t[j];
		if(!xrPanelRoleHittable(a.role)||!xrPanelRoleHittable(b.role))continue;
		const bool overlap=a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h;
		check(!overlap);
	}
}
int main(){
	for(int page:{0,1,2,3,6}) {
		XrPanelControl t[80];
		const int n=xrMenuLayout(page,t,80);
		check(n>(page==6 ? 2:10));
		checkTable(t,n,kXrMenuHeight,page);
	}
	{	// Commands compact and expanded share the same contract.
		XrPanelControl t[64];
		int n=xrCommandLayout(false,false,t,64);check(n>20);checkTable(t,n,kXrPanelHeight,-1);
		n=xrCommandLayout(false,true,t,64);check(n>25);checkTable(t,n,kXrPanelHeightTall,-1);
		n=xrCommandLayout(true,false,t,64);check(n==4);checkTable(t,n,kXrPanelHeight,-1);
		n=xrMenuLayout(4,t,80);check(n==4);checkTable(t,n,kXrPanelHeight,-1);
	}
	check(xrMenuHit(.5f,std::numeric_limits<float>::quiet_NaN())==-1);
	check(xrMenuHit(.001f,.999f,0)==-1);
	check(xrCommandHit(.5f,std::numeric_limits<float>::quiet_NaN())==-1);
	XrMenuState s;
	check(!s.update(true,0));check(!s.update(false,0)); // held on first frame
	check(!s.update(false,0));check(!s.update(true,0));check(s.update(false,0));check(!s.update(false,0));
	check(!s.update(true,0));check(!s.update(true,1));check(!s.update(true,0));check(!s.update(false,0));
	check(!s.update(false,-1));check(!s.update(true,-1));check(!s.update(true,0));check(!s.update(false,0));
	check(!s.update(false,18));check(!s.update(true,18));check(s.update(false,18));
	check(!s.update(false,0));check(!s.update(true,0));s.click.cancel();check(!s.update(false,-1));
	s.click.cancel();check(!s.update(true,0));check(!s.update(false,0));check(!s.update(false,0));
	check(!s.update(true,0));check(s.update(false,0));
	check(!s.update(true,0));check(!s.update(true,0,false));check(!s.update(true,0,false));
	check(!s.update(true,0));check(!s.update(false,0));check(!s.update(false,0));
	check(!s.update(true,0));check(s.update(false,0));
	printf("PASS %d menu geometry/capture checks\n",checks);
}
