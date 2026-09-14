// GeneralsX @bugfix Codex 14/09/2026 View intent is not capture readiness.
#pragma once
// GeneralsX @bugfix Codex 14/09/2026 A deliberately omitted planar image
// must never be sampled after late stereo loss. Use a complete composed
// frame, or request recovery when P17 omitted ordinary draws as well.
template<class Host> bool xrResolveCapturedView(Host &host,bool splitReady,bool eyesReady,bool planarReady,bool composedReady=true) {
	if(!splitReady)host.splitVisible=false;
	host.stereoVisible=host.stereoWorld && splitReady && eyesReady;
	if(!host.stereoVisible && !planarReady)host.splitVisible=false;
	// P17: a missing ordinary world is not a valid fallback image. The caller
	// displays an explicit recovery card and requests full rendering next frame.
	return !host.stereoVisible && !host.splitVisible && !composedReady;
}
template<class Host> void xrApplyWorldStartup(Host &host,bool eligible) {
	// P15 gameplay is always tabletop, including return from a cinematic.
	if(host.interactiveGame && eligible) {
		host.stereoWorld=true;host.uprightGame=false;host.startViewApplied=true;
	}
}
template<class Host> void xrRequestWorldView(Host &host,bool enabled) {
	(void)enabled; // Legacy callers cannot select planar gameplay.
	host.stereoWorld=true;
	host.uprightGame=false; // Release the legacy full-panel override first.
	host.startViewApplied=true; // A later first capture must not undo this choice.
	host.controlsArmed=false;host.grab.cancel();
}
