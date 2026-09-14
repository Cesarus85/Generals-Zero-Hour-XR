// GeneralsX @test Codex 14/09/2026 Real engine adapter with read-only view spies.
#include "XrWorld.h"
#include <cstdio>
#include <cstdlib>
using Coord3D=XrVector3f;
struct Region3D{Coord3D lo,hi;};
constexpr float MAP_HEIGHT_SCALE=.625f,ViewDefaultPitchRadians=37.5f*3.141592654f/180;
static XrWorldFrame s_worldFrame;
static float s_worldMapping[16]={},s_worldAspect=0,s_worldSpan=0,s_worldMaxHeight=0;
static bool s_mappingReady=false,active=true,split=true;
static bool XrGameBoot_CanStereoWorld(){return active;}
static bool GX_XR_SplitUIAllowed(){return split;}
struct View {
 Coord3D position={1000,700,45};float height=500,angle=0;int w=1280,h=576;
 int getWidth(){return w;}int getHeight(){return h;}
 const Coord3D &getPosition(){return position;}
 float getAngle(){return angle;}float getHeightAboveGround(){return height;}
 float getFieldOfView(){return 50*3.141592654f/180;}
} view;
struct Terrain {
 Region3D extent={{0,0,15.625f},{2000,2000,159.375f}};
 void getExtent(Region3D *out){*out=extent;}
} terrain;
static View *TheTacticalView=&view;static Terrain *TheTerrainLogic=&terrain;
#include "height-bridge.inc"
static unsigned checks=0;
static void check(bool b){++checks;if(!b){fprintf(stderr,"height bridge check %u failed\n",checks);exit(1);}}
int main(){
 s_worldFrame.enabled=true;check(xrPrepareWorldMapping() && s_mappingReady);
 const float scale=s_worldMapping[10],z=s_worldMapping[14],span=s_worldSpan;
 for(float ground:{0.0f,49.6f,159.375f}) {
  view.position={1000+ground,700-ground,ground};view.angle=ground*.01f;
  check(xrPrepareWorldMapping());check(s_worldMapping[10]==scale && s_worldMapping[14]==z);
  check(s_worldSpan==span && s_worldMaxHeight==159.375f);
 }
 // Cinematic suppression cannot expose stale picks or change the map datum.
 split=false;check(!xrPrepareWorldMapping() && !s_mappingReady);
 split=true;check(xrPrepareWorldMapping() && s_worldMapping[14]==z);
 // A new/load map's native extent replaces the old datum immediately, even
 // when all engine pointers and the saved game frame number are reused.
 terrain.extent.lo.z=80;check(xrPrepareWorldMapping());
 check(fabsf(xrInversePoint(s_worldMapping,{}).z-80)<.001f);
 view.height=250;check(xrPrepareWorldMapping());check(fabsf(s_worldSpan*2-span)<.001f);
 s_worldFrame.coverage=2;check(xrPrepareWorldMapping());check(s_worldSpan==span);
 view.w=0;check(!xrPrepareWorldMapping() && !s_mappingReady);view.w=1280;
 terrain.extent.lo.z=200;check(!xrPrepareWorldMapping() && !s_mappingReady);terrain.extent.lo.z=0;
 TheTerrainLogic=nullptr;check(!xrPrepareWorldMapping() && !s_mappingReady);TheTerrainLogic=&terrain;
 active=false;check(!xrPrepareWorldMapping());active=true;
 s_worldFrame.enabled=false;check(!xrPrepareWorldMapping());
 printf("PASS %u production stable-mapping bridge checks\n",checks);
}
