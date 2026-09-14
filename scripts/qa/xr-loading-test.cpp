// GeneralsX @test Codex 14/09/2026 Production synchronous-load frame ownership.
#include "XrLoadingFrame.h"
#include <cstdio>
#include <cstdlib>
#include <string>
static int checks=0;
static void check(bool value){++checks;if(!value){fprintf(stderr,"loading check %d failed\n",checks);exit(1);}}
int main() {
	XrLoadingFrame f;std::string calls;
	auto close=[&]{calls+='C';return true;};auto ready=[&]{calls+='R';return true;};
	auto begin=[&]{calls+='B';return true;};auto draw=[&]{calls+='D';return true;};auto end=[&]{calls+='E';return true;};
	check(!f.consumed && !f.failed);
	check(f.pump(close,ready,begin,draw,end));check(calls=="CRBDE" && f.consumed);
	for(int i=0;i<240;++i){calls.clear();check(f.pump(close,ready,begin,draw,end));check(calls=="RBDE");}
	calls.clear();check(f.pump(close,[]{return false;},begin,draw,end));check(calls.empty());
	check(f.pump(close,ready,begin,[&]{check(f.pump(close,ready,begin,draw,end));return true;},end));
	check(calls=="RBE"); // nested callback cannot begin another frame
	for(int failure=0;failure<4;++failure) {
		XrLoadingFrame broken;calls.clear();
		check(!broken.pump([&]{calls+='C';return failure!=0;},ready,
			[&]{calls+='B';return failure!=1;},[&]{calls+='D';return failure!=2;},[&]{calls+='E';return failure!=3;}));
		check(broken.consumed && broken.failed && !broken.busy);
		check(calls==(failure==0 ? "C":failure==1 ? "CRB":"CRBDE"));
		calls.clear();check(!broken.pump(close,ready,begin,draw,end));check(calls.empty());
	}
	// A held launch click never skips a movie; release then press is required.
	XrLoadingFrame input;check(!input.skip(true,true));check(!input.skip(true,true));
	check(!input.skip(true,false));check(input.skip(true,true));check(!input.skip(true,true));
	check(!input.skip(false,false));check(!input.skip(true,true));
	check(!input.skip(true,false));check(input.skip(true,true));
	printf("PASS %d loading frame ownership/input checks\n",checks);
}
