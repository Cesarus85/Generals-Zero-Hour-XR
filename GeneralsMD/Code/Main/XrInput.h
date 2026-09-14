// GeneralsX @feature Codex 13/09/2026 Shared production/test pointer routing.
#pragma once
static void updateControls(XrHello &x, const XrControllerState &c, XrTime time)
{
	float u=0.5f, v=0.5f;
	int desired=-1; float nearest=11;
	int hitPiece=-1;
	// P7.3 conventional UI keeps first refusal; world uses its actual 3D ray.
	const bool spatial=x.stereoWorld && x.splitVisible;
	const bool dialog=XrGameBoot_ExpandedUI();
	XrWorldHit worldHit;
	x.rayVisible=x.panelLatched && c.aimValid && !x.arranging;
	x.rayHit=false;x.rayStart=c.aim.position;
	x.rayEnd=xrAdd(c.aim.position,xrRotate(c.aim.orientation,{0,0,-2.5f}));
	if(x.panelLatched && c.aimValid) for(int slot=x.splitVisible ? 1:0;slot<=(x.splitVisible ? 3:0);++slot) {
		if(surfaceRect(slot).h<=0)continue;
		if(spatial && slot==1) continue;
		const auto s=displayedSurface(x,slot);
		float m[16],tu=0,tv=0; surfaceMatrix(s,m);
		if(!panelRayUV(m,surfaceAspect(slot),c.aim,&tu,&tv)) continue;
		const auto rect=surfaceRect(slot);
		// The unframed HUD's empty space must not become an invisible input wall.
		if(slot==3 && !XrGameBoot_HasUIAt((rect.x+tu*rect.w)*(XrGameBoot_GameWidth()-1),
			(1-rect.y-tv*rect.h)*(XrGameBoot_GameHeight()-1))) continue;
		const auto point=xrAdd(s.pose.position,xrRotate(s.pose.orientation,
			{(tu-.5f)*s.width,(tv-.5f)*s.width*surfaceAspect(slot),0}));
		const float distance=xrLength(xrSub(point,c.aim.position));
		if(distance<nearest) { nearest=distance; desired=slot==3 ? 2:slot; hitPiece=slot; u=tu; v=tv; x.rayEnd=point; }
	}
	if(spatial && !dialog && x.stereoVisible && c.aimValid && desired!=2 && !x.arranging &&
		XrGameBoot_PickWorld(x.surfaces[1],c.aim,worldHit)) {desired=1;hitPiece=1;nearest=worldHit.distance;x.rayEnd=worldHit.room;}
	// The on-demand UI button/menu captures input before this game router.
	const int oldRoute=x.pointerRoute.source;
	const bool routed=x.pointerRoute.update(desired,c.select || c.secondary);
	if(x.pointerRoute.source!=oldRoute && x.pointerRoute.source>=0) XrGameBoot_RoutePointer(x.pointerRoute.source);
	const bool hit=desired>=0 && routed;
	x.rayHit=hit;
	if (!hit) x.inputArmed=false;
	else if (!c.select && !c.secondary) x.inputArmed=true;
	x.pointerVisible=hit;
	x.pointerPiece=hit ? hitPiece:-1;
	x.pointerPressed=hit && x.inputArmed && c.select;
	if (hit) { x.pointerU=u; x.pointerV=v; }
	const bool world=spatial && hit && desired==1;
	XrGameBoot_SpatialPointer(world);
	x.worldCursorVisible=world;x.worldCursor=worldHit.room;
	float wheel=0;
	if (!spatial && !dialog && hit && desired!=2 && x.interactiveGame && !c.tilt && fabsf(c.zoom.y)>0.4f && time>=x.nextZoomTime) {
		wheel=c.zoom.y>0 ? 1.0f : -1.0f;
		x.cameraCustom=true;
		x.nextZoomTime=time+150000000;
	}
	const auto rect=surfaceRect(hitPiece);
	XrGameBoot_Pointer(hit,world ? worldHit.x:(rect.x+u*rect.w)*(XrGameBoot_GameWidth()-1),
		world ? worldHit.y:(1-rect.y-v*rect.h)*(XrGameBoot_GameHeight()-1),
		!world && x.pointerPressed,!world && hit && x.inputArmed && c.secondary,wheel);
	const bool commandTarget=world && !dialog && x.inputArmed && !c.back && !c.tilt &&
		fabsf(c.pan.x)<.25f && fabsf(c.pan.y)<.25f && fabsf(c.zoom.x)<.25f && fabsf(c.zoom.y)<.25f;
	XrGameBoot_SpatialTrigger(c.select,commandTarget && !c.secondary,c.grip[0]);
	const bool cancel=x.worldCancel.update(c.secondary,commandTarget && !c.select);
	if(cancel) XrGameBoot_SpatialClick(true);
	const bool pan = !spatial && !dialog && x.interactiveGame && hit && desired!=2;
	const bool keys[] = {c.back,pan && c.pan.x < -0.4f,pan && c.pan.x > 0.4f,
	                    pan && c.pan.y > 0.4f,pan && c.pan.y < -0.4f};
	for (int i=0;i<5;++i) if (keys[i]!=x.keyHeld[i]) {
		XrGameBoot_Key((XrGameKey)i,keys[i]); x.keyHeld[i]=keys[i];
	}
}
