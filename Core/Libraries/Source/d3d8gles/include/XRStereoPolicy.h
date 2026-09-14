// GeneralsX @bugfix Codex 13/09/2026 Model stencil tagging is not a shadow pass.
#pragma once
inline bool gxXrWorldColorPass(unsigned colorMask,bool stencilEnabled,bool stencilAlways) {
	return (colorMask&7)!=0 && (!stencilEnabled || stencilAlways);
}
