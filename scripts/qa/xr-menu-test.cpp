// GeneralsX @test Codex 13/09/2026 Menu geometry and release-only capture.
#include "XrMenu.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
static int checks=0;
static void check(bool ok){++checks;if(!ok){fprintf(stderr,"menu check %d failed\n",checks);exit(1);}}
int main(){
	for(int i=0;i<kXrMenuButtons;++i) {
		const float x=(i%2 ? 392:32)+172,y=160+(i/2)*90+36;
		check(xrMenuHit(x/768,1-y/1024)==i);
		check(xrMenuHit(x/768,1-(y+42)/1024)==-1);
	}
	for(float x:{0.0f,31.0f,380.0f,740.0f,1000.0f}) check(xrMenuHit(x/768,.8f)==-1);
	check(xrMenuHit(.5f,std::numeric_limits<float>::quiet_NaN())==-1);
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
