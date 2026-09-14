// GeneralsX @test Codex 13/09/2026 Production input router with engine spies.
#include "XrWorld.h"
#include "XrLayers.h"
#include "XrTactics.h"
#include <cstdio>
#include <cstdlib>
struct XrControllerState {
	XrPosef aim={{0,0,0,1},{0,0,0}};
	bool aimValid=true,select=false,secondary=false,back=false,tilt=false;
	bool grip[2]={};
	XrVector2f pan={},zoom={};
};
struct XrHello {
	bool stereoWorld=true,splitVisible=true,stereoVisible=true,panelLatched=true,arranging=false;
	bool inputArmed=false,pointerVisible=false,pointerPressed=false,worldCursorVisible=false,interactiveGame=true,cameraCustom=false;
	int pointerPiece=-1;float pointerU=0,pointerV=0;XrTime nextZoomTime=0;
	bool keyHeld[5]={};XrPointerRoute pointerRoute;XrWorldClick worldClick,worldCancel;
	XrVector3f worldCursor={};XrSurface surfaces[3];
	XrVector3f rayStart={},rayEnd={};bool rayVisible=false,rayHit=false;
};
enum class XrGameKey {Back,Left,Right,Up,Down};
static bool ui=false,spatialActive=false,rawSelect=false,rawSecondary=false,pointerActive=false,worldAvailable=true;
static bool expanded=false;
static bool XrGameBoot_ExpandedUI(){return expanded;}
static int checks=0,commits=0,cancels=0,worldPicks=0,panKeys=0;
static float px=0,py=0,wheelSeen=0;
static void check(bool b){++checks;if(!b){fprintf(stderr,"input check %d failed\n",checks);exit(1);}}
static XrSurface displayedSurface(const XrHello &,int slot){XrSurface s;s.pose.position={slot>=2 && !ui ? 5.0f:0,0,-1};return s;}
static float surfaceAspect(int){return .5f;}
static XrGameRect surfaceRect(int){return {};}
static int XrGameBoot_GameWidth(){return 1280;}
static int XrGameBoot_GameHeight(){return 720;}
static bool XrGameBoot_HasUIAt(float,float){return ui;}
static bool XrGameBoot_PickWorld(const XrSurface &,const XrPosef &,XrWorldHit &h){++worldPicks;h={640,200,1,{0,0,-1}};return worldAvailable;}
static void XrGameBoot_RoutePointer(int){}
static void XrGameBoot_SpatialPointer(bool active){spatialActive=active;}
static void XrGameBoot_Pointer(bool active,float x,float y,bool select,bool secondary,float wheel){
	pointerActive=active;px=x;py=y;rawSelect=select;rawSecondary=secondary;wheelSeen=wheel;
}
static void XrGameBoot_SpatialClick(bool cancel){check(spatialActive);if(cancel)++cancels;else ++commits;}
static XrTriggerGesture gesture;
static void XrGameBoot_SpatialTrigger(bool down,bool valid,bool add){
	if(gesture.update(down,valid,{0,0,0},true,add)==XrTriggerEvent::Click)XrGameBoot_SpatialClick(false);
}
static void XrGameBoot_Key(XrGameKey key,bool down){if(key!=XrGameKey::Back && down) ++panKeys;}
#include "XrInput.h"
int main(){
	XrHello x;XrControllerState c;XrTime time=1;
	auto frame=[&](){time+=200000000;updateControls(x,c,time);};
	frame();frame();frame();check(x.pointerRoute.source==1 && spatialActive && pointerActive);
	check(px==640 && py==200 && x.worldCursorVisible);
	check(x.rayVisible && x.rayHit && x.rayEnd.z==-1);
	c.select=true;frame();check(commits==0 && !rawSelect);c.select=false;frame();check(commits==1);
	frame();check(commits==1);
	ui=true;int picks=worldPicks;frame();frame();frame();check(worldPicks==picks && x.pointerRoute.source==2 && !spatialActive);
	c.select=true;frame();check(rawSelect && commits==1);c.select=false;frame();check(commits==1);
	// UI press cannot become a world order on release.
	frame();c.select=true;frame();ui=false;frame();c.select=false;frame();frame();frame();check(commits==1);
	// World press crossing UI is cancelled even if the ray returns before release.
	c.select=true;frame();ui=true;frame();ui=false;frame();c.select=false;frame();frame();frame();check(commits==1);
	c.secondary=true;frame();check(!rawSecondary);c.secondary=false;frame();check(cancels==1);
	frame();c.pan={1,1};c.zoom.y=1;frame();check(panKeys==0 && wheelSeen==0);
	c.select=true;frame();c.select=false;frame();check(commits==1);c={};frame();
	c.select=true;frame();c.aimValid=false;frame();c.select=false;frame();c={};frame();frame();frame();check(commits==1);
	worldAvailable=false;frame();check(x.rayVisible && !x.rayHit && x.rayEnd.z==-2.5f);
	c.select=true;frame();worldAvailable=true;frame();c.select=false;frame();frame();frame();check(commits==1);
	frame();frame();frame();check(x.rayHit && spatialActive);
	c.aimValid=false;frame();check(!x.rayVisible);c.aimValid=true;frame();
	x.stereoVisible=false;frame();check(!spatialActive && !x.worldCursorVisible);
	c.select=true;frame();c.select=false;frame();check(commits==1);
	x.stereoWorld=false;frame();frame();frame();c.select=true;frame();check(rawSelect && !spatialActive);
	c={};expanded=true;c.pan={1,1};c.zoom={1,1};const int before=panKeys;
	frame();frame();check(panKeys==before && wheelSeen==0);
	x.stereoWorld=true;x.stereoVisible=true;ui=false;const int beforePicks=worldPicks;
	frame();frame();check(!spatialActive && worldPicks==beforePicks);
	printf("PASS %d production input routing checks\n",checks);
}
