// GeneralsX @test Codex 14/09/2026 Compile the actual XR loader presenter
// against compositor spies. No game data, graphics context or device needed.
#include <openxr/openxr.h>
#include "XrLoadingFrame.h"
#include "XrCommands.h"
#include "XrLayout.h"
#include "XrTracking.h"
#include "XrWorld.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <vector>
static int checks=0;
static void check(bool value){++checks;if(!value){fprintf(stderr,"presenter check %d failed\n",checks);exit(1);}}
#define XR_LOG(...) ((void)0)
#define XR_LOGE(...) ((void)0)
static std::atomic<bool> g_stopFlag{false};
struct XrControllerState {bool aimValid=true,back=false,select=false;};
struct XrHello {
	struct Perf {void invalidate(){}} performance;
	struct Timer {void end(bool){}} gpuTimer;
	XrSession session=XR_NULL_HANDLE;XrSpace localSpace=XR_NULL_HANDLE;
	XrPassthroughLayerFB passthroughLayer=XR_NULL_HANDLE;
	XrSessionState state=XR_SESSION_STATE_FOCUSED;
	bool sessionRunning=true,passthroughActive=true,splitVisible=true,stereoVisible=true,diorama=false;
	bool loadingPresentation=false,arranging=true,controlsArmed=true,inputArmed=true;
	bool rayVisible=true,pointerVisible=true,hoverVisible=true;
	bool recoveryVisible=false;
	XrObserverState observer;bool renderedObserver=true;XrTime observerFadeStart=0;
	XrMenuState menu;XrCommandState commands;XrSurfaceGrab grab;XrLayout layout;
	XrTime previousInputTime=1;int controls=0;
	unsigned frame=0,viewWidth=1000,viewHeight=1100;
	struct Swapchain {XrSwapchain handle=XR_NULL_HANDLE;} swapchains[2];
};
enum class XrGameKey {Back};
static void (*callback)(void *)=nullptr;static void *callbackContext=nullptr;
static void XrGameBoot_SetLoadingPresenter(void (*fn)(void *),void *context){callback=fn;callbackContext=context;}
static std::vector<bool> keys;
static void XrGameBoot_Key(XrGameKey,bool down){keys.push_back(down);}
static int waits=0,begins=0,ends=0,draws=0,invalidations=0;
static bool begun=true,render=true,valid=true,drawOK=true,endOK=true,waitOK=true;
static XrTime predicted=100;
static XrControllerState controller;
static void pollEvents(XrHello &,bool &){}
static XrControllerState pollControls(int,XrSession,XrSpace,XrTime time,bool,bool){check(time==predicted);return controller;}
static void updateControls(XrHello &,const XrControllerState &c,XrTime){check(!c.select && !c.back);}
static void placePanel(XrHello &x,const XrView *views){check(!x.splitVisible && !x.stereoVisible && !x.arranging && x.loadingPresentation);check(views[0].pose.position.x==-.03f);}
static int referenceUpdates=0;
static void applyWorkspaceReference(XrHello &x,const XrView *views,XrTime time){check(time==predicted);++referenceUpdates;placePanel(x,views);}
static bool renderEye(XrHello &,int eye,const XrPosef &pose,const XrFovf &){++draws;check(pose.position.x==(eye ? .03f:-.03f));return drawOK;}
static void d3d8gles_InvalidateCachedState(){++invalidations;}
static unsigned gameTexture=1,recoveries=0;
static unsigned XrGameBoot_GameTexture(){return gameTexture;}
static void d3d8gles_RequireXRFullWorld(){++recoveries;}
static void updateMenuTextures(XrHello &x,XrTime){check(x.recoveryVisible);}
XRAPI_ATTR XrResult XRAPI_CALL xrWaitFrame(XrSession,const XrFrameWaitInfo *,XrFrameState *state){
	check(!begun);++waits;if(!waitOK)return XR_ERROR_RUNTIME_FAILURE;
	state->predictedDisplayTime=++predicted;state->shouldRender=render;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrBeginFrame(XrSession,const XrFrameBeginInfo *){check(!begun);begun=true;++begins;return XR_SUCCESS;}
XRAPI_ATTR XrResult XRAPI_CALL xrLocateViews(XrSession,const XrViewLocateInfo *info,XrViewState *state,uint32_t,uint32_t *count,XrView *views){
	check(begun && info->displayTime==predicted);*count=2;
	state->viewStateFlags=valid ? XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT |
		XR_VIEW_STATE_POSITION_TRACKED_BIT | XR_VIEW_STATE_ORIENTATION_TRACKED_BIT:0;
	for(int eye=0;eye<2;++eye){views[eye].pose.orientation.w=1;views[eye].pose.position.x=eye ? .03f:-.03f;}
	return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrEndFrame(XrSession,const XrFrameEndInfo *end){
	check(begun);begun=false;++ends;check(end->environmentBlendMode==XR_ENVIRONMENT_BLEND_MODE_OPAQUE);
	if(end->displayTime==42)check(end->layerCount==0);
	else {
		check(end->displayTime==predicted);
		check(end->layerCount==(render ? (valid && drawOK ? 2u:1u):0u));
		if(end->layerCount==2){
			check(end->layers[0]->type==XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB);
			const auto *layer=reinterpret_cast<const XrCompositionLayerProjection *>(end->layers[1]);
			check(layer->viewCount==2 && layer->views[0].pose.position.x==-.03f);
			check(layer->views[1].subImage.imageRect.extent.width==1000);
		}
	}
	return endOK ? XR_SUCCESS:XR_ERROR_RUNTIME_FAILURE;
}
#include "xr-loading-presenter.inc"
int main(){
	XrHello x;bool quit=false;
	check(x.observer.arm(true));
	{
		XrLoadingPresenter presenter(x,42,quit);check(callback && !presenter.frame.consumed);
		check(x.observer.mode==XrObserverMode::Armed && x.renderedObserver);
		controller.select=true;callback(callbackContext);check(!keys.back());
		check(x.observer.mode==XrObserverMode::Off && !x.renderedObserver);
		check(ends==2 && begins==1 && draws==2 && !begun && !quit);
		controller.select=false;callback(callbackContext);controller.select=true;callback(callbackContext);
		check(keys.back());controller.select=false;
		const auto oldDraws=draws;render=false;callback(callbackContext);check(draws==oldDraws);
		render=true;valid=false;callback(callbackContext);check(draws==oldDraws);
		valid=true;x.sessionRunning=false;const auto oldWaits=waits;callback(callbackContext);
		check(waits==oldWaits && keys.back());x.sessionRunning=true;
		callback(callbackContext);check(draws==oldDraws+2 && ends==begins+1);
		check(invalidations==4 && !x.controlsArmed && !x.inputArmed);
		gameTexture=0;callback(callbackContext);check(x.recoveryVisible && recoveries==1);
		gameTexture=1;callback(callbackContext);check(!x.recoveryVisible);
	}
	check(!callback && !callbackContext && !keys.back() && !x.loadingPresentation);
	// Normal game frame leaves outer frame ownership completely untouched.
	check(x.observer.arm(true));x.renderedObserver=true;
	begun=true;const auto oldEnds=ends;{XrLoadingPresenter unused(x,42,quit);}
	check(begun && ends==oldEnds);
	check(x.observer.mode==XrObserverMode::Armed && x.renderedObserver);
	// A failed eye still balances the frame and leaves via the movie abort path.
	drawOK=false;{XrLoadingPresenter broken(x,42,quit);callback(callbackContext);check(quit && broken.frame.failed && !begun);}
	check(!keys.back() && !callback);
	printf("PASS %d production XR loading presenter checks\n",checks);
}
