// GeneralsX @test Codex 14/09/2026 Production dialog detection and UI geometry.
#include "XrLayout.h"
#include "XrWorkspacePlacement.h"
#include "XrTracking.h"
#include "XrWorld.h"
#include "XrLayers.h"
#include "XrTactics.h"
#include "XrCameraProfile.h"
#include <cstdio>
#include <cstdlib>
constexpr bool FALSE=false;
static bool GX_XR_OffscreenBoot=true,interactive=true;
static bool XrGameBoot_IsInteractiveGame(){return interactive;}
static bool diplomacy=false;
static bool IsDiplomacyVisible(){return diplomacy;}
struct ControlBar {bool visible=false;bool isPurchaseScienceVisible(){return visible;}} science;
static ControlBar *TheControlBar=&science;
struct UI {bool quit=false,video=false;bool isQuitMenuVisible(){return quit;}bool videoBuffer(){return video;}} ui;
struct WM {bool modal=false;bool hasModalWindow(){return modal;}} wm;
struct WindowLayout {bool hidden=true;bool isHidden(){return hidden;}} options;
struct Shell {WindowLayout *layout=nullptr;WindowLayout *getOptionsLayout(bool){return layout;}bool active=false;bool isShellActive(){return active;}} shell;
static bool s_splitEnabled=true;
struct Global {bool m_loadScreenRender=false,m_disableRender=false,m_playIntro=false,m_afterIntro=false;} globals;
static Global *TheGlobalData=&globals;
struct Display {bool movie=false,letterbox=false;bool isMoviePlaying(){return movie;}bool isLetterBoxed(){return letterbox;}} display;
static Display *TheDisplay=&display;
struct Camera {
	bool moving=false,locked=false;int writes=0;
	bool isCameraMovementFinished(){return !moving;}bool isUserControlLocked(){return locked;}
	void userSetAngle(float){++writes;moving=false;}void userSetPitch(float){++writes;moving=false;}
	void userZoom(float){++writes;moving=false;}void userSetAngleToDefault(){++writes;moving=false;}
	void userSetZoom(float){++writes;moving=false;}
	float getHeightAboveGround(){return 500;}float getPitch(){return 1;}float getAngle(){return 0;}
	float getZoom(){return 1;}float getDefaultPitch(){return .6f;}
} camera;
static Camera *TheTacticalView=&camera;
static bool s_hasCameraFavorite=true;
static XrCameraProfile s_cameraFavorite;
static XrWorldFrame s_worldFrame;
static bool d3d8gles_XRStereoMultiview(){return false;}
static unsigned d3d8gles_XROrdinarySkipped(){return 0;}
bool d3d8gles_XRStereoAtlas(){return true;}
static constexpr int kXrGameWidth=1280,kXrGameHeight=720;
#define GXLOG(...) ((void)0)
static UI *TheInGameUI=&ui;static WM *TheWindowManager=&wm;static Shell *TheShell=&shell;
struct XrHello {XrSurface surfaces[3];};
static XrGameRect bar=xrCommandRect(.28f);
static XrGameRect XrGameBoot_CommandRect(){return bar;}
static XrGameRect XrGameBoot_WorldRect(){return {0,.3f,1,.7f};}
static int XrGameBoot_GameWidth(){return 1280;}
static int XrGameBoot_GameHeight(){return 720;}
#include "xr-workspace-bridge.inc"
static int checks=0;
static void check(bool v){++checks;if(!v){fprintf(stderr,"workspace check %d failed\n",checks);exit(1);}}
static void near(float a,float b){check(std::isfinite(a+b) && fabsf(a-b)<.0001f);}
int main(int argc,char **argv) {
	check(argc==2);
	// Estimated positions must neither anchor a workspace nor enable grabbing.
	for(unsigned flags=0;flags<16;++flags) {
		check(xrTrackedViews(flags)==(flags==15));
		check(xrTrackedSpace(flags)==(flags==15));
	}
	// Start after a turned/seated movie, then remain stationary during play.
	for(float yaw:{-2.4f,0.0f,1.7f})for(float height:{.7f,1.65f}) {
		XrLayout prefs;XrSurface surfaces[3],menu;XrPosef anchor={{0,0,0,1},{0,0,0}};
		XrView eyes[2]={};for(auto &eye:eyes)eye.pose={xrAxisAngle({0,1,0},yaw),{2,height,-3}};
		prefs.gamePlacementPending=true;
		check(!xrPlaceWorkspaceForGame(prefs,surfaces,anchor,menu,eyes,false));
		check(prefs.gamePlacementPending);
		check(xrPlaceWorkspaceForGame(prefs,surfaces,anchor,menu,eyes,true));
		const auto before=surfaces[1];
		near(before.pose.position.y,height-.54f);
		const auto relative=xrPoseMul(xrPoseInverse(anchor),surfaces[2].pose);
		near(relative.position.z,-1.18f);near(relative.position.y,-.38f);
		for(auto &eye:eyes)eye.pose.position.y+=.5f;
		check(!xrPlaceWorkspaceForGame(prefs,surfaces,anchor,menu,eyes,true));
		near(surfaces[1].pose.position.y,before.pose.position.y);
		prefs.sessionPlacementChosen=true;prefs.gamePlacementPending=true;
		check(!xrPlaceWorkspaceForGame(prefs,surfaces,anchor,menu,eyes,true));
		near(surfaces[1].pose.position.y,before.pose.position.y);
		// Recenter is rigid: board/build separation, tilt and scale survive.
		const auto local=xrPoseMul(xrPoseInverse(surfaces[1].pose),surfaces[2].pose);
		xrRecenterWorkspace(surfaces,anchor,menu,eyes);
		const auto after=xrPoseMul(xrPoseInverse(surfaces[1].pose),surfaces[2].pose);
		near(xrLength(xrSub(local.position,after.position)),0);
		near(local.orientation.x,after.orientation.x);near(surfaces[2].width,1.8f);
	}
	for(auto chosen:{XrLanguage::German,XrLanguage::English})for(int system:{0,1}) {
		XrLayout saved;saved.language=chosen;saved.initializeLanguage(true,system);check(saved.language==chosen);
		saved.initializeLanguage(false,system);check(saved.language==(system==0 ? XrLanguage::German:XrLanguage::English));
	}
	check(GX_XR_SplitUIAllowed());
	// The old favorite application stopped scripted camera movement as soon
	// as the short native lock expired, even while letterboxed/movie playback.
	for(bool *block:{&globals.m_loadScreenRender,&globals.m_disableRender,&globals.m_playIntro,&globals.m_afterIntro,&display.movie,&display.letterbox,&ui.video,&camera.moving,&camera.locked,&shell.active}) {
		*block=true;const int before=camera.writes;
		check(!XrGameBoot_CameraPreset(kXrCameraFavorite));check(!XrGameBoot_AdjustCamera(.1f,.1f));
		check(*block && camera.writes==before);*block=false;
	}
	check(XrGameBoot_CameraPreset(kXrCameraFavorite) && camera.writes==3);
	s_hasCameraFavorite=false;check(XrGameBoot_CameraPreset(1) && camera.writes==6);
	s_splitEnabled=false;check(XrGameBoot_CanControlCamera());s_splitEnabled=true; // Normal flat gameplay still permits camera control.
	TheTacticalView=nullptr;check(!XrGameBoot_CanControlCamera());TheTacticalView=&camera;
	display.letterbox=true;check(XrGameBoot_PresentationStatus(false,true).find("Kamerasequenz")!=std::string::npos);display.letterbox=false;
	check(XrGameBoot_PresentationStatus(false,true).find("vorbereitet")!=std::string::npos);
	xrStereoExtent(2064,2160,s_worldFrame.width,s_worldFrame.height,true);
	check(XrGameBoot_PresentationStatus(true,true).find("1920x2009")!=std::string::npos);
	for(bool *block:{&globals.m_loadScreenRender,&globals.m_disableRender,&globals.m_playIntro,&globals.m_afterIntro,&display.movie,&display.letterbox,&ui.video,&shell.active}) {
		*block=true;check(!GX_XR_SplitUIAllowed());*block=false;check(GX_XR_SplitUIAllowed());
	}
	s_splitEnabled=false;check(!GX_XR_SplitUIAllowed());s_splitEnabled=true;
	TheDisplay=nullptr;check(!GX_XR_SplitUIAllowed());TheDisplay=&display;
	TheInGameUI=nullptr;check(GX_XR_SplitUIAllowed());TheInGameUI=&ui;
	// Science is full-canvas UI over the world, not a fullscreen movie.
	science.visible=true;check(GX_XR_SplitUIAllowed());science.visible=false;
	science.visible=true;check(XrGameBoot_ExpandedUI());science.visible=false;check(!XrGameBoot_ExpandedUI());
	TheControlBar=nullptr;check(!XrGameBoot_ExpandedUI());TheControlBar=&science;
	check(!XrGameBoot_ExpandedUI());ui.quit=true;check(XrGameBoot_ExpandedUI());
	ui.quit=false;wm.modal=true;check(XrGameBoot_ExpandedUI());wm.modal=false;
	diplomacy=true;check(XrGameBoot_ExpandedUI());diplomacy=false;check(!XrGameBoot_ExpandedUI());
	shell.layout=&options;check(!XrGameBoot_ExpandedUI());options.hidden=false;check(XrGameBoot_ExpandedUI());
	GX_XR_OffscreenBoot=false;check(!XrGameBoot_ExpandedUI());GX_XR_OffscreenBoot=true;
	interactive=false;check(!XrGameBoot_ExpandedUI());interactive=true;options.hidden=true;
	// Full UI expands upward from the original bottom edge, not into the table.
	XrLayout layout;check(layout.startStereo && !layout.highQuality && layout.commandsVisible);
	near(layout.relative[2].pose.position.z,-1.18f);
	check(layout.formatVersion==10 && !layout.upgradeDefaults());
	XrHello x;for(int i=0;i<3;++i)x.surfaces[i]=layout.relative[i];
	for(float crop:{.18f,.3f,.6f}) for(float pitch:{0.0f,-.42f,-1.2f}) {
		bar=xrCommandRect(crop);x.surfaces[2].pose.orientation=xrAxisAngle({1,0,0},pitch);
		ui.quit=false;const auto compact=displayedSurface(x,2);const float compactAspect=surfaceAspect(2);
		const auto bottom=xrAdd(compact.pose.position,xrRotate(compact.pose.orientation,{0,-compact.width*compactAspect*.5f,0}));
		ui.quit=true;const auto full=displayedSurface(x,2);const float aspect=surfaceAspect(2);
		const auto expandedBottom=xrAdd(full.pose.position,xrRotate(full.pose.orientation,{0,-full.width*aspect*.5f,0}));
		near(bottom.x,expandedBottom.x);near(bottom.y,expandedBottom.y);near(bottom.z,expandedBottom.z);
		near(aspect,9.0f/16);near(surfaceRect(2).h,1);near(surfaceRect(3).h,0);
		float matrix[16];surfaceMatrix(full,matrix);
		for(float u:{.01f,.5f,.99f})for(float v:{.01f,.5f,.99f}) {
			const auto target=xrAdd(full.pose.position,xrRotate(full.pose.orientation,{(u-.5f)*full.width,(v-.5f)*full.width*aspect,0}));
			const XrPosef aim={full.pose.orientation,xrAdd(target,xrRotate(full.pose.orientation,{0,0,1}))};
			float pickedU=0,pickedV=0;check(panelRayUV(matrix,aspect,aim,&pickedU,&pickedV));near(pickedU,u);near(pickedV,v);
		}
	}
	// Apply the requested photo default once; preserve independent preferences.
	layout.formatVersion=6;layout.leftHanded=true;layout.worldZoom=.61f;layout.relative[0].width=.96f;
	layout.relative[1].width=3;layout.startStereo=false;layout.commandsVisible=false;
	check(layout.upgradeDefaults());check(layout.startStereo && layout.commandsVisible && layout.leftHanded);
	near(layout.worldZoom,.61f);near(layout.relative[0].width,.96f);near(layout.relative[1].width,1.65f);
	check(layout.relative[2].pose.position.z<layout.relative[1].pose.position.z);
	check(layout.relative[2].pose.position.y>layout.relative[1].pose.position.y);
	check(xrRotate(layout.relative[2].pose.orientation,{0,0,1}).y>.3f);
	layout.relative[2].width=1.9f;layout.highQuality=false;layout.startStereo=false;
	layout.language=XrLanguage::English;
	check(layout.save(argv[1]));XrLayout restored;check(restored.load(argv[1]));check(!restored.upgradeDefaults());
	near(restored.relative[2].width,1.9f);check(!restored.highQuality && !restored.startStereo && restored.leftHanded);
	check(restored.language==XrLanguage::English);
	// P20 startup retains preferences but never trusts room-relative geometry
	// without a persistent room anchor. All three surfaces must be reachable.
	XrLayout sessionStart=restored;
	sessionStart.relative[0].pose.position={3,2,-4};sessionStart.relative[0].width=2.4f;
	sessionStart.relative[1].pose.position={-3,1,2};sessionStart.relative[1].width=4.0f;
	sessionStart.relative[2].pose.position={2,-2,3};sessionStart.relative[2].width=2.5f;
	sessionStart.worldZoom=.61f;sessionStart.highQuality=true;sessionStart.leftHanded=true;
	sessionStart.language=XrLanguage::English;sessionStart.commandsVisible=false;sessionStart.startStereo=false;
	sessionStart.applyFreeStandingStart();
	near(sessionStart.relative[0].width,1.35f);near(sessionStart.relative[0].pose.position.z,-1.1f);
	near(sessionStart.relative[1].width,1.65f);near(sessionStart.relative[1].pose.position.y,-.54f);
	near(sessionStart.relative[2].width,1.8f);near(sessionStart.relative[2].pose.position.z,-1.18f);
	check(sessionStart.startStereo && sessionStart.commandsVisible && sessionStart.highQuality && sessionStart.leftHanded);
	check(sessionStart.language==XrLanguage::English);near(sessionStart.worldZoom,.61f);
	// v7/v8 retain P15 quality migration and receive only the additional UI setback.
	layout.formatVersion=7;check(layout.upgradeDefaults());near(layout.relative[2].width,1.9f);check(layout.startStereo);
	// P16.1 preserves the complete current arrangement except the requested UI depth.
	layout.formatVersion=8;layout.highQuality=true;layout.startStereo=false;
	layout.relative[1].width=2.7f;layout.relative[1].pose.position={.2f,-.7f,-.9f};
	layout.relative[2].pose.orientation=xrAxisAngle({1,0,0},-.31f);
	const auto previous=layout;
	check(layout.upgradeDefaults() && layout.formatVersion==10 && !layout.highQuality && layout.startStereo);
	for(int i=0;i<3;++i){
		near(layout.relative[i].width,previous.relative[i].width);
		const auto &a=layout.relative[i].pose;const auto &b=previous.relative[i].pose;
		near(a.position.x,b.position.x);near(a.position.y,b.position.y);near(a.position.z,b.position.z-(i==2 ? .1f:0));
		near(a.orientation.x,b.orientation.x);near(a.orientation.y,b.orientation.y);near(a.orientation.z,b.orientation.z);near(a.orientation.w,b.orientation.w);
		check(layout.snap[i]==previous.snap[i]);
	}
	check(layout.language==previous.language && layout.leftHanded==previous.leftHanded);
	near(layout.worldZoom,previous.worldZoom);
	layout.highQuality=true;check(!layout.upgradeDefaults() && layout.highQuality);
	check(layout.save(argv[1]));XrLayout deliberateHigh;check(deliberateHigh.load(argv[1]));
	check(!deliberateHigh.upgradeDefaults() && deliberateHigh.highQuality);
	// Real v9 file: move only UI depth once, preserve every independent choice.
	layout=previous;layout.formatVersion=9;layout.highQuality=true;layout.startStereo=false;
	layout.healthBars=false;layout.unitRings=false;layout.boardFrame=false;layout.commandsVisible=false;
	FILE *old=fopen(argv[1],"w");check(old!=nullptr);fprintf(old,"GENERALS_XR_LAYOUT 9\n");
	for(int i=0;i<3;++i){const auto &s=layout.relative[i];const auto &p=s.pose.position;const auto &q=s.pose.orientation;
		fprintf(old,"%.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d\n",s.width,p.x,p.y,p.z,q.x,q.y,q.z,q.w,int(layout.snap[i]));}
	fprintf(old,"%.9g\n0 0 0 0\n0\n%d\n1\n%d\n",layout.worldZoom,int(layout.leftHanded),int(layout.language));fclose(old);
	XrLayout v9;check(v9.load(argv[1]) && v9.formatVersion==9 && v9.upgradeDefaults());
	check(v9.highQuality && !v9.startStereo && !v9.healthBars && !v9.unitRings && !v9.boardFrame && !v9.commandsVisible);
	check(v9.language==layout.language && v9.leftHanded==layout.leftHanded);near(v9.worldZoom,layout.worldZoom);
	for(int i=0;i<3;++i){const auto &a=v9.relative[i];const auto &b=layout.relative[i];
		near(a.width,b.width);near(a.pose.position.x,b.pose.position.x);near(a.pose.position.y,b.pose.position.y);
		near(a.pose.position.z,b.pose.position.z-(i==2 ? .1f:0));
		near(a.pose.orientation.x,b.pose.orientation.x);near(a.pose.orientation.y,b.pose.orientation.y);
		near(a.pose.orientation.z,b.pose.orientation.z);near(a.pose.orientation.w,b.pose.orientation.w);check(v9.snap[i]==layout.snap[i]);}
	const float migratedZ=v9.relative[2].pose.position.z;check(v9.save(argv[1]));
	XrLayout current;check(current.load(argv[1]) && current.formatVersion==10 && !current.upgradeDefaults());
	near(current.relative[2].pose.position.z,migratedZ);
	current.relative[2].pose.position.z=-1.6f;check(current.save(argv[1]));
	check(current.load(argv[1]) && !current.upgradeDefaults());near(current.relative[2].pose.position.z,-1.6f);
	// Keep an unusually distant valid custom pose valid instead of crossing the 5 m limit.
	current.formatVersion=9;current.relative[2].pose.position={0,0,-4.95f};check(current.upgradeDefaults());
	near(current.relative[2].pose.position.z,-4.95f);check(current.save(argv[1]));check(current.load(argv[1]));
	for(const char *language:{"2","-1",""}) {
		FILE *bad=fopen(argv[1],"w");check(bad!=nullptr);fprintf(bad,"GENERALS_XR_LAYOUT 8\n");
		for(int i=0;i<3;++i)fprintf(bad,"1 0 0 -1 0 0 0 1 0\n");
		fprintf(bad,"1\n1 1 1 1\n1\n0\n1\n%s\n",language);fclose(bad);
		check(!restored.load(argv[1]));near(restored.relative[2].width,1.9f);check(restored.language==XrLanguage::English);
	}
	// Malformed v7 quality must not replace a valid arrangement.
	for(const char *quality:{"2",""}) {
		FILE *bad=fopen(argv[1],"w");check(bad!=nullptr);fprintf(bad,"GENERALS_XR_LAYOUT 7\n");
		for(int i=0;i<3;++i)fprintf(bad,"1 0 0 -1 0 0 0 1 0\n");
		fprintf(bad,"1\n1 1 1 1\n1\n0\n%s\n",quality);fclose(bad);
		check(!restored.load(argv[1]));near(restored.relative[2].width,1.9f);
	}
	for(int width:{0,64,1000,2064,3000,10000})for(int height:{0,64,1000,2160,3000,10000})for(bool high:{false,true}) {
		int w=0,h=0;xrStereoExtent(width,height,w,h,high);check(w>=64 && w<=2048 && h>=64 && h<=2048);
	}
	check(std::strstr(xrOrderHint(XrOrderMode::Guard,1),"verbündetes Objekt")!=nullptr);
	check(std::strstr(xrOrderHint(XrOrderMode::Guard,0),"Zuerst")!=nullptr);
	check(xrOrderHint(XrOrderMode::Context,1)[0]==0);
	remove(argv[1]);printf("PASS %d production workspace/layout/quality checks\n",checks);
}
