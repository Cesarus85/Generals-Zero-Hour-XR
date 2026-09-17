// GeneralsX @test Codex 13/09/2026 Headless mode/focus routing with production interaction code.
#include "XrLayout.h"
#include "XrLayers.h"
#include "XrCommands.h"
#include "XrBuildRotation.h"
#include "XrScene.h"
#include "XrWorld.h"
#include <cstdlib>
struct XrControllerState {
	XrPosef aim={{0,0,0,1},{0,0,0}}, hands[2]={{{0,0,0,1},{-.3f,0,0}},{{0,0,0,1},{.3f,0,0}}};
	bool aimValid=true,select=false,secondary=false,back=false,recenter=false,upright=false;
	bool arrange=false,preset=false,homeBase=false,tilt=false,buttonsHeld=false,grip[2]={},handValid[2]={true,true};
	XrVector2f pan={},zoom={};
};
struct XrHello {
	bool roomPoseLost=false;
	XrObserverState observer;XrTime observerFadeStart=0;
	bool rayVisible=false,rayHit=false,hoverVisible=false;
	XrVector3f rayStart={},rayEnd={};
	XrScene scene;bool inputArmed=true;
	XrMenuState menu;XrCommandState commands;
	XrBuildRotation buildRotation;
	XrLayout layout; XrSurface surfaces[3]; XrSurfaceGrab grab;
	XrPointerRoute pointerRoute;
	bool splitVisible=true; int arrangeSlot=1;
	bool diorama=false,dioramaReady=true;
	bool stereoWorld=false;float worldZoom=1;
	bool startViewApplied=false;
	XrPosef layoutAnchor={{0,0,0,1},{0,0,0}};
	XrTime previousInputTime=0,recenterTime=0;
	XrSessionState state=XR_SESSION_STATE_FOCUSED;
	bool anchorKnown=true,layoutDirty=false,controlsArmed=false,arranging=false,interactiveGame=true,uprightGame=false;
	int cameraPreset=1; bool cameraPending=true,cameraCustom=false;
	bool cameraSaveFailed=false;
};
static int checks=0,presets=0,adjustments=0;
static bool cameraLocked=false;
static bool hasFavorite=false,saveFails=false;
static bool canStereo=false;
static bool expanded=false;
static bool buildPending=false;
static float buildingAngle=0;
static bool XrGameBoot_CanRotatePlacement(){return buildPending && !cameraLocked;}
static bool XrGameBoot_RotatePlacement(float delta){buildingAngle+=delta;return true;}
static int targetCancels=0;
static void XrGameBoot_CancelTarget(){++targetCancels;}
static bool XrGameBoot_ExpandedUI(){return expanded;}
static int navigations=0;
static float navRight=0,navForward=0,navZoom=0;
static bool XrGameBoot_NavigateWorld(float r,float f,float z) {
	if(cameraLocked || !canStereo || (r==0 && f==0 && z==0)) return false;
	++navigations;navRight=r;navForward=f;navZoom=z;return true;
}
static XrControllerState lastInput;
static const char *path=nullptr;
static void check(bool b) { ++checks;if(!b){fprintf(stderr,"interaction check %d failed\n",checks);exit(1);} }
#define XR_LOG(...) ((void)0)
#define XR_LOGE(...) ((void)0)
static const char *XrGameBoot_LayoutPath(){return path;}
static bool XrGameBoot_CameraPreset(int) {if(cameraLocked)return false;++presets;return true;}
static bool XrGameBoot_AdjustCamera(float,float) {if(cameraLocked)return false;++adjustments;return true;}
static int XrGameBoot_DefaultCameraPreset(){return hasFavorite ? 4:1;}
static bool XrGameBoot_SaveCameraDefault(){if(cameraLocked || saveFails)return false;hasFavorite=true;return true;}
static bool XrGameBoot_CanStereoWorld(){return canStereo;}
static bool XrGameBoot_CanAdjustWorld(){return canStereo && !cameraLocked;}
static bool observerGroundValid=true;
static bool XrGameBoot_PickObserverGround(const XrSurface &,const XrPosef &,XrVector3f &ground,XrVector3f *room){
	if(!observerGroundValid)return false;ground={500,500,20};if(room)*room={0,0,-1};return true;
}
static int baseViews=0;
static bool XrGameBoot_ViewBase(){if(!XrGameBoot_CanAdjustWorld() || expanded)return false;++baseViews;return true;}
static void placePanel(XrHello &,const XrView *){}
static int activeSurface(const XrHello &x){return x.diorama ? 1:x.splitVisible ? (x.arranging ? x.arrangeSlot:1):0;}
static void updateControls(XrHello &,const XrControllerState &c,XrTime){lastInput=c;}
static bool menuCapture=false,consoleCapture=false;
static bool updateXrMenu(XrHello &,const XrControllerState &,const XrView *,XrTime){return menuCapture;}
static bool updateCommands(XrHello &,const XrControllerState &,XrTime){return consoleCapture;}
static void saveLayout(XrHello &x);
#include "XrArrangement.h"
#include "XrInteraction.h"
int main(int argc,char **argv)
{
	check(argc==2);path=argv[1];XrHello x;XrView views[2]={};
	views[0].pose.orientation.w=views[1].pose.orientation.w=1;
	for(int i=0;i<3;++i) x.surfaces[i]=x.layout.relative[i];
	XrTime time=1000000000;
	auto frame=[&](XrControllerState c){time+=14000000;updateInteraction(x,c,views,time);};
	XrControllerState c;x.splitVisible=false;
	frame(c);check(x.controlsArmed);frame(c);check(presets==0 && x.cameraPending);
	x.splitVisible=true;frame(c);check(presets==1);
	c.select=true;frame(c);check(lastInput.select);
	c.arrange=true;frame(c);check(x.arranging && !x.controlsArmed && !lastInput.select);
	c.arrange=false;frame(c);check(!x.controlsArmed && !lastInput.select);
	c={};frame(c);check(x.controlsArmed);frame(c);check(x.grab.armed);
	c.grip[1]=true;frame(c);check(x.grab.mode==2);
	c.hands[1].position.x+=.2f;frame(c);check(fabsf(x.surfaces[1].pose.position.x-.2f)<.0001f);check(!lastInput.secondary);
	c.handValid[1]=false;frame(c);check(!x.grab.armed);
	c.handValid[1]=true;c.hands[1].position.x+=1;frame(c);check(x.grab.mode==0);check(fabsf(x.surfaces[1].pose.position.x-.2f)<.0001f);
	c={};frame(c);check(x.grab.armed);
	c.back=true;frame(c);check(!x.arranging && !x.controlsArmed && !lastInput.back);
	frame(c);check(!x.controlsArmed && !lastInput.back);
	c={};frame(c);check(x.controlsArmed);
	c.preset=true;frame(c);check(x.cameraPreset==2 && presets==2);
	c={};c.zoom.x=1;frame(c);check(adjustments==1 && x.cameraCustom);
	x.state=XR_SESSION_STATE_VISIBLE;c.select=true;frame(c);check(!x.controlsArmed && !lastInput.select);check(targetCancels>0);
	x.state=XR_SESSION_STATE_FOCUSED;frame(c);check(!x.controlsArmed && !lastInput.select);
	c={};frame(c);check(x.controlsArmed);
	c.aimValid=false;frame(c);check(!x.controlsArmed);
	c.aimValid=true;c.zoom.x=1;frame(c);check(!x.controlsArmed && adjustments==1);
	c={};frame(c);check(x.controlsArmed);
	cameraLocked=true;c.preset=true;frame(c);check(x.cameraPending && presets==2);
	c={};frame(c);check(x.cameraPending && presets==2);
	cameraLocked=false;frame(c);check(!x.cameraPending && presets==3);
	// P4 save/restore chords consume stick deflections and respect locks.
	c.tilt=true;c.preset=true;c.zoom={1,1};frame(c);
	check(hasFavorite && x.cameraPreset==4 && !x.cameraSaveFailed && !x.cameraPending);
	check(adjustments==1 && lastInput.zoom.x==0 && lastInput.zoom.y==0);
	c={};c.preset=true;frame(c);check(x.cameraPreset==1 && presets==4);
	c={};c.tilt=true;c.recenter=true;frame(c);check(x.cameraPreset==4 && presets==5);
	check(!x.cameraCustom);
	// P5 detached UI must not rotate the battlefield with its right stick.
	x.pointerRoute.source=2;c={};c.zoom.x=1;frame(c);check(adjustments==1);
	x.pointerRoute.source=1;
	saveFails=true;c={};c.preset=true;c.tilt=true;frame(c);check(x.cameraSaveFailed && x.cameraPreset==4);
	saveFails=false;cameraLocked=true;frame(c);check(x.cameraSaveFailed && presets==5);
	cameraLocked=false;c={};c.preset=true;frame(c);check(!x.cameraSaveFailed && x.cameraPreset==1);
	c={};
	c.upright=true;frame(c);check(!x.uprightGame && x.controlsArmed);
	c={};frame(c);c.arrange=true;frame(c);check(x.arranging);
	c={};frame(c);c.preset=true;frame(c);check(x.arrangeSlot==2 && !x.controlsArmed);
	c={};frame(c);frame(c);c.zoom.y=1;const float oldWidth=x.surfaces[2].width;frame(c);
	check(x.surfaces[2].width>oldWidth);
	c={};c.recenter=true;frame(c);check(fabsf(x.surfaces[2].width-1.8f)<.0001f);
	// P6 regression: even a legacy saved LEVEL window stays tilted after release.
	x.layout.snap[2]=true;c={};frame(c);frame(c);
	c.grip[1]=true;frame(c);c.hands[1].orientation=xrAxisAngle({1,0,0},-.3f);frame(c);
	const auto tilted=x.surfaces[2].pose.orientation;
	check(!x.layout.snap[2]);check(fabsf(xrRotate(tilted,{0,1,0}).z)>.2f);
	c={};frame(c);check(fabsf(x.surfaces[2].pose.orientation.x-tilted.x)<.00001f);
	XrLayout disk;check(disk.load(path));check(!disk.snap[2]);
	check(fabsf(disk.relative[2].pose.orientation.x-tilted.x)<.00001f);
	// Explicit alignment remains available; a later grab is free again.
	c.upright=true;frame(c);check(x.layout.snap[2]);check(fabsf(x.surfaces[2].pose.orientation.x)<.00001f);
	c={};c.back=true;frame(c);check(!x.arranging);
	c={};frame(c);
	// A live match refuses the demo chord and does not toggle combined view.
	const bool oldUpright=x.uprightGame;c.tilt=true;c.upright=true;frame(c);
	check(!x.diorama && x.uprightGame==oldUpright);
	// P15 startup owns tabletop intent; legacy chords must not change it.
	canStereo=true;x.stereoWorld=true;frame(c);check(x.stereoWorld && !x.uprightGame);
	c={};frame(c);x.pointerRoute.source=-1;c.pan={1,1};c.zoom.y=-1;frame(c);
	check(navigations==1 && navRight>0 && navForward>0 && navZoom==0 && x.worldZoom>1);
	c.tilt=true;frame(c);check(navigations==2 && navZoom==0);
	cameraLocked=true;frame(c);check(navigations==2);cameraLocked=false;
	x.state=XR_SESSION_STATE_VISIBLE;frame(c);check(navigations==2);
	x.state=XR_SESSION_STATE_FOCUSED;c={};frame(c);c.aimValid=false;c.pan.x=1;frame(c);check(navigations==2);
	c={};frame(c);const bool oldConsole=x.layout.commandsVisible;c.arrange=true;frame(c);
	check(x.layout.commandsVisible!=oldConsole && !x.arranging && !x.controlsArmed);
	// Workspace menu may still enter explicit arrangement; camera stays paused.
	x.arranging=true;c={};frame(c);c.pan.x=1;frame(c);check(navigations==2);
	c={};c.back=true;frame(c);
	c={};frame(c);c.tilt=true;c.upright=true;frame(c);check(x.stereoWorld && !lastInput.select);
	canStereo=false;c={};frame(c);
	x.interactiveGame=false;x.splitVisible=false;c={};frame(c);
	c.tilt=true;c.upright=true;c.select=true;frame(c);check(x.diorama && !x.controlsArmed && !lastInput.select);
	frame(c);check(x.diorama && !x.controlsArmed);
	c={};frame(c);check(x.controlsArmed);c.select=true;c.zoom={1,1};frame(c);
	check(!lastInput.select && lastInput.zoom.x==0);
	c={};c.arrange=true;frame(c);check(x.arranging && activeSurface(x)==1);
	c={};frame(c);frame(c);c.grip[1]=true;frame(c);
	c.hands[1].orientation=xrAxisAngle({1,0,0},.25f);frame(c);
	c={};frame(c);check(fabsf(xrRotate(x.surfaces[1].pose.orientation,{0,0,1}).y-1)<.00001f);
	c.back=true;frame(c);check(!x.arranging && x.diorama);
	c={};frame(c);c.back=true;frame(c);check(!x.diorama && !lastInput.back);
	c={};frame(c);x.dioramaReady=false;c.tilt=true;c.upright=true;frame(c);check(!x.diorama);
	// Home is camera-only, one press, and wins over accidental stick deflection.
	x=XrHello{};x.controlsArmed=true;x.stereoWorld=true;canStereo=true;
	c={};c.homeBase=true;c.pan={1,1};c.zoom={1,1};
	const int navBeforeHome=navigations,anglesBeforeHome=adjustments;
	const auto poseBeforeHome=x.surfaces[1].pose;const float zoomBeforeHome=x.worldZoom;
	frame(c);check(baseViews==1 && navigations==navBeforeHome && adjustments==anglesBeforeHome);
	check(x.worldZoom==zoomBeforeHome && xrLength(xrSub(poseBeforeHome.position,x.surfaces[1].pose.position))==0);
	c={};c.buttonsHeld=true;frame(c);check(baseViews==1); // no repeated edge
	for(int gate=0;gate<10;++gate) {
		x=XrHello{};x.controlsArmed=true;x.stereoWorld=true;c={};c.homeBase=true;
		if(gate==0)x.arranging=true;if(gate==1)x.state=XR_SESSION_STATE_VISIBLE;
		if(gate==2)c.aimValid=false;if(gate==3)x.interactiveGame=false;
		if(gate==4)menuCapture=true;if(gate==5)expanded=true;
		if(gate==6)cameraLocked=true;if(gate==7)buildPending=true;
		if(gate==8)c.select=true;if(gate==9)c.grip[0]=true;
		frame(c);check(baseViews==1);
		menuCapture=false;expanded=false;cameraLocked=false;buildPending=false;
	}
	// A held shortcut after focus loss must be released before re-arming.
	x=XrHello{};c={};c.homeBase=true;frame(c);check(!x.controlsArmed && baseViews==1);
	c={};frame(c);c.homeBase=true;consoleCapture=true;frame(c);consoleCapture=false;
	check(baseViews==2);
	check(remove(path)==0);
	// P10.1: console hover cannot steal pan; modal menu and rearming can.
	x={};x.controlsArmed=true;x.stereoWorld=true;canStereo=true;c={};frame(c);
	const int priorNav=navigations;consoleCapture=true;c.pan.x=1;frame(c);check(navigations==priorNav+1);
	menuCapture=true;frame(c);check(navigations==priorNav+1);menuCapture=false;
	x.controlsArmed=false;c={};c.buttonsHeld=true;frame(c);check(!x.controlsArmed);
	c={};frame(c);check(x.controlsArmed);c.pan.y=1;frame(c);check(navigations==priorNav+2);
	x.state=XR_SESSION_STATE_VISIBLE;frame(c);check(navigations==priorNav+2 && !x.controlsArmed);
	// P10.2: native quit/options dialog owns pointer/back, not camera shortcuts.
	x.state=XR_SESSION_STATE_FOCUSED;c={};frame(c);consoleCapture=false;expanded=true;
	c.pan={1,1};c.zoom={1,1};c.arrange=true;c.preset=true;c.select=true;
	const int beforePresets=presets,beforeAdjust=adjustments;frame(c);
	check(navigations==priorNav+2 && presets==beforePresets && adjustments==beforeAdjust && !x.arranging);
	check(lastInput.select);expanded=false;
	// P11.1: modal/console hover cannot swallow either arrangement exit.
	for(bool useBack:{true,false}) {
		x={};x.controlsArmed=true;x.arranging=true;x.menu.open=true;x.stereoWorld=true;
		menuCapture=true;consoleCapture=true;c={};c.back=useBack;c.arrange=!useBack;
		frame(c);check(!x.arranging && !x.menu.open && !x.controlsArmed && !lastInput.back);
		frame(c);check(!x.controlsArmed && !x.arranging);
		menuCapture=false;consoleCapture=false;c={};frame(c);check(x.controlsArmed);
		const int nav=navigations;c.pan.x=1;frame(c);check(navigations==nav+1);
		const int rotated=adjustments;c.pan.x=0;c.zoom.x=1;frame(c);check(adjustments==rotated+1);
	}
	// P18.1 actual interaction router: modifier never drives the camera,
	// remains captured after Grip release until stick neutral, and freezes
	// the angle through trigger down AND release (commit is queued).
	x={};x.controlsArmed=true;x.stereoWorld=true;x.cameraPending=false;canStereo=true;
	c={};frame(c);buildPending=true;c.grip[0]=true;c.zoom={1,1};c.pan={1,1};
	const int beforeRotationNav=navigations,beforeRotationCamera=adjustments;
	const float beforeRotationZoom=x.worldZoom;frame(c);
	check(buildingAngle<0 && navigations==beforeRotationNav && adjustments==beforeRotationCamera);
	check(x.worldZoom==beforeRotationZoom && lastInput.zoom.x==0 && lastInput.pan.y==0);
	const float angle=buildingAngle;c.select=true;frame(c);check(buildingAngle==angle);
	c.select=false;frame(c);check(buildingAngle==angle);frame(c);check(buildingAngle<angle);
	c.grip[0]=false;const float releasedAngle=buildingAngle;frame(c);
	check(buildingAngle==releasedAngle && adjustments==beforeRotationCamera && x.buildRotation.captured);
	c={};frame(c);check(!x.buildRotation.captured);c.zoom.x=1;frame(c);check(adjustments==beforeRotationCamera+1);
	c.grip[0]=true;menuCapture=true;frame(c);check(buildingAngle==releasedAngle);menuCapture=false;
	expanded=true;frame(c);check(buildingAngle==releasedAngle && !x.buildRotation.captured);expanded=false;
	c={};frame(c);c.grip[0]=true;c.zoom.x=1;c.secondary=true;frame(c);check(buildingAngle==releasedAngle);
	c.secondary=false;frame(c);check(buildingAngle==releasedAngle);
	x.state=XR_SESSION_STATE_VISIBLE;frame(c);check(!x.buildRotation.captured && !x.controlsArmed);
	// GeneralsX @test Codex 14/09/2026 Recovery cannot overwrite good storage.
	XrHello persisted;
	for(int i=0;i<3;++i)persisted.surfaces[i]=persisted.layout.relative[i];
	persisted.layoutDirty=true;saveLayout(persisted);
	XrLayout beforeLost,afterLost;check(beforeLost.load(path));
	persisted.roomPoseLost=true;persisted.surfaces[1].pose.position.y+=2;
	persisted.layoutDirty=true;saveLayout(persisted);
	check(persisted.layoutDirty && afterLost.load(path));
	check(afterLost.relative[1].pose.position.y==beforeLost.relative[1].pose.position.y);
	// Repeated live reset at different head heights moves companions rigidly.
	x=XrHello{};x.controlsArmed=true;canStereo=true;buildPending=false;
	for(int i=0;i<3;++i)x.surfaces[i]=x.layout.relative[i];
	const float separation=xrLength(xrSub(x.surfaces[2].pose.position,x.surfaces[1].pose.position));
	for(float height:{1.5f,.7f,1.7f}) {
		c={};frame(c);frame(c); // Release/re-arm between explicit reset presses.
		for(auto &eye:views)eye.pose.position={1,height,2};
		c={};c.recenter=true;frame(c);
		check(fabsf(xrLength(xrSub(x.surfaces[2].pose.position,x.surfaces[1].pose.position))-separation)<.0001f);
		check(!x.inputArmed && !x.controlsArmed);
		check(fabsf(x.surfaces[1].pose.position.y-(height-.54f))<.0001f);
	}
	x.scene.placed=true;c={};frame(c);frame(c);
	const auto placed=x.surfaces[1];c.recenter=true;frame(c);
	check(x.menu.page==6 && x.menu.open && x.scene.placed);
	check(xrLength(xrSub(placed.pose.position,x.surfaces[1].pose.position))<.0001f);
	// P25 production router: armed/active mode swallows every game action,
	// requires release after entry and exit, and leaves workspace untouched.
	x=XrHello{};x.controlsArmed=true;x.stereoWorld=true;canStereo=true;
	const auto savedSurface=x.surfaces[1];const int beforeNav=navigations,beforePreset=presets;
	check(x.observer.arm(true));c={};c.select=true;c.pan={1,1};c.zoom={1,1};frame(c);
	check(x.observer.mode==XrObserverMode::Armed && !lastInput.select && navigations==beforeNav);
	c={};frame(c);check(!x.observer.requireRelease);
	observerGroundValid=false;c.select=true;frame(c);check(!x.rayHit && x.observer.mode==XrObserverMode::Armed);
	c={};frame(c);observerGroundValid=true;c.select=true;frame(c);
	check(x.observer.mode==XrObserverMode::Active && !lastInput.select && presets==beforePreset);
	c.pan={1,1};c.zoom={1,1};frame(c);check(navigations==beforeNav && !lastInput.select);
	c={};frame(c);c.back=true;frame(c);check(x.observer.mode==XrObserverMode::Off && !lastInput.back);
	check(xrLength(xrSub(savedSurface.pose.position,x.surfaces[1].pose.position))==0);
	c.select=true;frame(c);check(!lastInput.select && !x.controlsArmed);
	c={};frame(c);check(x.controlsArmed);
	check(remove(path)==0);
	printf("PASS %d interaction routing checks\n",checks);
}
