// GeneralsX @feature Codex 13/09/2026 Real terrain boundary, never replacement scenery.
#pragma once
#include "XrWorld.h"
#include "XrBoardGeometry.h"
#include <vector>
struct XrBoardVertex {XrVector3f position;float r,g,b,a;};
static_assert(sizeof(XrBoardVertex)==7*sizeof(float),"GLES decoration stride");
struct XrBoardMesh {
	std::vector<XrBoardVertex> vertices;
	void quad(XrVector3f a,XrVector3f b,XrVector3f c,XrVector3f d,XrVector3f color) {
		for(auto p:{a,b,c,a,c,d}) vertices.push_back({p,color.x,color.y,color.z,1});
	}
	void ring(XrVector3f p,float radius,float thickness,XrVector3f color) {
		for(int i=0;i<32;++i) {
			const float a=i*6.283185307f/32,b=(i+1)*6.283185307f/32;
			quad(xrAdd(p,{cosf(a)*radius,sinf(a)*radius,0}),xrAdd(p,{cosf(b)*radius,sinf(b)*radius,0}),
				xrAdd(p,{cosf(b)*(radius+thickness),sinf(b)*(radius+thickness),0}),xrAdd(p,{cosf(a)*(radius+thickness),sinf(a)*(radius+thickness),0}),color);
		}
	}
};
template<class Height> XrBoardMesh xrBuildBoard(float aspect,int segments,Height height,float ceiling=.5f) {
	XrBoardMesh m;segments=std::clamp(segments,16,1024);
	if(!std::isfinite(aspect) || aspect<=0 || !std::isfinite(ceiling) || ceiling<0) return m;
	// GeneralsX @perf XR 19/09/2026 Pre-size: 4 sides x segments x 3 quads x
	// 6 vertices plus the underside quad; avoids repeated realloc passes on
	// big boards (up to ~43k vertices at full map span).
	m.vertices.reserve(size_t(segments)*4*18+6);
	const auto sample=[&](float x,float y) {const float z=height(x,y);return std::isfinite(z) ? z:0.0f;};
	// GeneralsX @tweak Codex 16/09/2026 P20.1 keeps P18's fixed soil datum
	// while halving only the visible lower plinth thickness.
	const float bottom=kXrBoardSoilBottom,underside=kXrBoardUnderside,lip=kXrBoardLip;
	const XrVector3f corners[]={{-.5f,-aspect*.5f,0},{.5f,-aspect*.5f,0},{.5f,aspect*.5f,0},{-.5f,aspect*.5f,0}};
	const auto outer=[&](XrVector3f p){return XrVector3f{p.x+(p.x<0 ? -lip:lip),p.y+(p.y<0 ? -lip:lip),underside};};
	for(int side=0;side<4;++side) {
		const auto a=corners[side],b=corners[(side+1)%4];
		const auto delta=xrSub(b,a),oa=outer(a),od=xrSub(outer(b),oa);
		for(int i=0;i<segments;++i) {
			auto p=xrAdd(a,xrScale(delta,float(i)/segments)),q=xrAdd(a,xrScale(delta,float(i+1)/segments));
			p.z=std::clamp(sample(p.x,p.y),bottom,ceiling);q.z=std::clamp(sample(q.x,q.y),bottom,ceiling);
			// Soil face follows the actual terrain cut, with a low dark base trim.
			m.quad({p.x,p.y,bottom},{q.x,q.y,bottom},q,p,{.31f,.27f,.20f});
			// Miter outer corners, closing both the trim and its underside.
			const auto po=xrAdd(oa,xrScale(od,float(i)/segments)),qo=xrAdd(oa,xrScale(od,float(i+1)/segments));
			m.quad({p.x,p.y,bottom},{q.x,q.y,bottom},{qo.x,qo.y,bottom},{po.x,po.y,bottom},{.12f,.20f,.22f});
			m.quad({po.x,po.y,underside},{qo.x,qo.y,underside},{qo.x,qo.y,bottom},{po.x,po.y,bottom},{.08f,.12f,.14f});
		}
	}
	m.quad(outer(corners[0]),outer(corners[3]),outer(corners[2]),outer(corners[1]),{.08f,.12f,.14f});
	return m;
}
// GeneralsX @perf XR 19/09/2026 Marks cached board vertices as non-feedback
// (alpha 0) for the decoration shader. Runs only when the cached board is
// rebuilt; per-frame feedback appended later via quad()/ring() already
// carries alpha 1, so no per-frame alpha pass is needed. Covered by
// scripts/qa/xr-board-test.cpp.
inline void xrMarkBoardVertices(XrBoardMesh &m) {
	for(auto &v:m.vertices) v.a=0;
}
