// GeneralsX @feature Codex 13/09/2026 Exclusive arrangement versus game input.
// Included after XrHello and updateControls; no engine state in placement math.
#pragma once
#include "XrCameraProfile.h"
#include "XrViewMode.h"
#include "XrWorkspacePlacement.h"

static void saveLayout(XrHello &x)
{
	// GeneralsX @bugfix Codex 14/09/2026 Never persist poses in an unknown
	// room reference; closing recovery must not overwrite the last good layout.
	if (!x.anchorKnown || !x.layoutDirty || x.roomPoseLost) return;
	x.layout.worldZoom=x.worldZoom;
	for (int i=0;i<3;++i) {
		x.layout.relative[i]=x.surfaces[i];
		x.layout.relative[i].pose=xrPoseMul(xrPoseInverse(x.layoutAnchor),x.surfaces[i].pose);
	}
	if (x.layout.save(XrGameBoot_LayoutPath())) {
		x.layoutDirty=false; XR_LOG("P3 layout saved: upright=%.2fm table=%.2fm",x.surfaces[0].width,x.surfaces[1].width);
	} else XR_LOGE("P3 could not save layout");
}

static float xrStick(float v) { return fabsf(v)>.25f ? (v>0 ? v-.25f : v+.25f)/.75f : 0; }

