// GeneralsX @test Codex 17/09/2026 Production P25 state, mapping and guards.
#include "XrWorld.h"
#include "XrHandedness.h"
#include "XRStereoShader.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
static int checks=0;
static void check(bool value) {++checks;if(!value){fprintf(stderr,"observer check %d failed\n",checks);exit(1);}}
static void near(float a,float b) {check(std::isfinite(a)&&fabsf(a-b)<.0001f);}
int main(int argc,char **argv) {
	check(argc==3);std::ifstream source(argv[1]);check(source.good());
	const std::string code((std::istreambuf_iterator<char>(source)),std::istreambuf_iterator<char>());
	std::ifstream bootSource(argv[2]);check(bootSource.good());
	const std::string boot((std::istreambuf_iterator<char>(bootSource)),std::istreambuf_iterator<char>());
	// The same central gate controls the button, placement, active rendering and
	// walking. Campaign entry must retain the shared cinematic/camera guard.
	const auto gate=boot.find("bool XrGameBoot_CanObserveGround()");
	const auto gateEnd=boot.find("bool XrGameBoot_ViewBase()",gate);
	check(gate!=std::string::npos && gateEnd!=std::string::npos);
	const auto gateBody=boot.substr(gate,gateEnd-gate);
	check(gateBody.find("GAME_SKIRMISH")!=std::string::npos);
	check(gateBody.find("GAME_SINGLE_PLAYER")!=std::string::npos);
	check(gateBody.find("XrGameBoot_CanAdjustWorld()")!=std::string::npos);
	check(code.find("!XrGameBoot_CanObserveGround() || x.roomPoseLost || x.resultVisible")!=std::string::npos);
	check(boot.find("TheDisplay->isMoviePlaying()")!=std::string::npos);
	check(boot.find("TheTacticalView->isCameraMovementFinished()")!=std::string::npos);
	// Production-order regression: registering the loading callback is done
	// around every ordinary frame; only an invoked present() may cancel mode.
	const auto constructor=code.find("XrLoadingPresenter(XrHello &host");
	const auto destructor=code.find("~XrLoadingPresenter()",constructor);
	const auto presenter=code.find("void present()",destructor);
	const auto next=code.find("static void runLoop",presenter);
	check(constructor!=std::string::npos && destructor!=std::string::npos && presenter!=std::string::npos && next!=std::string::npos);
	check(code.substr(constructor,destructor-constructor).find("observer.cancel()") ==std::string::npos);
	check(code.substr(presenter,next-presenter).find("observer.cancel()") !=std::string::npos);
	check(code.find("x.renderedObserver=world.observer")!=std::string::npos);
	check(code.find("layerBase=x.passthroughActive && !x.renderedObserver")!=std::string::npos);
	check(std::string(GX_XR_STEREO_FRAGMENT_BODY).find("uXrAspect>0.0")!=std::string::npos);
	XrObserverState state;
	check(!state.arm(false));check(state.arm(true));check(state.mode==XrObserverMode::Armed);
	check(!state.canChoose(true));state.neutral(true);check(state.canChoose(true));
	check(!state.canChoose(true));state.neutral(true);
	const XrVector3f ground={1000,700,20},head={.2f,1.5f,-.3f};
	check(state.choose(ground,head,{0,0,-1}));
	check(state.mode==XrObserverMode::Active && state.requireRelease);
	check(xrLength(state.walkDelta({0,1},{0,0,-1},.05f))==0);
	XrObserverState moving=state;moving.neutral(true);
	const XrVector3f tracked={head.x+.25f,head.y,head.z-.15f};
	float preTurn[16];check(xrObserverWorldToRoom(preTurn,moving.ground,moving.head,moving.forward));
	const auto pivot=xrInversePoint(preTurn,tracked);
	moving.turn(1,.05f,tracked);
	float postTurn[16];check(xrObserverWorldToRoom(postTurn,moving.ground,moving.head,moving.forward));
	const auto pivotAfter=xrInversePoint(postTurn,tracked);
	near(pivot.x,pivotAfter.x);near(pivot.y,pivotAfter.y);near(pivot.z,pivotAfter.z);
	check(moving.forward.x<0); // Right turn rotates the world to the left.
	const auto forwardStep=moving.walkDelta({0,1},{0,0,-1},.05f);
	check(forwardStep.y>.9f && xrLength(forwardStep)<1.01f);
	const auto strafe=moving.walkDelta({1,0},{0,0,-1},.05f);
	check(strafe.x>.9f && xrLength(strafe)<1.01f);
	const auto diagonal=moving.walkDelta({1,1},{0,0,-1},.05f);
	check(xrLength(diagonal)<=1.01f);
	XrPhysicalHand physical[2];physical[0].stick={.4f,.8f};physical[1].stick={-.6f,.3f};
	for(bool leftHanded:{false,true}) {
		const auto mapped=xrMapHands(physical,leftHanded,false);
		near(mapped.leftStick.x,.4f);near(mapped.leftStick.y,.8f);
		near(mapped.rightStick.x,-.6f);near(mapped.rightStick.y,.3f);
	}
	float m[16];check(xrObserverWorldToRoom(m,state.ground,state.head,state.forward));
	const auto floor=xrTransformPoint(m,ground);
	near(floor.x,head.x);near(floor.y,head.y-kXrObserverEyeHeightMetres);near(floor.z,head.z);
	const auto right=xrTransformPoint(m,xrAdd(ground,{10,0,0}));near(right.x-head.x,1);near(right.z-head.z,0);
	const auto forward=xrTransformPoint(m,xrAdd(ground,{0,10,0}));near(forward.z-head.z,-1);
	const auto up=xrTransformPoint(m,xrAdd(ground,{0,0,10}));near(up.y-floor.y,1);
	const auto back=xrInversePoint(m,head);near(back.x,ground.x);near(back.y,ground.y);near(back.z,ground.z+16.5f);
	check(xrObserverContainsSphere(back,ground,2));
	check(!xrObserverContainsSphere(back,xrAdd(ground,{900,0,0}),1));
	check(xrObserverValidGround(ground,{0,0,0},{2000,1500,100},true,false));
	check(!xrObserverValidGround(ground,{990,0,0},{2000,1500,100},true,false));
	check(!xrObserverValidGround(ground,{0,0,0},{2000,1500,100},false,false));
	check(!xrObserverValidGround(ground,{0,0,0},{2000,1500,100},true,true));
	XrWorldFrame frame;frame.observer=true;frame.observerGround=ground;frame.observerHead=head;
	frame.eyes[0]={{0,0,0,1},head};frame.eyes[1]={{0,0,0,1},{head.x+.064f,head.y,head.z}};
	for(int eye=0;eye<2;++eye)frame.fov[eye]={-.8f,.8f,.8f,-.8f};
	float clip[16];xrWorldEyeClip(clip,frame,0,m);
	const auto ahead=xrAdd(ground,{0,30,16.5f});
	const float w=clip[3]*ahead.x+clip[7]*ahead.y+clip[11]*ahead.z+clip[15];
	check(std::isfinite(w) && w>0);
	state.cancel();check(state.mode==XrObserverMode::Off && state.requireRelease);
	check(!state.arm(false));check(state.arm(true));check(!state.canChoose(true));
	printf("PASS %d P25 observer state/mapping checks\n",checks);
}
