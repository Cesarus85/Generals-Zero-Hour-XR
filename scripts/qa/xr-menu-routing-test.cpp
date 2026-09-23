// GeneralsX @test Codex 13/09/2026 Production workspace actions and ray routing.
#include "XrMenu.h"
#include "XrScene.h"
#include "XrLayout.h"
#include "XrCommands.h"
#include "XrPerformance.h"
#include "XrBuildRotation.h"
#include "XrWorld.h"
#include <cstdio>
#include <cstdlib>
struct XrControllerState {XrPosef aim={{0,0,0,1},{0,0,0}};bool aimValid=true,select=false,back=false;};
struct XrHello {
	XrScene scene;
	XrObserverState observer;
	XrBuildRotation buildRotation;bool inputArmed=true,roomPoseLost=false;
	XrMenuState menu;XrSurface surfaces[3];XrLayout layout;XrSurfaceGrab grab;
	XrPerformance performance;
	bool uprightGame=false,startViewApplied=false;
	XrCommandState commands;bool diorama=false;
	XrPosef layoutAnchor={{0,0,0,1},{0,0,0}};
	XrSessionState state=XR_SESSION_STATE_FOCUSED;
	bool splitVisible=true,panelLatched=true,arranging=false,controlsArmed=true,interactiveGame=true;
	bool stereoWorld=false,stereoVisible=false,recoveryVisible=false,layoutDirty=false,rayVisible=false,rayHit=false,pointerPressed=false,hoverVisible=false;
	int keyboardField=0;bool keyboardReady=false;
	int arrangeSlot=1;float worldZoom=1;XrVector3f rayStart={},rayEnd={};
};
static int checks=0,releases=0,saves=0;static bool locked=false;
static void checkAt(bool b,int line){++checks;if(!b){fprintf(stderr,"menu route check %d failed at line %d\n",checks,line);exit(1);}}
#define check(b) checkAt((b),__LINE__)
static bool XrGameBoot_CanStereoWorld(){return true;}
static bool expanded=false;
static bool XrGameBoot_ExpandedUI(){return expanded;}
static bool XrGameBoot_CanAdjustWorld(){return !locked;}
static bool groundAllowed=true;
static bool XrGameBoot_CanObserveGround(){return groundAllowed;}
static int tactic=-1;
static void XrGameBoot_TacticalAction(int action){tactic=action;}
static int group=-1,operation=-1;
static void XrGameBoot_TacticalGroup(int g,int op){group=g;operation=op;}
static int bookmark=-1;static bool bookmarkSave=false;
static void XrGameBoot_Bookmark(int slot,bool save){bookmark=slot;bookmarkSave=save;}
static std::string XrGameBoot_TacticalReason(int){return {};}
static int communicator=0,language=-1;
static void XrGameBoot_Communicator(){++communicator;}
static void XrGameBoot_SetLanguage(int value){language=value;}
static float surfaceAspect(int){return .25f;}
static void saveLayout(XrHello &){++saves;}
static void updateControls(XrHello &,const XrControllerState &c,XrTime){check(!c.select);++releases;}
static bool xrSceneMenuAction(XrHello &,int){return false;}
static void XrGameBoot_CancelTarget(){}
static int textKeyField=-1,textKeyAscii=-1;
static bool XrGameBoot_DirectConnectTextKey(int field,int ascii){textKeyField=field;textKeyAscii=ascii;return true;}
#include "XrMenuUI.h"
#include "XrCommandUI.h"
int main(){
	XrHello x;XrView views[2]={};views[0].pose.orientation.w=views[1].pose.orientation.w=1;
	for(int i=0;i<3;++i)x.surfaces[i]=x.layout.relative[i];
	// GeneralsX @test Codex 23/09/2026 Keyboard actions remain local to the
	// focused field, including backspace and the explicit Done close path.
	x.menu.open=true;x.menu.page=7;x.keyboardField=2;
	applyMenuAction(x,'7',views);check(textKeyField==2 && textKeyAscii=='7' && x.menu.open);
	applyMenuAction(x,1000,views);check(textKeyAscii==8 && x.menu.open);
	applyMenuAction(x,1001,views);check(!x.menu.open && x.keyboardField==0 && !x.keyboardReady);
	x.menu.open=false;x.menu.page=0;
	const auto uiDock=uiButtonSurface(x),commandsDock=commandButtonSurface(x),groundDock=groundButtonSurface(x);
	check(uiDock.width==.20f && commandsDock.width==uiDock.width && groundDock.width==uiDock.width);
	check(fabsf(commandsDock.pose.position.y-uiDock.pose.position.y+.16f)<.0001f);
	check(fabsf(groundDock.pose.position.y-commandsDock.pose.position.y+.16f)<.0001f);
	check(fabsf(uiDock.pose.position.x-commandsDock.pose.position.x)<.0001f &&
		fabsf(uiDock.pose.position.x-groundDock.pose.position.x)<.0001f);
	check(fabsf(uiDock.pose.position.z-x.surfaces[1].pose.position.z+.08f)<.0001f);
	const auto facing=xrRotate(uiDock.pose.orientation,{0,0,1});
	check(facing.x<-.25f && facing.z>.95f && fabsf(facing.y)<.0001f);
	check(fabsf(commandsDock.pose.orientation.w-uiDock.pose.orientation.w)<.0001f &&
		fabsf(groundDock.pose.orientation.w-uiDock.pose.orientation.w)<.0001f);
	const auto oldBuild=x.surfaces[2];x.surfaces[2].pose.position.x+=.5f;
	check(xrLength(xrSub(uiButtonSurface(x).pose.position,uiDock.pose.position))<.0001f);
	x.surfaces[2]=oldBuild;x.surfaces[1].pose.position.x+=.2f;
	check(fabsf(uiButtonSurface(x).pose.position.x-uiDock.pose.position.x-.2f)<.0001f);
	x.surfaces[1].pose.position.x-=.2f;
	const auto compact=commandSurface(x);applyCommandAction(x,37);
	check(x.commands.tactics && xrCommandHeight(x.commands)==1280);
	const auto expandedConsole=commandSurface(x);
	check(fabsf(compact.pose.position.y+.72f*1024/1536-(expandedConsole.pose.position.y+.72f*1280/1536))<.0001f);
	for(int id=0;id<4;++id)check(xrCommandHit((id%2 ? 564:204)/768.0f,1-(1074+id/2*68)/1280.0f,false,true)==40+id);
	for(int id=0;id<4;++id)check(xrCommandHit((116+id*179)/768.0f,1-1242.0f/1280,false,true)==44+id);
	applyCommandAction(x,43);applyCommandAction(x,46);check(bookmark==2 && bookmarkSave && !x.commands.bookmarkSave);
	applyCommandAction(x,44);check(bookmark==0 && !bookmarkSave);
	for(int id=40;id<43;++id){applyCommandAction(x,id);check(tactic==id && x.stereoWorld);}
	locked=true;applyCommandAction(x,47);check(bookmark==0);locked=false;
	applyCommandAction(x,37);check(!x.commands.tactics);tactic=-1;x.stereoWorld=false;
	for(int i=0;i<3;++i)x.surfaces[i]=x.layout.relative[i];
	x.surfaces[2].pose={{0,0,0,1},{0,0,-1}};
	const auto button=uiButtonSurface(x);XrControllerState c;c.aim.position={button.pose.position.x,button.pose.position.y,0};
	check(updateXrMenu(x,c,views,1));check(!x.menu.open);
	c.select=true;check(updateXrMenu(x,c,views,2));check(!x.menu.open);
	c.select=false;check(updateXrMenu(x,c,views,3));check(x.menu.open && !x.controlsArmed);
	check(x.menu.target==1 && x.menu.surface.width==.64f);
	// Menu background consumes the ray, so no game clicks can pass through it.
	c.aim.position={0,5,0};c.select=true;check(updateXrMenu(x,c,views,4));
	c.select=false;check(updateXrMenu(x,c,views,5));check(x.menu.open);
	c.back=true;check(updateXrMenu(x,c,views,6));check(!x.menu.open);c.back=false;
	check(!updateXrMenu(x,c,views,7));
	// Direct Ground View button shares the board-side column, captures its
	// own laser click and arms placement only after a release.
	x.stereoVisible=true;
	const auto groundButton=groundButtonSurface(x);
	check(xrLength(xrSub(groundButton.pose.position,button.pose.position))>.12f);
	c.aim.position={groundButton.pose.position.x,groundButton.pose.position.y,0};
	check(updateXrMenu(x,c,views,11));c.select=true;
	check(updateXrMenu(x,c,views,12));check(x.observer.mode==XrObserverMode::Off);
	c.select=false;check(updateXrMenu(x,c,views,13));
	check(x.observer.mode==XrObserverMode::Armed && !x.menu.open && !x.controlsArmed);
	x.observer.cancel();x.stereoVisible=false;
	check(!updateXrMenu(x,c,views,14)); // No button when mode is unavailable.
	// P18.1 help captures input on every page and never issues an order.
	const int priorTactic=tactic;
	check(xrMenuHit(700.0f/768,1-36.0f/1024)==24);
	applyMenuAction(x,24,views);check(x.menu.page==4 && x.menu.helpPage==0);
	for(int page=1;page<=kXrControllerHelpPages;++page){applyMenuAction(x,36,views);check(x.menu.helpPage==page%kXrControllerHelpPages);}
	applyMenuAction(x,0,views);check(tactic==priorTactic && x.menu.page==4);
	applyMenuAction(x,34,views);check(x.menu.page==0);
	x.menu.open=true;check(xrEditTarget(x)==1);
	applyMenuAction(x,1,views);check(xrEditTarget(x)==2);
	applyMenuAction(x,23,views);check(xrEditTarget(x)==-1);
	x.menu.page=0;x.menu.open=false;check(xrEditTarget(x)==-1);
	x.arranging=true;x.arrangeSlot=2;check(xrEditTarget(x)==2);x.arranging=false;
	// All adjustment actions operate on the selected surface only.
	applyMenuAction(x,1,views);check(x.menu.target==2);
	const auto oldBoard=x.surfaces[1];const float original=x.surfaces[2].width;
	applyMenuAction(x,3,views);check(x.surfaces[2].width>original);
	applyMenuAction(x,8,views);const auto tilt=x.surfaces[2].pose.orientation;
	check(tilt.x<0 && !x.layout.snap[2]);check(hoverCardSurface(x).pose.orientation.x==tilt.x);
	applyMenuAction(x,6,views);check(x.surfaces[2].pose.position.y>.07f);
	applyMenuAction(x,4,views);check(x.surfaces[2].pose.position.z>-.95f);
	check(x.surfaces[1].width==oldBoard.width);
	applyMenuAction(x,0,views);x.surfaces[1].width=3.99f;applyMenuAction(x,3,views);check(x.surfaces[1].width==4);
	applyMenuAction(x,12,views);check(x.worldZoom>1);
	locked=true;const float zoom=x.worldZoom;applyMenuAction(x,12,views);check(x.worldZoom==zoom);locked=false;
	applyMenuAction(x,16,views);check(x.menu.page==5);
	x.menu.page=0;applyMenuAction(x,14,views);check(x.arranging && !x.menu.open && !x.controlsArmed);
	check(releases>4 && saves>7);
	// Loss of focus cannot turn a held trigger into a fresh workspace click.
	x.arranging=false;x.menu.open=false;c.aim.position={button.pose.position.x,button.pose.position.y,0};c.select=true;
	x.state=XR_SESSION_STATE_VISIBLE;updateXrMenu(x,c,views,8);
	x.state=XR_SESSION_STATE_FOCUSED;updateXrMenu(x,c,views,9);c.select=false;updateXrMenu(x,c,views,10);
	check(!x.menu.open);
	// Every tab is an action, not a leaking world click. Targeted commands
	// activate stereo explicitly; locked camera prevents arming.
	applyMenuAction(x,21,views);check(x.menu.page==1);
	x.stereoWorld=false;locked=true;applyMenuAction(x,3,views);check(tactic==-1 && !x.stereoWorld);locked=false;
	for(int action=0;action<=13;++action) {
		x.menu.open=true;applyMenuAction(x,action,views);check(tactic==action);
		if(action<=8) check(x.stereoWorld && !x.menu.open && !x.controlsArmed);
	}
	applyMenuAction(x,16,views);check(tactic==31);
	applyMenuAction(x,22,views);check(x.menu.page==2);
	for(int action=0;action<12;++action) {applyMenuAction(x,action,views);check(tactic==action+20);}
	applyMenuAction(x,12,views);check(tactic==12);applyMenuAction(x,13,views);check(tactic==10);
	applyMenuAction(x,23,views);check(x.menu.page==3);
	// Recover from flat mode BEFORE any split capture exists. The old action
	// required splitVisible and never cleared the override that prevented it.
	x.splitVisible=false;x.uprightGame=true;x.stereoWorld=false;x.startViewApplied=false;
	x.layout.startStereo=true;xrApplyWorldStartup(x,false);check(!x.startViewApplied);
	xrApplyWorldStartup(x,true);check(x.stereoWorld && x.startViewApplied && !x.uprightGame);
	x.stereoWorld=false;xrApplyWorldStartup(x,true);check(x.stereoWorld); // Return from cinematic.
	xrRequestWorldView(x,false);xrApplyWorldStartup(x,true);check(x.stereoWorld);
	x.menu.page=0;applyMenuAction(x,16,views);
	check(x.menu.page==5 && x.stereoWorld && !x.uprightGame);
	x.splitVisible=true;x.menu.page=3;
	const int beforePerfSaves=saves,beforePerfTactic=tactic;const auto epoch=x.performance.epoch;
	check(!x.performance.volumeShadows && !x.performance.enabled);
	applyMenuAction(x,12,views);check(x.performance.volumeShadows);
	applyMenuAction(x,13,views);check(x.performance.enabled && x.performance.epoch>epoch);
	check(x.performance.multiviewStereo && !x.performance.atlasStereo);
	applyMenuAction(x,14,views);check(!x.performance.atlasStereo && !x.performance.multiviewStereo);
	applyMenuAction(x,14,views);check(x.performance.atlasStereo && !x.performance.multiviewStereo);
	applyMenuAction(x,14,views);check(!x.performance.atlasStereo && x.performance.multiviewStereo);
	check(x.performance.elideWorldCopy);applyMenuAction(x,15,views);check(!x.performance.elideWorldCopy);
	applyMenuAction(x,15,views);check(x.performance.elideWorldCopy);
	applyMenuAction(x,12,views);applyMenuAction(x,13,views);
	check(!x.performance.volumeShadows && !x.performance.enabled);
	check(saves==beforePerfSaves && tactic==beforePerfTactic);
	applyMenuAction(x,0,views);check(x.stereoWorld);applyMenuAction(x,2,views);check(x.layout.startStereo);
	applyMenuAction(x,1,views);check(x.stereoWorld && x.layout.startStereo);
	applyMenuAction(x,3,views);check(x.layout.startStereo);
	applyMenuAction(x,4,views);check(!x.layout.healthBars);applyMenuAction(x,5,views);check(!x.layout.unitRings);
	applyMenuAction(x,6,views);check(!x.layout.boardFrame);
	x.menu.open=true;x.controlsArmed=true;applyMenuAction(x,8,views);
	check(x.layout.leftHanded && !x.menu.open && !x.controlsArmed);
	applyMenuAction(x,8,views);check(!x.layout.leftHanded);
	applyMenuAction(x,10,views);check(x.layout.resolutionTier==1);
	applyMenuAction(x,10,views);check(x.layout.resolutionTier==2);
	applyMenuAction(x,10,views);check(x.layout.resolutionTier==0);
	applyMenuAction(x,11,views);check(language==1 && x.layout.language==XrLanguage::English && g_xrLanguage==XrLanguage::English);
	applyMenuAction(x,11,views);check(language==0 && x.layout.language==XrLanguage::German);
	x.surfaces[1].width=4;applyMenuAction(x,9,views);
	check(x.layout.startStereo && x.layout.commandsVisible && x.surfaces[1].width==1.65f && x.surfaces[2].width==1.8f && !x.menu.open);
	check(fabsf(x.surfaces[2].pose.position.z+1.18f)<.0001f);
	const int beforeBlank=saves;applyMenuAction(x,15,views);check(saves==beforeBlank);
	applyMenuAction(x,20,views);check(x.menu.page==0);
	// Direct console actions require no UI tab, close or world click.
	x.menu.open=false;check(commandsAvailable(x));x.layout.commandsVisible=true;
	for(int i=0;i<16;++i){applyCommandAction(x,i);check(tactic==xrCommandAction(i));}
	for(int op=0;op<4;++op)for(int g=0;g<10;++g){
		if(op)applyCommandAction(x,29+op);applyCommandAction(x,20+g);
		check(group==g && operation==op && x.commands.groupOperation==0);
	}
	applyCommandAction(x,30);applyCommandAction(x,30);check(x.commands.groupOperation==0);
	locked=true;const int previous=tactic;applyCommandAction(x,5);check(tactic==previous);locked=false;
	const auto console=commandSurface(x);const auto angle=console.pose.orientation;
	const auto inward=xrRotate(angle,{0,0,1});check(inward.x>.6f && console.width==.72f);
	// Console remains gravity-upright for freely pitched/rolled build windows.
	for(float pitch:{-1.57f,-.8f,-.42f,0.0f,.5f})for(float roll:{-.6f,0.0f,.6f})for(float yaw:{-2.0f,0.0f,2.0f}) {
		x.surfaces[2].pose.orientation=xrMul(xrAxisAngle({0,1,0},yaw),xrMul(xrAxisAngle({1,0,0},pitch),xrAxisAngle({0,0,1},roll)));
		const auto up=xrRotate(commandSurface(x).pose.orientation,{0,1,0});
		check(fabsf(up.x)<1e-5f && fabsf(up.y-1)<1e-5f && fabsf(up.z)<1e-5f);
	}
	applyCommandAction(x,34);check(x.commands.help && x.commands.groupOperation==0);
	applyCommandAction(x,36);check(x.commands.helpPage==1);
	check(xrCommandHit(.2f,1-960.0f/1024,true)==34);
	check(xrCommandHit(.7f,1-960.0f/1024,true)==36);
	check(xrCommandHit(.2f,1-172.0f/1024,true)==-1);
	applyCommandAction(x,34);check(!x.commands.help);
	applyCommandAction(x,35);check(communicator==1);
	locked=true;applyCommandAction(x,35);check(communicator==1);locked=false;
	// GeneralsX @test Ultron 15/09/2026 P21 redesigned console: assert each
	// direct order at its new shared-table rect center (XrPanelLayout.h).
	const int cx[16]={564,204,564,204,564,144,384,144,204,624,264,144,384,384,624,624};
	const int cy[16]={406,254,254,330,330,514,514,582,406,650,718,650,650,582,582,514};
	for(int id=0;id<16;++id)check(xrCommandHit(cx[id]/768.0f,1-cy[id]/1024.0f)==id);
	for(int id=0;id<10;++id)check(xrCommandHit((64+id*71)/768.0f,1-816.0f/1024.0f)==20+id);
	for(int id=0;id<3;++id)check(xrCommandHit((144+id*240)/768.0f,1-876.0f/1024.0f)==30+id);
	check(xrCommandHit(700.0f/768,1-50.0f/1024)==33);
	check(xrCommandHit(264.0f/768,1-514.0f/1024)==-1);check(xrCommandHit(.5f,1-470.0f/1024)==-1);check(xrCommandHit(.1f,.99f)==-1);
	// Nonmodal: a ray outside the panel is free; background is captured.
	x.surfaces[2].pose={{0,0,0,1},{0,0,-1}};c={};c.aim.position={5,0,0};
	check(!updateCommands(x,c,100));
	const auto commandToggle=commandButtonSurface(x);
	c.aim.position={commandToggle.pose.position.x,commandToggle.pose.position.y,0};
	check(updateCommands(x,c,100));c.select=true;check(updateCommands(x,c,100));
	c.select=false;check(updateCommands(x,c,100));check(!x.layout.commandsVisible);
	check(updateCommands(x,c,100));c.select=true;check(updateCommands(x,c,100));
	c.select=false;check(updateCommands(x,c,100));check(x.layout.commandsVisible);
	const auto fixed=commandSurface(x);
	// GeneralsX @test Ultron 15/09/2026 P21 the console center is a real
	// button in the new layout; the no-dispatch contract moves to a true
	// gutter between the selection buttons (canvas x=264, y=514).
	const auto gutter=xrAdd(fixed.pose.position,xrRotate(fixed.pose.orientation,{(264.0f/768-.5f)*fixed.width,(.5f-514.0f/1024)*fixed.width*1024/768,0}));
	c.aim.position={gutter.x,gutter.y,0};
	check(updateCommands(x,c,101)); // panel gutter
	c.select=true;check(updateCommands(x,c,102));c.select=false;check(updateCommands(x,c,103));check(tactic==previous);
	applyCommandAction(x,33);check(!x.layout.commandsVisible);
	check(!updateCommands(x,c,104));
	x.layout.commandsVisible=true;c.back=true;check(!updateCommands(x,c,105));c.back=false;
	// A world-origin press must never activate a console button on release.
	c.aim.position={5,0,0};updateCommands(x,c,106);c.select=true;updateCommands(x,c,107);
	const auto target=xrAdd(fixed.pose.position,xrRotate(fixed.pose.orientation,{(564.0f/768-.5f)*fixed.width,(.5f-406.0f/1024)*fixed.width*1024/768,0}));
	c.aim.position={target.x,target.y,0};updateCommands(x,c,108);c.select=false;updateCommands(x,c,109);check(tactic==previous);
	// A fresh click now reaches precisely command zero.
	updateCommands(x,c,110);c.select=true;updateCommands(x,c,111);c.select=false;updateCommands(x,c,112);check(tactic==0);
	x.commands.groupOperation=1;x.menu.open=true;check(!updateCommands(x,c,113));check(x.commands.groupOperation==0);
	x.menu.open=false;expanded=true;check(!commandsAvailable(x));expanded=false;
	// P11.1: the UI button becomes Done, not another modal submenu.
	x={};for(int i=0;i<3;++i)x.surfaces[i]=x.layout.relative[i];
	x.surfaces[2].pose={{0,0,0,1},{0,0,-1}};c={};
	const auto arrangeButton=uiButtonSurface(x);
	c.aim.position={arrangeButton.pose.position.x,arrangeButton.pose.position.y,0};
	applyMenuAction(x,14,views);check(x.arranging && !x.menu.open);
	updateXrMenu(x,c,views,120);c.select=true;updateXrMenu(x,c,views,121);
	check(x.arranging);c.select=false;updateXrMenu(x,c,views,122);
	check(!x.arranging && !x.menu.open && !x.controlsArmed && x.grab.mode==0);
	for(int page=0;page<4;++page) {
		x.arranging=true;x.menu.open=true;x.menu.page=page;x.controlsArmed=true;
		applyMenuAction(x,17,views);check(!x.arranging && !x.menu.open && !x.controlsArmed);
	}
	x.arranging=true;x.menu.open=true;c.back=true;updateXrMenu(x,c,views,123);
	check(!x.arranging && !x.menu.open && !x.controlsArmed);
	// New button is directly reachable in the default page, preserves geometry,
	// and never abandons a confirmed play surface without a second click.
	x={};for(int i=0;i<3;++i)x.surfaces[i]=x.layout.relative[i];
	check(xrMenuHit(.5f,1-949.0f/1024,0)==18);
	x.menu.open=true;const auto board=x.surfaces[1];
	x.scene.placed=true;applyMenuAction(x,18,views);
	check(x.menu.page==6 && x.scene.placed && x.menu.open);
	check(xrLength(xrSub(board.pose.position,x.surfaces[1].pose.position))<.0001f);
	applyMenuAction(x,19,views);check(x.menu.page==0 && x.scene.placed);
	check(xrLength(xrSub(board.pose.position,x.surfaces[1].pose.position))<.0001f);
	applyMenuAction(x,18,views);
	const auto local=xrPoseMul(xrPoseInverse(x.surfaces[1].pose),x.surfaces[2].pose);
	for(auto &eye:views)eye.pose={xrMul(xrAxisAngle({0,1,0},1.1f),xrAxisAngle({1,0,0},-.7f)),{1,1.6f,2}};
	applyMenuAction(x,18,views);
	check(!x.scene.placed && !x.menu.open && !x.arranging && !x.inputArmed);
	const auto after=xrPoseMul(xrPoseInverse(x.surfaces[1].pose),x.surfaces[2].pose);
	check(xrLength(xrSub(local.position,after.position))<.0001f);
	check(fabsf(xrRotate(x.surfaces[1].pose.orientation,{0,0,1}).y-1)<.0001f);
	check(x.surfaces[1].width==board.width && x.surfaces[2].width==1.8f);
	// Free workspace needs only one click, also from the main menu.
	x.interactiveGame=false;x.splitVisible=false;x.menu.open=true;
	applyMenuAction(x,18,views);check(!x.menu.open && !x.scene.placed);
	printf("PASS %d production menu action/routing checks\n",checks);
}
