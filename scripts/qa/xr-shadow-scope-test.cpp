// GeneralsX @test Codex 14/09/2026 Compile actual frame shadow override scope.
#include "XrWorld.h"
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
using Bool=bool;
constexpr bool FALSE=false,TRUE=true;
struct GlobalData {Bool m_useShadowVolumes,m_useShadowDecals,m_enableBehindBuildingMarkers;};
static GlobalData globals={false,false,true};
static GlobalData *TheGlobalData=&globals,*TheWritableGlobalData=&globals;
static bool s_booted=true,world=true,fail=false;
static unsigned s_xrPresentationFrame=0,calls=0;
static XrWorldFrame s_worldFrame;
static int checks=0;
static void check(bool v){++checks;if(!v){fprintf(stderr,"FAIL shadow scope %d\n",checks);exit(1);}}
static GlobalData expected;
struct Engine {Bool executeSingleFrame(){++calls;check(globals.m_useShadowVolumes==expected.m_useShadowVolumes);
	check(globals.m_useShadowDecals==expected.m_useShadowDecals);check(globals.m_enableBehindBuildingMarkers==expected.m_enableBehindBuildingMarkers);
	if(fail)throw std::runtime_error("test");return true;}} engine;
static Engine *TheGameEngine=&engine;
static bool GX_XR_SplitUIAllowed(){return true;}
static void d3d8gles_BeginXRFrame(bool,bool=false){}
bool GX_XR_WorldRequested(){return world;}
#define GXLOGE(...) ((void)0)
#include "xr-shadow-scope.inc"
int main(){
	for(bool oldVolumes:{false,true})for(bool oldDecals:{false,true})for(bool oldMarkers:{false,true})
	for(bool xr:{false,true})for(bool volumes:{false,true})for(bool error:{false,true}){
		globals={oldVolumes,oldDecals,oldMarkers};world=xr;s_worldFrame.volumeShadows=volumes;fail=error;
		expected=xr ? GlobalData{volumes,true,false}:globals;
		const auto frames=s_xrPresentationFrame,before=calls;
		check(XrGameBoot_Frame()==!error);check(calls==before+1);check(s_xrPresentationFrame==frames+unsigned(!error));
		check(globals.m_useShadowVolumes==oldVolumes && globals.m_useShadowDecals==oldDecals && globals.m_enableBehindBuildingMarkers==oldMarkers);
	}
	s_booted=false;const auto before=calls;check(!XrGameBoot_Frame() && calls==before);
	printf("PASS %d production shadow scope checks\n",checks);
}
