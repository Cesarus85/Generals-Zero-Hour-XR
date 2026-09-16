// GeneralsX @test Codex 13/09/2026 Sampled terrain cut and renderer budget.
#include "XrBoardMesh.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
static int checks=0;
static void check(bool v){++checks;if(!v){fprintf(stderr,"board check %d failed\n",checks);exit(1);}}
int main(){
	check(fabsf(kXrBoardSoilBottom+.018f)<.000001f);
	check(fabsf(kXrBoardPlinthThickness-.009f)<.000001f);
	check(fabsf(kXrBoardUnderside+.027f)<.000001f);
	check(kXrBoardSoilBottom-kXrBoardUnderside<=.009001f);
	for(float aspect:{.5f,1.0f,2.0f}) for(int segments:{0,16,600,2000}) {
		const auto m=xrBuildBoard(aspect,segments,[](float x,float y){return .1f+.04f*x+.02f*y;});
		const int n=std::clamp(segments,16,1024);check(m.vertices.size()==size_t(n*4*18+6));
		bool finite=true,bounded=true,edge=true;
		for(const auto &v:m.vertices){const auto p=v.position;finite &= std::isfinite(p.x+p.y+p.z);bounded &= fabsf(p.x)<=.513f && fabsf(p.y)<=aspect*.5f+.013f && p.z>=-.164f && p.z<=.49f;}
		for(int i=0;i<4*n;++i){const auto &p=m.vertices[i*18+2].position;edge &= fabsf(p.z-(.1f+.04f*p.x+.02f*p.y))<.00001f;}
		check(finite);check(bounded);check(edge);check(m.vertices.size()+192*396+768<200000);
	}
	const float nan=std::numeric_limits<float>::quiet_NaN();
	check(xrBuildBoard(nan,16,[](float,float){return 0.0f;}).vertices.empty());
	check(xrBuildBoard(0,16,[](float,float){return 0.0f;}).vertices.empty());
	const auto safe=xrBuildBoard(1,16,[&](float,float){return nan;});bool finite=true;
	for(const auto &v:safe.vertices)finite &= std::isfinite(v.position.z);check(finite);
	for(size_t i=safe.vertices.size()-6;i<safe.vertices.size();++i)
		check(fabsf(safe.vertices[i].position.z-kXrBoardUnderside)<.000001f);
	XrBoardMesh ring;ring.ring({0,0,1},3,1,{1,1,1});check(ring.vertices.size()==192);
	printf("PASS %d terrain board/ring checks\n",checks);
}