static void updateInteraction(XrHello &x, const XrControllerState &c, const XrView *views, XrTime time)
{
	const float dt=x.previousInputTime ? std::clamp(float(time-x.previousInputTime)*1e-9f,0.0f,.05f) : 0;
	x.previousInputTime=time;
	// GeneralsX @feature Codex 17/09/2026 Observer owns every controller
	// action. The native game receives explicit releases, never a world click.
	if(x.observer.mode!=XrObserverMode::Off) {
		const bool neutral=!c.buttonsHeld && !c.select && !c.secondary && !c.back &&
			!c.grip[0] && !c.grip[1] && fabsf(c.pan.x)<.25f && fabsf(c.pan.y)<.25f &&
			fabsf(c.zoom.x)<.25f && fabsf(c.zoom.y)<.25f;
		x.observer.neutral(neutral);
		updateControls(x,XrControllerState{},time);
		x.grab.cancel();x.buildRotation={};x.commands.input.click.cancel();x.menu.click.cancel();
		x.controlsArmed=false;x.inputArmed=false;x.hoverVisible=false;
		if(x.observer.mode==XrObserverMode::Armed) {
			x.rayVisible=c.aimValid;x.rayStart=c.aim.position;
			x.rayEnd=xrAdd(c.aim.position,xrRotate(c.aim.orientation,{0,0,-2.5f}));
			XrVector3f target={},room={};const bool valid=c.aimValid &&
				XrGameBoot_PickObserverGround(x.surfaces[1],c.aim,target,&room);
			x.rayHit=valid;
			if(valid)x.rayEnd=room;
			if((c.back || c.secondary) && !x.observer.requireRelease) {
				x.observer.cancel();x.controlsArmed=false;x.rayVisible=false;
			} else if(x.observer.canChoose(c.select) && valid) {
				const auto head=xrScale(xrAdd(views[0].pose.position,views[1].pose.position),.5f);
				float fx=0,fz=-1;yawForwardFromQuat(views[0].pose.orientation,&fx,&fz);
				if(x.observer.choose(target,head,{fx,0,fz})) {
					x.observerFadeStart=time;x.rayVisible=false;x.menu.open=false;
				}
			}
		} else if(c.back && !x.observer.requireRelease) {
			x.observer.cancel();x.observerFadeStart=time;x.controlsArmed=false;
		} else if(x.observer.mode==XrObserverMode::Active && !x.observer.requireRelease) {
			// Physical left/right sticks are independent of gameplay handedness.
			// Look direction includes real head rotation; only the virtual world
			// turns, with the current head as pivot.
			const auto head=xrScale(xrAdd(views[0].pose.position,views[1].pose.position),.5f);
			float fx=0,fz=-1;yawForwardFromQuat(views[0].pose.orientation,&fx,&fz);
			x.observer.turn(xrStick(c.rightStick.x),dt,head);
			const auto delta=x.observer.walkDelta({xrStick(c.leftStick.x),xrStick(c.leftStick.y)},
				{fx,0,fz},dt);
			if(fabsf(delta.x)+fabsf(delta.y)>.0001f) {
				XrVector3f next={};
				if(XrGameBoot_ObserverStep(x.observer.ground,delta,next) ||
					(fabsf(delta.x)>.0001f && XrGameBoot_ObserverStep(x.observer.ground,{delta.x,0,0},next)) ||
					(fabsf(delta.y)>.0001f && XrGameBoot_ObserverStep(x.observer.ground,{0,delta.y,0},next)))
					x.observer.ground=next;
			}
		}
		return;
	}
	// GeneralsX @feature Codex 14/09/2026 Modal/focus/arrangement transitions
	// clear the modifier capture; their existing neutral re-arm still applies.
	if(x.state!=XR_SESSION_STATE_FOCUSED || !c.aimValid || !x.controlsArmed ||
		x.menu.open || x.arranging || XrGameBoot_ExpandedUI())x.buildRotation={};
	// A is the direct console shortcut in live play, not a hidden layout mode.
	if(x.state==XR_SESSION_STATE_FOCUSED && c.aimValid && x.controlsArmed && c.arrange &&
		!x.arranging && !XrGameBoot_ExpandedUI() && x.interactiveGame && x.splitVisible && XrGameBoot_CanStereoWorld()) {
		x.layout.commandsVisible=!x.layout.commandsVisible;x.layoutDirty=true;x.commands.groupOperation=0;
		x.menu.open=false;x.controlsArmed=false;saveLayout(x);updateControls(x,XrControllerState{},time);return;
	}
	if (x.state!=XR_SESSION_STATE_FOCUSED) {
		XrGameBoot_CancelTarget();x.commands.bookmarkSave=false;x.commands.groupOperation=0;
		x.grab.cancel(); x.controlsArmed=false;x.menu.click.cancel();x.commands.input.click.cancel();
		updateControls(x,XrControllerState{},time); return;
	}
	if(!x.arranging && !c.aimValid) {
		XrGameBoot_CancelTarget();x.commands.bookmarkSave=false;x.commands.groupOperation=0;
		x.controlsArmed=false;x.menu.click.cancel();x.commands.input.click.cancel();updateControls(x,XrControllerState{},time);return;
	}
	// Release all game actions before changing modes. Held inputs after focus
	// loss cannot become a click, camera motion or a new grab on reacquisition.
	if (!x.controlsArmed && !x.menu.open) {
		if (!c.buttonsHeld && !c.select && !c.grip[0] && !c.grip[1] && !c.back && !c.tilt &&
		    !c.arrange && !c.preset && !c.homeBase && !c.upright && !c.recenter &&
		    xrStick(c.pan.x)==0 && xrStick(c.pan.y)==0 && xrStick(c.zoom.x)==0 && xrStick(c.zoom.y)==0)
			x.controlsArmed=true;
		updateControls(x,XrControllerState{},time); return;
	}
	// GeneralsX @bugfix Codex 14/09/2026 Exit precedes menu/console capture:
	// pointing at either panel must not swallow the arrangement Back/A key.
	if(x.arranging && (c.back || c.arrange)) {
		finishArrangement(x);updateControls(x,XrControllerState{},time);return;
	}
	if(updateXrMenu(x,c,views,time)) return;
	// GeneralsX @bugfix Codex 14/09/2026 A native dialog consumes navigation,
	// shortcuts and world gestures but still receives its normal pointer/back.
	if(XrGameBoot_ExpandedUI()) {updateControls(x,c,time);placePanel(x,views);return;}
	// Supporting Grip is additive selection outside construction. During a
	// building preview it exclusively owns the sticks; dominant Grip cancels.
	const bool buildPending=!x.arranging && !x.diorama && x.stereoWorld &&
		x.splitVisible && XrGameBoot_CanRotatePlacement();
	const float buildDelta=x.buildRotation.update(buildPending,c.grip[0],c.zoom,
		c.select || c.secondary || c.back || c.tilt,dt);
	const bool buildControl=x.buildRotation.captured;
	// GeneralsX @feature Codex 15/09/2026 Home consumes this frame before pan,
	// zoom or ray orders. It remains available over the nonmodal Commands panel.
	if(c.homeBase && !x.arranging) {
		if(!buildPending && !buildControl && !x.diorama && x.interactiveGame && x.splitVisible &&
			!c.select && !c.secondary && !c.grip[0] && !c.grip[1] && !c.tilt && !c.back &&
			!c.recenter && !c.upright && !c.preset && XrGameBoot_ViewBase())x.cameraCustom=true;
		placePanel(x,views);updateControls(x,XrControllerState{},time);return;
	}
	// GeneralsX @bugfix Codex 14/09/2026 The console captures the pointer,
	// not the supporting stick. Modal workspace/focus/arrangement still block pan.
	if(!buildControl && !x.arranging && !x.diorama && x.stereoWorld && x.splitVisible && c.aimValid &&
		!c.back && !c.upright && !c.recenter && !c.preset &&
		XrGameBoot_NavigateWorld(xrStick(c.pan.x)*dt,xrStick(c.pan.y)*dt,0)) x.cameraCustom=true;
	if(updateCommands(x,c,time)) return;
	if (c.arrange || (x.arranging && c.back)) {
		x.arranging=!x.arranging; x.grab.cancel(); x.controlsArmed=false;
		saveLayout(x); updateControls(x,XrControllerState{},time);
		XR_LOG("P3 mode=%s",x.arranging ? "ARRANGE" : "PLAY"); return;
	}
	int slot=activeSurface(x);
	// GeneralsX @feature Codex 13/09/2026 Opt-in stereo proof from the shell only.
	// Never hide a live match or send demo controls into its command stream.
	if(!x.arranging && ((c.tilt && c.upright) || (x.diorama && c.back))) {
		if(x.diorama || (!x.interactiveGame && x.dioramaReady)) {
			x.diorama=!x.diorama; x.controlsArmed=false; x.grab.cancel();
			placePanel(x,views); updateControls(x,XrControllerState{},time);
			XR_LOG("P6 stereo diorama=%s (shell-only, no game-world stereo)",x.diorama ? "ON":"OFF");
		}
		return;
	}
	if(x.arranging && x.splitVisible && c.preset) {
		x.arrangeSlot=x.arrangeSlot==1 ? 2:1; slot=x.arrangeSlot;
		x.grab.cancel(); x.controlsArmed=false;
		placePanel(x,views); updateControls(x,XrControllerState{},time); return;
	}
	if (c.upright) {
		if (x.arranging) {
			x.layout.snap[slot]=!x.layout.snap[slot];
			if(x.layout.snap[slot]) snapSurface(x.surfaces[slot],slot==1);
			x.grab.cancel(); x.layoutDirty=true; saveLayout(x);
		}
	}
	if (c.recenter && c.tilt && !x.arranging && x.interactiveGame) {
		x.cameraPreset=XrGameBoot_DefaultCameraPreset(); x.cameraPending=true;
		x.cameraCustom=false; x.cameraSaveFailed=false;
	} else if (c.recenter) {
		if(!x.arranging && x.interactiveGame) {
			requestWorkspaceRecenter(x,views);
			placePanel(x,views);updateControls(x,XrControllerState{},time);return;
		} else {
		// X restores a reachable pose; arrangement additionally restores size.
		float fx=0,fz=-1; yawForwardFromQuat(views[0].pose.orientation,&fx,&fz);
		const XrPosef head={xrAxisAngle({0,1,0},atan2f(-fx,-fz)),
			xrScale(xrAdd(views[0].pose.position,views[1].pose.position),.5f)};
		const XrLayout defaults;
		x.surfaces[slot].pose=xrPoseMul(head,defaults.relative[slot].pose);
		if(x.arranging) x.surfaces[slot].width=defaults.relative[slot].width;
		}
		x.grab.cancel(); x.layoutDirty=true; saveLayout(x);
	}
	if (x.arranging) {
		updateControls(x,XrControllerState{},time);
		auto &surface=x.surfaces[slot];
		const int oldMode=x.grab.mode;
		const bool changed=x.grab.update(surface,c.hands,c.grip,c.handValid,slot==1 ? 4.0f:2.5f);
		// GeneralsX @bugfix Codex 13/09/2026 A manual window grab overrides even
		// an old saved LEVEL flag. Y can explicitly align it again, once.
		if(slot!=1 && changed) x.layout.snap[slot]=false;
		bool stickChanged=false;
		if (!c.grip[0] && !c.grip[1] && c.aimValid) {
			const float scale=xrStick(c.zoom.y), distance=xrStick(c.pan.y);
			if (scale!=0) surface.width=std::clamp(surface.width*expf(scale*dt),.45f,slot==1 ? 4.0f:2.5f);
			if (distance!=0) surface.pose.position=xrAdd(surface.pose.position,
				xrRotate(c.aim.orientation,{0,0,-distance*dt*.6f}));
			stickChanged=scale!=0 || distance!=0;
		}
		// Keep saved layouts within the validated, recoverable personal workspace.
		auto offset=xrSub(surface.pose.position,x.layoutAnchor.position);
		if(xrLength(offset)>4.5f) surface.pose.position=xrAdd(x.layoutAnchor.position,xrScale(offset,4.5f/xrLength(offset)));
		x.layoutDirty=x.layoutDirty || changed || stickChanged;
		if(slot==1 && oldMode!=0 && !c.grip[0] && !c.grip[1] && x.layout.snap[slot]) snapSurface(surface,true);
		if(x.grab.mode==0 && !stickChanged) saveLayout(x);
	} else if(x.diorama) {
		updateControls(x,XrControllerState{},time);
	} else {
		if (!buildControl && c.preset && x.interactiveGame) {
			if(c.tilt) {
				x.cameraSaveFailed=!XrGameBoot_SaveCameraDefault();
				if(!x.cameraSaveFailed) { x.cameraPreset=kXrCameraFavorite; x.cameraPending=false; x.cameraCustom=false; }
			} else {
				x.cameraPreset=(x.cameraPreset+1)%kXrCameraPresetCount;
				x.cameraPending=true; x.cameraCustom=false; x.cameraSaveFailed=false;
			}
		}
		// P12.1 Wait for a completed world/UI capture, not merely the early
		// GAME_SINGLE_PLAYER flag set while the campaign is still starting.
		if(x.cameraPending && x.splitVisible && XrGameBoot_CameraPreset(x.cameraPreset)) x.cameraPending=false;
		const float yaw=buildControl || c.preset || c.recenter ? 0 : -xrStick(c.zoom.x)*dt;
		const float pitch=!buildControl && c.tilt && !c.preset && !c.recenter ? xrStick(c.zoom.y)*dt*.65f : 0;
		if(c.aimValid && x.pointerRoute.source!=2 && (yaw!=0 || pitch!=0) && XrGameBoot_AdjustCamera(yaw,pitch)) {
			x.cameraCustom=true; x.cameraPending=false; x.cameraSaveFailed=false;
		}
		placePanel(x,views);
		auto routed=c;
		if(!buildControl && x.stereoWorld && x.splitVisible && c.aimValid && !c.back && !c.tilt && !c.preset && !c.recenter && XrGameBoot_CanAdjustWorld()) {
			const float zoom=std::clamp(x.worldZoom*expf(-xrStick(c.zoom.y)*dt*.8f),.5f,3.0f);
			x.layoutDirty=x.layoutDirty || zoom!=x.worldZoom;x.worldZoom=zoom;
		}
		if(xrStick(c.zoom.y)==0) saveLayout(x);
		if(c.preset || (c.recenter && c.tilt)) routed.zoom={0,0};
		if(buildControl) {routed.pan={0,0};routed.zoom={0,0};}
		updateControls(x,routed,time);
		// Ray routing has now established current spatial ownership. UI hover,
		// tracking loss and held/releasing placement triggers cannot rotate it.
		if(buildDelta!=0)XrGameBoot_RotatePlacement(buildDelta);
	}
	placePanel(x,views); // identical transform for next render and ray picking
}
