// GeneralsX @test Codex 13/09/2026 Actual terrain Cast_Ray body and WWMath,
// using a flat height-sample fixture (no renderer/runtime/game initialization).
// Generate xr-terrain-cast.inc from BaseHeightMap.cpp's complete Cast_Ray
// function; include this directory and link the matching built libwwmath.a.
#include "coltest.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
using Bool=bool;using Int=int;using Short=short;
constexpr int MAP_XY_FACTOR=10,VERTEX_BUFFER_TILE_LENGTH=32,SURFACE_TYPE_DEFAULT=0;
constexpr float MAP_HEIGHT_SCALE=.625f;
#define REAL_TO_INT_FLOOR(x) int(std::floor(x))
#define REAL_TO_INT_CEIL(x) int(std::ceil(x))
#define ADJUST_FROM_INDEX_TO_REAL(x) ((x-borderSize)*MAP_XY_FACTOR)
struct FlatMap {
	int getBorderSizeInline(){return 0;}
	int getXExtent(){return 128;}int getYExtent(){return 128;}
	int getMaxHeightValue(){return 255;}
};
class BaseHeightMapRenderObjClass {
public:
	FlatMap fixture;FlatMap *m_map=&fixture;
	Short getClipHeight(int,int){return 64;} // Actual ground is z=40.
	bool Cast_Ray(RayCollisionTestClass &raytest);
};
#ifndef GX_TERRAIN_CAST_BODY
#define GX_TERRAIN_CAST_BODY "xr-terrain-cast.inc"
#endif
#include GX_TERRAIN_CAST_BODY
int main(){
	BaseHeightMapRenderObjClass map;int checks=0;
	const struct {float start,end;bool hit;} cases[]={
		{350,-50,true}, // Conventional camera, both outside broad terrain box.
		{75,-50,true},  // Controller inside box, endpoint outside.
		{75,20,true},   // Both endpoints inside, crosses actual ground.
		{350,20,true},  // Start outside, end inside below ground.
		{75,60,false},  // Inside box is not itself a ground intersection.
		{350,200,false},// Entirely above box.
		{350,50,false}  // Enters box but stops before reaching ground.
	};
	for(const auto &c:cases) {
		LineSegClass line;line.Set(Vector3(505,507,c.start),Vector3(505,507,c.end));
		CastResultStruct result;RayCollisionTestClass ray(line,&result);
		const bool hit=map.Cast_Ray(ray);++checks;
		if(hit!=c.hit || (hit && std::fabs(result.ContactPoint.Z-40)>.001f)) {
			fprintf(stderr,"FAIL start=%.0f end=%.0f hit=%d expected=%d z=%.3f\n",c.start,c.end,hit,c.hit,hit ? result.ContactPoint.Z:-1);return 1;
		}
	}
	printf("PASS %d actual terrain Cast_Ray + WWMath cases\n",checks);
}
