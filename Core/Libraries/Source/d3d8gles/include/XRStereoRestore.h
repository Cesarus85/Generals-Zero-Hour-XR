// GeneralsX @performance Codex 14/09/2026 Restore only state changed by XR draws.
#pragma once

// Include after the GL declarations. State is the fixed-state key applied
// immediately before the original draw, not an independently guessed cache.
// The eye loop must not change depth enable/write/function, cull, polygon
// offset, stencil operations/masks/function, VAO or texture bindings.
// Atlas draws enable scissor temporarily and disable it before this helper;
// engine draws enter with scissor disabled (beginXRFrame/clear contract).
// If it ever does, extend this contract and its production-draw regression.
template<class State,class BlendMapper>
unsigned gxXrRestoreDrawState(const State &s,int rtHeight,bool shadow,bool ui,BlendMapper blend) {
	unsigned calls=0;
	// Non-shadow eye draws disable stencil, but never alter its parameters.
	if(!shadow && s.stencilEnable) {glEnable(GL_STENCIL_TEST);++calls;}
	glColorMask((s.colorWrite&1)!=0,(s.colorWrite&2)!=0,(s.colorWrite&4)!=0,(s.colorWrite&8)!=0);++calls;
	if(s.alphaBlend) {
		if(ui)glBlendFuncSeparate(blend(s.srcBlend),blend(s.destBlend),GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
		else glBlendFunc(blend(s.srcBlend ? s.srcBlend:D3DBLEND_ONE),blend(s.destBlend ? s.destBlend:D3DBLEND_ZERO));
		++calls;
	}
	glViewport(s.vpX,rtHeight-s.vpY-s.vpH,s.vpW,s.vpH);++calls;
	glDepthRangef(s.vpMinZ,s.vpMaxZ);++calls;
	return calls;
}
