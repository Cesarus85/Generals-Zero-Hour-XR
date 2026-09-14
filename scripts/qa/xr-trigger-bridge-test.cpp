// GeneralsX @test Codex 13/09/2026 Compiles the production bridge verbatim.
#include "XrTactics.h"
#include <cstdio>
#include <cstdlib>
struct Coord3D{float x,y,z;};
static XrTactics s_tactics;
static XrTriggerGesture s_triggerGesture;
static XrVector3f s_triggerStart;
static bool s_triggerPreview=false,s_spatialActive=true,locked=false,ability=false;
static int s_activePixel=0;
static int s_activeStart=0,s_activeEnd=0,s_triggerRayStart=0,s_triggerRayEnd=0,s_triggerPixel=0;
static XrPosef s_activeAim={{0,0,0,1},{0,0,1}};
static XrVector3f s_activeRoom={};
static XrRayDrag s_rayDrag;
static Coord3D aim={};
struct UI{bool construction=false;bool getPendingPlaceType(){return construction;}} ui;
static UI *TheInGameUI=&ui;
struct View{bool collision=true;bool screenToTerrain(int *,Coord3D *out){*out=aim;return collision;}} view;
static View *TheTacticalView=&view;
static bool XrGameBoot_CanAdjustWorld(){return !locked;}
namespace TouchInput {static bool hasArmedCommand(){return ability;}}
static int calls=0,checks=0;
static XrOrderMode committedMode;
static XrVector3f committedCorner;
static void check(bool v){++checks;if(!v){fprintf(stderr,"trigger bridge check %d failed\n",checks);exit(1);}}
static int committedPixel=0;
static void XrGameBoot_SpatialClick(bool cancel){check(!cancel);++calls;committedMode=s_tactics.mode;committedCorner=s_tactics.corner;committedPixel=s_activePixel;}
#include "xr-trigger-bridge.inc"
int main(){
	auto reset=[&](){s_activeAim={{0,0,0,1},{0,0,1}};s_activePixel=0;s_triggerGesture={};s_tactics={};aim={};s_triggerPreview=false;view.collision=true;ability=false;ui.construction=false;locked=false;calls=0;XrGameBoot_SpatialTrigger(false,true,false);};
	reset();XrGameBoot_SpatialTrigger(true,true,false);check(calls==0);
	aim.x=.01f;XrGameBoot_SpatialTrigger(false,true,false);check(calls==1 && committedMode==XrOrderMode::Context);
	reset();XrGameBoot_SpatialTrigger(true,true,false);aim.x=.1f;s_activeAim.position.x=.1f;XrGameBoot_SpatialTrigger(true,true,false);
	check(calls==0 && s_tactics.cornerKnown && s_triggerPreview);XrGameBoot_SpatialTrigger(false,true,false);
	check(calls==1 && committedMode==XrOrderMode::Box && committedCorner.x==0 && !s_tactics.cornerKnown);
	// Release can cross the threshold without an intermediate held sample.
	reset();XrGameBoot_SpatialTrigger(true,true,true);aim.y=.1f;s_activeAim.position.y=.1f;XrGameBoot_SpatialTrigger(false,true,false);
	check(calls==1 && committedMode==XrOrderMode::BoxAdd && s_tactics.mode==XrOrderMode::Context);
	reset();XrGameBoot_SpatialTrigger(true,true,true);XrGameBoot_SpatialTrigger(false,true,false);
	check(calls==1 && committedMode==XrOrderMode::Add && s_tactics.mode==XrOrderMode::Context);
	for(int reason=0;reason<3;++reason){
		reset();ability=reason==0;ui.construction=reason==1;if(reason==2)s_tactics.setMode(XrOrderMode::AttackMove);
		XrGameBoot_SpatialTrigger(true,true,true);aim.x=.2f;XrGameBoot_SpatialTrigger(true,true,true);XrGameBoot_SpatialTrigger(false,true,true);
		check(calls==1 && committedMode==(reason==2 ? XrOrderMode::AttackMove:XrOrderMode::Context));
	}
	for(int reason=0;reason<3;++reason){
		reset();XrGameBoot_SpatialTrigger(true,true,false);aim.x=.1f;s_activeAim.position.x=.1f;XrGameBoot_SpatialTrigger(true,true,false);
		locked=reason==0;view.collision=reason!=1;XrGameBoot_SpatialTrigger(true,reason!=2,false);
		check(!s_triggerPreview && !s_tactics.cornerKnown);locked=false;view.collision=true;
		XrGameBoot_SpatialTrigger(true,true,false);XrGameBoot_SpatialTrigger(false,true,false);check(calls==0);
	}
	// Far terrain motion without equivalent controller intent stays a click.
	reset();XrGameBoot_SpatialTrigger(true,true,false);aim.x=100;s_activePixel=99;s_activeAim.position.x=.005f;XrGameBoot_SpatialTrigger(false,true,false);
	check(calls==1 && committedMode==XrOrderMode::Context && committedPixel==0 && s_activePixel==99);
	// An object at the edge of the table need not have terrain behind it.
	reset();view.collision=false;XrGameBoot_SpatialTrigger(true,true,true);XrGameBoot_SpatialTrigger(false,true,false);
	check(calls==1 && committedMode==XrOrderMode::Add);
	reset();s_tactics.queue=true;XrGameBoot_SpatialTrigger(true,true,false);aim.x=.2f;XrGameBoot_SpatialTrigger(false,true,false);
	check(calls==1 && committedMode==XrOrderMode::Context && s_tactics.queue);
	printf("PASS %d production trigger bridge checks\n",checks);
}
