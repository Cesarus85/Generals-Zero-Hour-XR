// GeneralsX @test Codex 14/09/2026 Production W3D selection gates with engine spies.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#define __ANDROID__ 1
using Bool=bool;using Int=int;
constexpr int WIN_STATUS_SEE_THRU=1,COLL_TYPE_ALL=0;
struct ICoord2D{int x,y;};
enum PickType{PICK_TYPE_SELECTABLE};
struct Vector3 {
	float X=0,Y=0,Z=0;
	Vector3 operator-(Vector3 b)const{return {X-b.X,Y-b.Y,Z-b.Z};}
	Vector3 operator+(Vector3 b)const{return {X+b.X,Y+b.Y,Z+b.Z};}
	void Normalize(){const float n=sqrtf(X*X+Y*Y+Z*Z);X/=n;Y/=n;Z/=n;}
	void operator*=(float f){X*=f;Y*=f;Z*=f;}
};
struct Drawable{} drawable;
struct DrawableInfo{Drawable *m_drawable=&drawable;} info;
struct RenderObjClass{void *Get_User_Data(){return &info;}} render;
struct GameWindow {
	int status=0;GameWindow *parent=nullptr;
	int winGetStatus(){return status;}GameWindow *winGetParent(){return parent;}
} window,parent;
static bool BitIsSet(int value,int mask){return (value & mask)!=0;}
struct Windows{int calls=0;GameWindow *getWindowUnderCursor(int,int){++calls;return &window;}} windows;
static Windows *TheWindowManager=&windows;
struct LineSegClass{Vector3 a,b;void Set(Vector3 start,Vector3 end){a=start;b=end;}};
struct CastResultStruct{bool ComputeContactPoint=false;Vector3 ContactPoint;};
struct RayCollisionTestClass {
	LineSegClass Ray;CastResultStruct *Result;RenderObjClass *CollidedRenderObj=nullptr;
	RayCollisionTestClass(LineSegClass ray,CastResultStruct *result,int=0,bool=false,bool=false):Ray(ray),Result(result){}
};
static bool terrainHit=true,modelHit=true,spatial=true;
static float castEnd=0;
struct Terrain {
	int calls=0;
	bool Cast_Ray(RayCollisionTestClass &ray){++calls;ray.Result->ContactPoint={0,0,3};return terrainHit;}
} terrain;
static Terrain *TheTerrainRenderObject=&terrain;
struct Scene {
	bool castRay(RayCollisionTestClass &ray,bool,Int){castEnd=ray.Ray.b.Z;ray.CollidedRenderObj=modelHit ? &render:nullptr;return modelHit;}
} scene;
struct W3DDisplay{static Scene *m_3DScene;};Scene *W3DDisplay::m_3DScene=&scene;
bool GX_XR_PointerRay(const ICoord2D *,Vector3 *,Vector3 *){return spatial;}
struct W3DView {
	void getPickRay(const ICoord2D *,Vector3 *a,Vector3 *b){*a={0,0,10};*b={0,0,-10};}
	Drawable *pickDrawable(const ICoord2D *,Bool,PickType);
};
#include "xr-pick-bridge.inc"
static int checks=0;
static void check(bool b){++checks;if(!b){fprintf(stderr,"pick bridge check %d failed\n",checks);exit(1);}}
int main() {
	W3DView view;ICoord2D pixel={640,700};
	auto pick=[&](){return view.pickDrawable(&pixel,false,PICK_TYPE_SELECTABLE);};
	// Opaque legacy HUD cannot block the detached spatial table.
	check(pick()==&drawable);check(windows.calls==0);check(fabsf(castEnd-2.99f)<.001f);
	// Conventional mouse/touch still honors opaque GUI and opaque ancestors.
	spatial=false;check(pick()==nullptr);check(windows.calls==1);
	window.status=WIN_STATUS_SEE_THRU;window.parent=&parent;check(pick()==nullptr);
	parent.status=WIN_STATUS_SEE_THRU;check(pick()==&drawable);check(castEnd==-10);
	// No ground behind the model is a valid object pick, not a canceled click.
	spatial=true;terrainHit=false;check(pick()==&drawable);check(castEnd==-10);
	modelHit=false;check(pick()==nullptr);check(view.pickDrawable(nullptr,false,PICK_TYPE_SELECTABLE)==nullptr);
	printf("PASS %d production pick bridge checks\n",checks);
}
