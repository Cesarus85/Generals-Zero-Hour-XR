// GeneralsX @test Codex 13/09/2026 Spatial selection and native cap policy.
#include "XrTactics.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
static int checks=0;
static void check(bool v){++checks;if(!v){fprintf(stderr,"tactics check %d failed\n",checks);exit(1);}}
int main(){
	XrTactics t;check(t.mode==XrOrderMode::Context && !t.queue && !t.cornerKnown && t.group==0);
	for(int mode=0;mode<11;++mode){t.cornerKnown=true;t.setMode(static_cast<XrOrderMode>(mode));check(!t.cornerKnown && int(t.mode)==mode);}
	t.queue=true;t.cornerKnown=true;t.cancel();check(!t.queue && !t.cornerKnown && t.mode==XrOrderMode::Context);
	for(int i=0;i<4;++i){const XrVector3f a={i&1 ? 1.0f:-1.0f,i&2 ? 2.0f:-2.0f,0},b={-a.x,-a.y,0};
		check(xrSelectionContains(a,b,{0,0,50}));check(xrSelectionContains(a,b,a));check(!xrSelectionContains(a,b,{1.01f,0,0}));}
	const float nan=std::numeric_limits<float>::quiet_NaN();
	check(!xrSelectionContains({nan,0,0},{1,1,0},{0,0,0}));check(!xrSelectionContains({0,0,0},{1,nan,0},{0,0,0}));
	check(!xrSelectionContains({0,0,0},{1,1,0},{nan,0,0}));
	check(xrSelectionContains({1,1,0},{1,1,0},{1,1,0}));check(!xrSelectionContains({1,1,0},{1,1,0},{0,1,0}));
	for(int count=0;count<1000;++count){check(xrSelectionHasRoom(count,0));check(xrSelectionHasRoom(count,40)==(count<40));}
	printf("PASS %d tactics geometry/cap/reset checks\n",checks);
}
