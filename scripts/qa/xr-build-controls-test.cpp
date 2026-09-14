// GeneralsX @test Codex 14/09/2026 Extracted production preview adapter.
#include "XrBuildRotation.h"
#include "XrMenu.h"
#include "XrHandedness.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
bool GX_XR_OffscreenBoot=true;
struct Ghost {float angle=0;int writes=0;float getOrientation(){return angle;}
 void setOrientation(float a){angle=a;++writes;}};
struct Assistant {bool line=false;bool isLineBuildTemplate(const void *){return line;}} assistant;
static Assistant *TheBuildAssistant=&assistant;
struct W3DInGameUI {
 const void *m_pendingPlaceType=this;Ghost ghost;Ghost *m_placeIcon[1]={&ghost};bool anchored=false;
 bool isPlacementAnchored(){return anchored;}
 float getPlacementAngle(){return m_placeIcon[0]->getOrientation();}
 bool rotateXrPlacement(float radians);
} ui;
static W3DInGameUI *TheInGameUI=&ui;
static bool s_spatialActive=true,adjustable=true;
static bool XrGameBoot_CanAdjustWorld(){return adjustable;}
#include "xr-build-controls.inc"
static int checks=0;
static void check(bool v){++checks;if(!v){fprintf(stderr,"build controls check %d failed\n",checks);exit(1);}}
static void near(float a,float b){check(fabsf(a-b)<.0001f);}
int main(){
 check(XrGameBoot_CanRotatePlacement());check(ui.ghost.writes==0);
 check(XrGameBoot_RotatePlacement(1.570796327f));near(XrGameBoot_PlacementDegrees(),90);
 check(XrGameBoot_RotatePlacement(-3.141592654f));near(XrGameBoot_PlacementDegrees(),270);
 for(int i=0;i<2000;++i){check(XrGameBoot_RotatePlacement(.04f));check(fabsf(ui.ghost.angle)<=3.141593f);}
 const float prior=ui.ghost.angle;const int writes=ui.ghost.writes;
 for(int reason=0;reason<8;++reason){
  adjustable=reason!=0;s_spatialActive=reason!=1;GX_XR_OffscreenBoot=reason!=2;
  ui.m_pendingPlaceType=reason==3 ? nullptr:&ui;ui.m_placeIcon[0]=reason==4 ? nullptr:&ui.ghost;
  ui.anchored=reason==5;assistant.line=reason==6;TheBuildAssistant=reason==7 ? nullptr:&assistant;
  check(!XrGameBoot_RotatePlacement(.1f));near(ui.ghost.angle,prior);check(ui.ghost.writes==writes);
 }
 adjustable=true;s_spatialActive=true;GX_XR_OffscreenBoot=true;ui.m_pendingPlaceType=&ui;
 ui.m_placeIcon[0]=&ui.ghost;ui.anchored=false;assistant.line=false;TheBuildAssistant=&assistant;
 check(!XrGameBoot_RotatePlacement(std::numeric_limits<float>::quiet_NaN()));
 check(!XrGameBoot_RotatePlacement(std::numeric_limits<float>::infinity()));
 TheInGameUI=nullptr;check(!XrGameBoot_CanRotatePlacement());TheInGameUI=&ui;
 Ghost fresh;fresh.angle=.25f;ui.m_placeIcon[0]=&fresh;
 check(XrGameBoot_RotatePlacement(.1f));near(fresh.angle,.35f);near(ui.ghost.angle,prior);
 // Hand inversion is the real physical-to-logical mapping, not duplicated labels.
 for(bool left:{false,true}){
  XrPhysicalHand hands[2];hands[left ? 1:0].grip=true;hands[left ? 0:1].stick={1,0};
  const auto c=xrMapHands(hands,left,false);check(c.grip[0] && !c.secondary);
  XrBuildRotation rotation;near(rotation.update(true,c.grip[0],c.zoom,false,.02f),-.031415927f);
  near(rotation.update(true,true,{1,0},true,.02f),0);
  near(rotation.update(true,true,{1,0},false,.02f),0);
  check(rotation.update(true,true,{1,0},false,.02f)<0);
  near(rotation.update(true,false,{1,0},false,.02f),0);check(rotation.captured);
  rotation.update(false,false,{0,0},false,.02f);check(!rotation.captured);
  for(auto language:{XrLanguage::German,XrLanguage::English}){
   g_xrLanguage=language;
   for(int page=0;page<kXrControllerHelpPages;++page)check(xrControllerHelp(page,left).find('{')==std::string::npos);
   const auto help=xrControllerHelp(0,left);
   check(help.find(left ? "X:":"A:")!=std::string::npos);
   check(help.find(left ? "Y ":"B ")!=std::string::npos);
  }
 }
 // Render outline follows the physical plinth and current cropped UI exactly.
 for(float width:{.45f,1.65f,4.0f})for(float aspect:{.16f,.5625f,1.0f})for(float tilt:{0.0f,-.7f,-1.5707963f}) {
  XrSurface s;s.width=width;s.pose={xrAxisAngle({1,0,0},tilt),{.2f,-.3f,-1}};
  float a=aspect;const auto board=xrEditOutline(s,a,true);
  near(board.width,width*1.024f);near(board.width*a,width*(aspect+.024f));
  const auto local=xrRotate(xrPoseInverse(s.pose).orientation,xrSub(board.pose.position,s.pose.position));
  near(local.x,0);near(local.y,0);near(local.z,-.017f*width);
  a=aspect;const auto panel=xrEditOutline(s,a,false);near(panel.width,width);near(a,aspect);
  near(xrLength(xrSub(panel.pose.position,s.pose.position)),.003f);
 }
 printf("PASS %d production building rotation / hand / outline checks\n",checks);
}
