// GeneralsX @performance Codex 14/09/2026 P17 current-frame world omission.
#pragma once
struct GXWorldElision {
    bool requested=false,blocked=false,pendingMissing=false,composedComplete=true,pendingFresh=false;
    unsigned pendingSkipped=0,publishedSkipped=0;
    void configure(bool enabled) {
        if(!enabled)blocked=false; // Explicit full-reserve mode/re-entry permits a later retry.
        requested=enabled;
    }
    bool canOmit(bool currentStereo,bool covered,bool uiAllocated,bool multiviewHealthy) const {
        return requested && !blocked && currentStereo && covered && uiAllocated && multiviewHealthy;
    }
    void omit() {pendingMissing=true;++pendingSkipped;}
    // Only a full backbuffer color clear replaces all previously omitted pixels.
    void fullClear() {pendingFresh=true;pendingMissing=false;}
    bool pendingComplete() const {return (composedComplete || pendingFresh) && !pendingMissing;}
    void publish(bool eyesReady,bool uiReady) {
        composedComplete=pendingComplete();publishedSkipped=pendingSkipped;
        if(pendingMissing && (!eyesReady || !uiReady))blocked=true;
        pendingMissing=false;pendingFresh=false;pendingSkipped=0;
    }
    void requireFullWorld() {blocked=true;}
};
