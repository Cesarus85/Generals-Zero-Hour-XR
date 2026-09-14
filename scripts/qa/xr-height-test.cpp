// GeneralsX @test Codex 14/09/2026 P18 terrain datum, plinth and ray agreement.
// Host/NDK C++17; include Main, d3d8gles/include and configured OpenXR headers.
#include "XrBoardMesh.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
static unsigned checks=0;
static void check(bool b){++checks;if(!b){fprintf(stderr,"height check %u failed\n",checks);exit(1);}}
static void near(float a,float b,float epsilon=.0003f){check(std::isfinite(a+b) && fabsf(a-b)<epsilon);}
int main(){
 const float pitch=37.5f*3.141592654f/180,fov=50*3.141592654f/180;
 near(xrStableWorldSpan(500,fov,pitch,1),612.814f,.02f);
 near(xrStableWorldSpan(250,fov,pitch,1)*2,xrStableWorldSpan(500,fov,pitch,1));
 near(xrStableWorldSpan(500,fov,pitch,2),xrStableWorldSpan(500,fov,pitch,1)*2);
 check(xrStableWorldSpan(1,fov,pitch,1)==200 && xrStableWorldSpan(5000,fov,pitch,5)==3000);
 check(xrStableWorldSpan(0,fov,pitch,1)==0);
 check(xrStableWorldSpan(500,std::numeric_limits<float>::quiet_NaN(),pitch,1)==0);
 // Each map uses its own loader minimum, not a first-view or center sample.
 // Scroll/yaw never changes Z; user zoom/physical scale intentionally can.
 for(float datum:{0.0f,15.625f,80.0f,159.375f})
 for(float span:{200.0f,619.0f,3000.0f})
 for(float width:{.45f,1.65f,4.0f})
 for(float yaw:{-1.3f,0.0f,.8f}) {
  float m[16];XrSurface board;board.width=width;
  board.pose={xrMul(xrAxisAngle({0,1,0},.6f),xrAxisAngle({1,0,0},-1.35f)),{.3f,-.5f,-1.1f}};
  const auto room=[&](XrVector3f local){return xrAdd(board.pose.position,xrRotate(board.pose.orientation,xrScale(local,width)));};
  for(float pan:{-250.0f,0.0f,320.0f}) {
   check(xrWorldToBoard(m,{1000+pan,700-pan,datum},{cosf(yaw),sinf(yaw),0},span));
   const float ceiling=gxXrBoardCeiling(m);
   for(float elevation:{datum,159.375f}) {
    // Place both a terrain/build target and a model roof on exactly the ray.
    const auto ground=xrInversePoint(m,{.1f,-.05f,(elevation-datum)/span});
    for(float objectHeight:{0.0f,20.0f,70.0f}) {
     const auto target=xrAdd(ground,{0,0,objectHeight});const auto local=xrTransformPoint(m,target);
     near(local.z,(elevation+objectHeight-datum)/span);
     check(xrBoardContainsSphere(m,.56f,target,0));check(local.z<=ceiling);
     XrPosef aim={board.pose.orientation,room({.1f,-.05f,ceiling+.25f})};
     XrVector3f start,end;check(xrWorldRay(board,.56f,m,aim,start,end));
     const float t=(target.z-start.z)/(end.z-start.z);check(t>=0 && t<=1);
     const auto hit=xrAdd(start,xrScale(xrSub(end,start),t));
     near(hit.x,target.x,.003f);near(hit.y,target.y,.003f);near(hit.z,target.z,.003f);
     near(xrLength(xrSub(room(xrTransformPoint(m,hit)),room(local))),0);
    }
   }
   // High/low edges alter only soil faces, never the outer rim or underside.
   const auto low=xrBuildBoard(.56f,16,[](float,float){return 0.0f;},ceiling);
   const float raised=(159.375f-datum)/span;
   const auto high=xrBuildBoard(.56f,16,[&](float,float){return raised;},ceiling);
   check(low.vertices.size()==high.vertices.size());
   for(size_t i=0;i<high.vertices.size();++i) {
    const auto &a=low.vertices[i],&b=high.vertices[i];
    if(a.r!=.31f)near(xrLength(xrSub(a.position,b.position)),0);
   }
   for(size_t i=high.vertices.size()-6;i<high.vertices.size();++i) {
    near(high.vertices[i].position.z,-.036f);
    near(xrLength(xrSub(room(high.vertices[i].position),room(low.vertices[i].position))),0);
   }
  }
 }
 // The old .5 ceiling would remove both terrain and picking on high maps
 // at close zoom. The CPU envelope now includes the same height as GLES.
 float m[16];check(xrWorldToBoard(m,{0,0,0},{1,0,0},200));
 check(xrTransformPoint(m,{0,0,159.375f}).z>.5f);
 check(xrBoardContainsSphere(m,1,{0,0,159.375f},0));
 check(!xrBoardContainsSphere(m,1,{0,0,300},0));
 printf("PASS %u stable height/plinth/render-pick checks\n",checks);
}
