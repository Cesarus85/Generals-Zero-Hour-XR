// GeneralsX @test Codex 14/09/2026 Production scene geometry + async ownership.
#include "XrScene.h"
#include "XrReferenceSpace.h"
#include <cstdio>
#include <cstdlib>
static int checks=0,destroyed=0,enables=0,queries=0;
static bool failQuery=false,locatable=true,tracked=true;
static XrPosef located={{0,0,0,1},{0,0,0}};
static void check(bool yes){++checks;if(!yes){fprintf(stderr,"scene check %d failed\n",checks);exit(1);}}
extern "C" XrResult xrDestroySpace(XrSpace){++destroyed;return XR_SUCCESS;}
extern "C" XrResult xrGetInstanceProcAddr(XrInstance,const char *,PFN_xrVoidFunction *f){*f=nullptr;return XR_ERROR_FUNCTION_UNSUPPORTED;}
extern "C" XrResult xrLocateSpace(XrSpace,XrSpace,XrTime,XrSpaceLocation *l) {
	l->pose=located;l->locationFlags=tracked ? XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|
		XR_SPACE_LOCATION_POSITION_TRACKED_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT:0;return XR_SUCCESS;
}
static XrResult query(XrSession,const XrSpaceQueryInfoBaseHeaderFB *base,XrAsyncRequestIdFB *id) {
	const auto &q=*reinterpret_cast<const XrSpaceQueryInfoFB *>(base);
	check(q.maxResultCount==256 && q.timeout==5000000000 && q.queryAction==XR_SPACE_QUERY_ACTION_LOAD_FB);
	check(reinterpret_cast<const XrSpaceComponentFilterInfoFB *>(q.filter)->componentType==XR_SPACE_COMPONENT_TYPE_SEMANTIC_LABELS_FB);
	const auto *storage=reinterpret_cast<const XrSpaceStorageLocationFilterInfoFB *>(q.filter->next);
	check(storage && storage->type==XR_TYPE_SPACE_STORAGE_LOCATION_FILTER_INFO_FB && storage->location==XR_SPACE_STORAGE_LOCATION_LOCAL_FB);
	++queries;*id=42;return failQuery ? XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;
}
static XrResult status(XrSpace,XrSpaceComponentTypeFB,XrSpaceComponentStatusFB *s){s->enabled=locatable;s->changePending=false;return XR_SUCCESS;}
static XrResult enable(XrSpace,const XrSpaceComponentStatusSetInfoFB *,XrAsyncRequestIdFB *id){++enables;*id=99;return XR_SUCCESS;}
static XrResult labels(XrSession,XrSpace,XrSemanticLabelsFB *l){strcpy(l->buffer,"TABLE");return XR_SUCCESS;}
static XrResult box(XrSession,XrSpace,XrRect3DfFB *b){*b={{-2,-.5f,-2},{4,.5f,4}};return XR_SUCCESS;}
static XrResult planeBox(XrSession,XrSpace,XrRect2Df *b){*b={{-1,-1},{2,2}};return XR_SUCCESS;}
static XrResult noBoundary(XrSession,XrSpace,XrBoundary2DFB *b){b->vertexCountOutput=0;return XR_SUCCESS;}
static XrResult retrieve(XrSession,XrAsyncRequestIdFB id,XrSpaceQueryResultsFB *r) {
	check(id==42);r->resultCountOutput=1;
	if(r->resultCapacityInput)r->results[0].space=reinterpret_cast<XrSpace>(uintptr_t(100));return XR_SUCCESS;
}
template<class T> static void event(XrScene &s,T e){XrEventDataBuffer b={};memcpy(&b,&e,sizeof(e));s.event(XR_NULL_HANDLE,b);}
#include "XrLayout.h"
#include "XrCommands.h"
#include "XrBuildRotation.h"
using GLuint=unsigned;
struct XrHello {
	XrScene scene;XrMenuState menu;XrCommandState commands;XrBuildRotation buildRotation;
	XrSurface surfaces[3];XrLayout layout;XrSurfaceGrab grab;
	XrSession session=XR_NULL_HANDLE;XrSpace localSpace=XR_NULL_HANDLE;
	XrSessionState state=XR_SESSION_STATE_FOCUSED;
	XrPosef layoutAnchor={{0,0,0,1},{0,0,0}};
	bool roomPoseLost=false,loadingPresentation=false,interactiveGame=true;
	bool splitVisible=true,arranging=false,controlsArmed=true,inputArmed=true,layoutDirty=false;
	bool hoverVisible=false,pointerVisible=false,worldCursorVisible=false,recoveryVisible=false;
	bool rayVisible=false,rayHit=false,pointerPressed=false;XrVector3f rayStart={},rayEnd={};
	XrTime recenterTime=0;GLuint sceneTexture=0;std::string sceneKey;
};
struct XrControllerState {
	XrPosef aim={{0,0,0,1},{0,0,0}};
	bool aimValid=true,select=false,back=false,grip[2]={},buttonsHeld=false,secondary=false;
	XrVector2f pan={},zoom={};
};
static int permission=1,releases=0,saves=0,cancels=0;static bool locked=false,expanded=false;
static int scenePermission(XrHello &,bool){return permission;}
static bool XrGameBoot_CanAdjustWorld(){return !locked;}
static bool XrGameBoot_ExpandedUI(){return expanded;}
static void XrGameBoot_CancelTarget(){++cancels;}
static void updateControls(XrHello &,const XrControllerState &c,XrTime){check(!c.select && !c.back && c.pan.x==0 && c.zoom.y==0);++releases;}
static float surfaceAspect(int){return .7f;}
static void saveLayout(XrHello &){++saves;}
static bool paintPanel(XrHello &,GLuint &,const std::string &,const std::string &,const std::string &,int,int){return true;}
#define XR_LOG(...) ((void)0)
#include "xr-scene-ui.inc"
int main(int argc,char **argv) {
	XrSceneFace floor;floor.pose.orientation=xrAxisAngle({1,0,0},-1.5707963268f);
	xrSceneRectangle(floor,-3,-3,6,6);
	XrPosef ray={xrAxisAngle({1,0,0},-1.5707963268f),{0,1.5f,0}};
	XrVector3f hit;float distance=0;
	check(xrSceneHit(floor,ray,hit,distance) && fabsf(distance-1.5f)<.0001f);
	check(!xrSceneInside(floor.boundary,{4,0}));
	XrSurface old;old.width=1.2f;old.pose.orientation=xrAxisAngle({0,1,0},.7f);
	auto board=xrSceneBoard(old,hit,old.width);
	check(fabsf(board.pose.position.y-.0452f)<.0001f);
	check(xrRotate(board.pose.orientation,{0,0,1}).y>.999f);
	check(xrSceneFits(floor,board,.7f));
	auto slope=floor;slope.pose.orientation=xrMul(xrAxisAngle({0,0,1},.05f),floor.pose.orientation);
	const auto supported=xrSceneSupportedBoard(slope,old,{0,0,0},old.width,.7f);
	const auto normal=xrRotate(slope.pose.orientation,{0,0,1});
	for(int x:{-1,1})for(int y:{-1,1}) {
		const auto corner=xrAdd(supported.pose.position,xrRotate(supported.pose.orientation,{x*.512f*old.width,y*.362f*old.width,-.036f*old.width}));
		check(xrDot(normal,corner)>.001f);
	}
	auto beyond=board;beyond.pose.position.x=3;check(!xrSceneFits(floor,beyond,.7f));
	ray.position.x=4;check(!xrSceneHit(floor,ray,hit,distance));ray.position.x=0;
	auto wall=floor;wall.pose.orientation={0,0,0,1};check(!xrSceneHit(wall,ray,hit,distance));
	ray.position.y=-1;check(!xrSceneHit(floor,ray,hit,distance));ray.position.y=1.5f;
	// Top faces for all six choices of local vertical axis and arbitrary yaw.
	for(int axis=0;axis<3;++axis)for(int sign:{-1,1})for(int degree=0;degree<360;degree+=7) {
		const XrVector3f axes[]={{1,0,0},{0,1,0},{0,0,1}};
		const auto rotation=xrMul(xrAxisAngle({0,1,0},degree*.01745329252f),xrFromTo(xrScale(axes[axis],float(sign)),{0,1,0}));
		XrSceneFace face;XrRect3DfFB box={{-.5f,-.5f,-.5f},{1,1,1}};
		check(xrSceneTop({rotation,{0,.25f,0}},box,face));
		check(fabsf(face.pose.position.y-.75f)<.0001f && xrRotate(face.pose.orientation,{0,0,1}).y>.999f);
		check(xrSceneHit(face,ray,hit,distance) && fabsf(hit.y-.75f)<.0001f);
	}
	XrSurface surfaces[3];surfaces[1]=old;surfaces[2].pose={xrAxisAngle({1,0,0},-.25f),{.5f,.5f,-1}};
	const auto before=surfaces[2];xrSceneMoveWorkspace(surfaces,board);
	check(fabsf(xrLength(xrSub(surfaces[2].pose.position,board.pose.position))-xrLength(xrSub(before.pose.position,old.pose.position)))<.0001f);
	check(fabsf(surfaces[2].pose.orientation.x-before.pose.orientation.x)<.0001f && surfaces[2].width==before.width);
	XrScene scene;scene.fit=false;scene.available=true;scene.query=query;scene.retrieve=retrieve;scene.labels=labels;scene.box3=box;scene.status=status;scene.enable=enable;
	scene.refresh(XR_NULL_HANDLE);check(scene.querying && queries==1);
	scene.refresh(XR_NULL_HANDLE);check(queries==1);
	event(scene,XrEventDataSpaceQueryResultsAvailableFB{XR_TYPE_EVENT_DATA_SPACE_QUERY_RESULTS_AVAILABLE_FB,nullptr,41});check(scene.entries.empty());
	event(scene,XrEventDataSpaceQueryResultsAvailableFB{XR_TYPE_EVENT_DATA_SPACE_QUERY_RESULTS_AVAILABLE_FB,nullptr,42});check(scene.entries.size()==1);
	event(scene,XrEventDataSpaceQueryResultsAvailableFB{XR_TYPE_EVENT_DATA_SPACE_QUERY_RESULTS_AVAILABLE_FB,nullptr,42});check(scene.entries.size()==1 && destroyed==0);
	event(scene,XrEventDataSpaceQueryCompleteFB{XR_TYPE_EVENT_DATA_SPACE_QUERY_COMPLETE_FB,nullptr,42,XR_SUCCESS});
	check(!scene.querying && scene.message=="Raumdaten bereit");
	scene.placing=true;scene.cancel();check(!scene.placing && scene.entries.size()==1); // cancellation does not mutate room data
	scene.aim(XR_NULL_HANDLE,XR_NULL_HANDLE,100,ray,board,.7f);check(scene.preview);
	tracked=false;scene.aim(XR_NULL_HANDLE,XR_NULL_HANDLE,101,ray,board,.7f);check(!scene.preview);tracked=true;
	locatable=false;scene.aim(XR_NULL_HANDLE,XR_NULL_HANDLE,102,ray,board,.7f);
	scene.aim(XR_NULL_HANDLE,XR_NULL_HANDLE,103,ray,board,.7f);check(!scene.preview && enables==1);locatable=true;
	scene.filter=1;scene.aim(XR_NULL_HANDLE,XR_NULL_HANDLE,104,ray,board,.7f);check(!scene.preview);
	scene.filter=0;scene.entries[0].box={{-.4f,-.5f,-.4f},{.8f,.5f,.8f}};
	scene.aim(XR_NULL_HANDLE,XR_NULL_HANDLE,105,ray,board,.7f);check(!scene.preview && scene.detected && !scene.visibleFaces.empty());
	scene.fit=true;scene.aim(XR_NULL_HANDLE,XR_NULL_HANDLE,106,ray,board,.7f);check(scene.preview && scene.candidate.width<.8f && scene.candidate.width>=.45f);
	scene.refresh(XR_NULL_HANDLE);check(destroyed==1 && scene.querying);
	event(scene,XrEventDataSpaceQueryCompleteFB{XR_TYPE_EVENT_DATA_SPACE_QUERY_COMPLETE_FB,nullptr,42,XR_ERROR_RUNTIME_FAILURE});
	check(!scene.querying && scene.entries.empty());
	failQuery=true;scene.refresh(XR_NULL_HANDLE);check(!scene.querying);
	event(scene,XrEventDataSpaceQueryResultsAvailableFB{XR_TYPE_EVENT_DATA_SPACE_QUERY_RESULTS_AVAILABLE_FB,nullptr,42});check(scene.entries.empty());
	scene.init(XR_NULL_HANDLE,false,false);check(scene.message=="Raumflächen nicht unterstützt");
	scene.clear();check(destroyed==1);

 // Rebase invariance: oldFromNew * new pose must reproduce the old world pose.
 {
  XrReferenceChanges startup;
  XrEventDataReferenceSpaceChangePending e={XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING};
  e.changeTime=10;e.poseValid=XR_FALSE;startup.enqueue(e);
  e.changeTime=30;startup.enqueue(e);startup.adoptCurrentOrigin(20);
  check(startup.pending.size()==1 && startup.pending[0].changeTime==30);
 }
 for(int degrees=0;degrees<360;degrees+=11) {
  XrReferenceChanges changes;XrSurface poses[3],menuSurface;XrPosef anchor={{0,0,0,1},{0,1.6f,0}};
  for(int i=0;i<3;++i){poses[i]=board;poses[i].pose.position.x=float(i)*.4f;}
  const auto oldPose=poses[1].pose,oldAnchor=anchor;
  const XrPosef oldFromNew={xrAxisAngle({0,1,0},degrees*.01745329252f),{.3f,.7f,-.8f}};
  XrEventDataReferenceSpaceChangePending e={XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING};
  e.changeTime=200;e.poseValid=XR_TRUE;e.poseInPreviousSpace=oldFromNew;
  changes.enqueue(e);
  check(changes.apply(199,poses,anchor,menuSurface)==0);
  check(changes.apply(200,poses,anchor,menuSurface)==1);
  const auto restored=xrPoseMul(oldFromNew,poses[1].pose);
  check(xrLength(xrSub(restored.position,oldPose.position))<.00001f);
  check(xrLength(xrSub(xrPoseMul(oldFromNew,anchor).position,oldAnchor.position))<.00001f);
  check(changes.apply(201,poses,anchor,menuSurface)==0);
  e.changeTime=300;e.poseValid=XR_FALSE;changes.enqueue(e);
  const auto beforeInvalid=poses[1].pose;
  check(changes.apply(300,poses,anchor,menuSurface)==2);
  check(xrLength(xrSub(beforeInvalid.position,poses[1].pose.position))==0);
 }
 // Real wizard routing, including main-menu preview and no automatic scanning.
 XrHello x;x.scene=scene;x.scene.filter=0;x.scene.fit=false;x.menu.page=5;
 XrScene::Entry entry;entry.space=reinterpret_cast<XrSpace>(uintptr_t(100));entry.volume=true;
 entry.box={{-2,-.5f,-2},{4,.5f,4}};x.scene.entries={entry};x.surfaces[1]=board;
 XrControllerState c;c.aim=ray;
 const int beforeQueries=queries;
 check(xrSceneMenuAction(x,5) && !x.scene.placing && queries==beforeQueries); // blank, inert
 check(xrSceneMenuAction(x,0) && x.scene.step==XrScene::Step::Done && queries==beforeQueries);
 xrSceneMenuAction(x,1);check(x.scene.step==XrScene::Step::Choice);
 x.scene.step=XrScene::Step::Surfaces;
 locked=true;xrSceneMenuAction(x,0);check(!x.scene.placing);locked=false;
 x.interactiveGame=false;x.splitVisible=false;expanded=true;
 check(xrSceneMenuAction(x,0) && x.scene.placing && !x.menu.open && !x.controlsArmed && cancels==1);
 c.select=true;updateScenePlacement(x,c,100);check(saves==0 && !x.scene.armed);
 c.select=false;updateScenePlacement(x,c,101);check(x.scene.armed && x.scene.preview);
 c.select=true;updateScenePlacement(x,c,102);check(saves==1 && !x.scene.placing && x.menu.open);
 check(!updateScenePlacement(x,c,103) && saves==1);
 check(x.scene.step==XrScene::Step::Done);
 // Every exit consumes clicks; shell expansion is permitted, live dialogs are not.
 x.scene.step=XrScene::Step::Surfaces;
 c={};c.aim=ray;xrSceneMenuAction(x,0);updateScenePlacement(x,c,104);
 c.zoom={1,1};const auto unchanged=x.surfaces[1];
 updateScenePlacement(x,c,50000000);check(x.scene.workingBoard.width>unchanged.width);
 c.grip[0]=true;check(updateScenePlacement(x,c,50000001) && !x.scene.placing && saves==1);
 check(x.surfaces[1].width==unchanged.width && x.surfaces[1].pose.position.y==unchanged.pose.position.y);
 c={};c.aim=ray;xrSceneMenuAction(x,0);updateScenePlacement(x,c,106);
 c.aimValid=false;updateScenePlacement(x,c,107);check(!x.scene.placing && saves==1);
 c.aimValid=true;xrSceneMenuAction(x,0);updateScenePlacement(x,c,108);
 x.state=XR_SESSION_STATE_VISIBLE;updateScenePlacement(x,c,109);check(!x.scene.placing);
 x.state=XR_SESSION_STATE_FOCUSED;x.interactiveGame=true;x.splitVisible=true;expanded=false;
 xrSceneMenuAction(x,0);expanded=true;updateScenePlacement(x,c,110);check(!x.scene.placing);expanded=false;
 // Manual height needs two distinct fresh presses; no room query or actual edit before confirm.
 x.scene.step=XrScene::Step::Choice;xrSceneMenuAction(x,2);
 check(x.scene.placing && x.scene.manual && !x.scene.heightKnown);
 c={};c.aim=ray;updateScenePlacement(x,c,120);
 c.select=true;updateScenePlacement(x,c,121);check(x.scene.heightKnown && !x.scene.armed && saves==1);
 c.aim.position.y+=.7f;updateScenePlacement(x,c,122);check(saves==1);
 c.select=false;updateScenePlacement(x,c,123);check(x.scene.preview);
 check(fabsf(x.scene.manualFace.pose.position.y-1.5f)<.0001f);
 c.select=true;updateScenePlacement(x,c,124);check(saves==2 && !x.scene.placing);
 check(fabsf(x.surfaces[1].pose.position.y-(1.5f+.036f*x.surfaces[1].width+.002f))<.0001f);
 // No matching table has explicit feedback, not a silent preview miss.
 x.scene.step=XrScene::Step::Surfaces;x.scene.entries[0].floor=true;
 xrSceneMenuAction(x,0);check(!x.scene.placing && x.scene.message.find("Kein Tisch")!=std::string::npos);
 x.scene.entries[0].floor=false;
 permission=0;xrSceneMenuAction(x,2);check(!x.scene.permissionPending && !x.scene.querying);
 permission=2;xrSceneMenuAction(x,2);check(x.scene.permissionPending);
 permission=0;check(!updateScenePlacement(x,c,130) && !x.scene.permissionPending);
 permission=1;failQuery=false;xrSceneMenuAction(x,2);check(x.scene.querying && destroyed==2);
 xrSceneMenuAction(x,5);check(x.scene.fit);
 std::string menu;char state[512];
 // Dedicated canvas geometry agrees with hit testing, including gaps and footer.
 for(int i=0;i<6;++i)check(xrSceneMenuHit(.5f,1-(334+i*82)/1024.0f)==i);
 check(xrSceneMenuHit(.5f,1-375/1024.0f)==-1);
 check(xrSceneMenuHit(.25f,1-930/1024.0f)==16);
 check(xrSceneMenuHit(.75f,1-930/1024.0f)==17);
 if(argc==2) {
  FILE *out=fopen(argv[1],"wb");check(out!=nullptr);
  for(auto language:{XrLanguage::German,XrLanguage::English})
   for(auto step:{XrScene::Step::Choice,XrScene::Step::Surfaces,XrScene::Step::Done})
    for(bool lost:{false,true}) {
     g_xrLanguage=language;x.scene.step=step;x.roomPoseLost=lost;
     x.scene.message="Keine Raumfreigabe; manuelles Anordnen bleibt verfügbar";
     xrSceneMenuText(x,menu,state,sizeof(state));
     check(std::count(menu.begin(),menu.end(),'\n')==17);
     menu=xrLines(menu);
     for(const auto &field:{std::string("7"),std::string(xrTr("Spielplatz einrichten · P19.1")),std::string(state),menu}) {
      fwrite(field.data(),1,field.size(),out);fputc(0,out);
     }
    }
  check(fclose(out)==0);
 }
 check(releases>5 && saves==2);x.scene.clear();
 // A TABLE with both components uses the 2D tabletop, not its 3D volume.
 XrScene plane;plane.available=true;plane.query=query;plane.retrieve=retrieve;plane.labels=labels;
 plane.box2=planeBox;plane.boundary=noBoundary;plane.box3=box;plane.status=status;plane.enable=enable;
 plane.refresh(XR_NULL_HANDLE);
 event(plane,XrEventDataSpaceQueryResultsAvailableFB{XR_TYPE_EVENT_DATA_SPACE_QUERY_RESULTS_AVAILABLE_FB,nullptr,42});
 check(plane.entries.size()==1 && !plane.entries[0].volume && plane.entries[0].boundary.size()==4);
 check(plane.tableCount()==1 && plane.floorCount()==0);plane.clear();
 printf("PASS %d scene geometry, reference-space, wizard, ownership and modal input checks\n",checks);
 return 0;
}
