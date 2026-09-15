// GeneralsX @bugfix Codex 14/09/2026 Shared exit for every workspace route.
#pragma once
#include "XrWorkspacePlacement.h"
static void finishArrangement(XrHello &x) {
	x.arranging=false;x.menu.open=false;x.grab.cancel();x.controlsArmed=false;
	x.menu.click.cancel();x.commands.input.click.cancel();saveLayout(x);
}
// GeneralsX @feature Codex 15/09/2026 One explicit recenter action for the
// visible UI button and controller shortcut; never silently detach furniture.
static void requestWorkspaceRecenter(XrHello &x,const XrView *views,bool confirmed=false) {
	x.grab.cancel();x.arranging=false;x.controlsArmed=false;x.inputArmed=false;
	x.buildRotation={};XrGameBoot_CancelTarget();
	x.menu.click.cancel();x.commands.input.click.cancel();
	if(x.scene.placed && !confirmed) {
		x.menu.open=true;x.menu.page=6;
		x.menu.surface.pose=xrPoseMul(xrWorkspaceHeading(views),{{0,0,0,1},{0,-.16f,-.9f}});
		x.menu.surface.width=.64f;return;
	}
	if(x.roomPoseLost) {
		// Unknown origin: old relative distances cannot be trusted either.
		bool known=false;const XrLayout defaults;
		xrInitializeWorkspace(known,defaults,x.surfaces,x.layoutAnchor,views);
	} else xrRecenterWorkspace(x.surfaces,x.layoutAnchor,x.menu.surface,views);
	x.scene.cancel();x.scene.placed=false;x.scene.step=XrScene::Step::Done;
	x.scene.message="Freier Tisch aktiv; reale Fläche ist optional";
	x.roomPoseLost=false;x.layoutDirty=true;x.menu.page=0;finishArrangement(x);
}
