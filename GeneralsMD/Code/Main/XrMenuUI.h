// GeneralsX @feature Codex 13/09/2026 On-demand workspace panel. Rendering
// and ray hit testing share geometry; modal capture never leaks game clicks.
#pragma once
#include "XrViewMode.h"
#include "XrWorkspacePlacement.h"
static XrSurface uiButtonSurface(const XrHello &x) {
	if(!x.splitVisible) {
		auto s=x.surfaces[0];
		s.pose.position=xrAdd(s.pose.position,xrRotate(s.pose.orientation,{s.width*.57f,0,.025f}));
		s.width=.20f;return s;
	}
	// All three workspace shortcuts form one upright column beside the board.
	// Derive heading from its right edge: a flat board has no usable forward yaw.
	const auto &board=x.surfaces[1];
	auto right=xrRotate(board.pose.orientation,{1,0,0});
	const float length=sqrtf(right.x*right.x+right.z*right.z);
	const auto heading=length>1e-4f ? xrAxisAngle({0,1,0},atan2f(-right.z,right.x)):
		XrQuaternionf{0,0,0,1};
	XrSurface s;
	s.width=.20f;s.pose.orientation=heading;
	// Keep the column level with the board center in depth; the former +8 cm
	// player-facing offset put the buttons visibly ahead of the table edge.
	s.pose.position=xrAdd(board.pose.position,xrRotate(heading,{board.width*.5f+.135f,.40f,0}));
	return s;
}
// UI, Commands and Ground View share width, facing and 16 cm vertical pitch.
static XrSurface groundButtonSurface(const XrHello &x) {
	auto s=uiButtonSurface(x);
	s.pose.position=xrAdd(s.pose.position,{0,-.32f,0});
	return s;
}
static bool groundButtonAvailable(const XrHello &x) {
	return x.interactiveGame && x.splitVisible && x.stereoVisible &&
		!x.menu.open && !x.arranging && !x.scene.placing && !x.recoveryVisible &&
		x.observer.mode==XrObserverMode::Off && XrGameBoot_CanObserveGround();
}
static XrSurface hoverCardSurface(const XrHello &x) {
	auto s=x.surfaces[2];
	s.pose.position=xrAdd(s.pose.position,xrRotate(s.pose.orientation,{0,s.width*surfaceAspect(2)*.5f+.38f,.015f}));
	s.width=.68f;return s;
}
#include "XrArrangement.h"
static bool armGroundView(XrHello &x) {
	if(!x.stereoVisible || !XrGameBoot_CanObserveGround() || !x.observer.arm(true))return false;
	x.menu.open=false;x.controlsArmed=false;x.inputArmed=false;x.grab.cancel();
	x.menu.click.cancel();x.commands.input.click.cancel();XrGameBoot_CancelTarget();
	return true;
}
static void applyMenuAction(XrHello &x,int action,const XrView *views) {
	if(x.menu.page==6) {
		if(action==18)requestWorkspaceRecenter(x,views,true);
		else if(action==19)x.menu.page=0;
		else if(action==17)finishArrangement(x);
		return;
	}
	if(x.menu.page==0 && action==18){requestWorkspaceRecenter(x,views);return;}
	// GeneralsX @feature Codex 14/09/2026 Guide is reachable from every tab,
	// including the shell. Help navigation never dispatches gameplay actions.
	if(xrSceneMenuAction(x,action))return;
	if(action==24) {x.menu.page=4;x.menu.helpPage=0;return;}
	if(x.menu.page==4) {
		if(action==33)finishArrangement(x);
		else if(action==34)x.menu.page=0;
		else if(action==36)x.menu.helpPage=(x.menu.helpPage+1)%kXrControllerHelpPages;
		return;
	}
	if(action==17) {finishArrangement(x);return;}
	if(action>=20 && action<=23) {x.menu.page=action-20;return;}
	if(x.menu.page==1 || x.menu.page==2) {
		if(action==17) {x.menu.open=false;x.controlsArmed=false;return;}
		if(x.menu.page==1) {
			if(action==14) {x.menu.page=2;return;}
			if(action==15) {x.menu.open=false;x.controlsArmed=false;return;}
			// Targeted tactics use the real world ray, never an inert planar mode.
			if(action>=0 && action<=9) {
				if(!x.splitVisible || !XrGameBoot_CanAdjustWorld()) return;
				x.stereoWorld=true;
			}
			XrGameBoot_TacticalAction(action==16 ? 31:action);
			if(action<=8 || action==12 || action==13 || action==16) {x.menu.open=false;x.controlsArmed=false;}
		} else {
			if(action<12) XrGameBoot_TacticalAction(action+20);
			if(action==12) XrGameBoot_TacticalAction(12);
			if(action==13) XrGameBoot_TacticalAction(10);
			if(action==14) {x.menu.page=1;return;}
			if(action==15) {x.menu.page=0;return;}
			if(action==16) XrGameBoot_TacticalAction(13);
			if((action>=3 && action<=11) || action==12) {x.menu.open=false;x.controlsArmed=false;}
		}
		return;
	}
	if(x.menu.page==3) {
		if(action<0 || action>16) return;
		if(action<=3)return; // P15 reserved status/help slots; no flat mode.
		if(action==16) {
			armGroundView(x);
			return;
		}
		// GeneralsX @performance Codex 14/09/2026 Session-only experiments,
		// no layout save/migration and no simulation/input commands.
		if(action>=12 && action<=15) {
			if(action==12)x.performance.volumeShadows=!x.performance.volumeShadows;
			else if(action==13)x.performance.enabled=!x.performance.enabled;
			else if(action==14) {
				if(x.performance.multiviewStereo){x.performance.multiviewStereo=false;x.performance.atlasStereo=false;}
				else if(x.performance.atlasStereo){x.performance.atlasStereo=false;x.performance.multiviewStereo=true;}
				else x.performance.atlasStereo=true;
			}
			else x.performance.elideWorldCopy=!x.performance.elideWorldCopy;
			x.performance.invalidate();return;
		}
		if(action==4) x.layout.healthBars=!x.layout.healthBars;
		if(action==5) x.layout.unitRings=!x.layout.unitRings;
		if(action==6) x.layout.boardFrame=!x.layout.boardFrame;
		if(action==7) {x.menu.open=false;x.controlsArmed=false;}
		// Changing roles closes the menu and requires both controllers neutral.
		if(action==8) {x.layout.leftHanded=!x.layout.leftHanded;x.menu.open=false;x.controlsArmed=false;x.grab.cancel();x.menu.click.cancel();x.commands.input.click.cancel();}
		// GeneralsX @feature Codex 14/09/2026 Recover the whole photo arrangement.
		if(action==9) {
			x.layoutAnchor=xrWorkspaceHeading(views);
			x.layout.applyTabletopPreset();
			for(int i=1;i<3;++i) {x.surfaces[i]=x.layout.relative[i];x.surfaces[i].pose=xrPoseMul(x.layoutAnchor,x.surfaces[i].pose);}
			if(XrGameBoot_CanStereoWorld())xrRequestWorldView(x,true);
			x.menu.open=false;x.controlsArmed=false;x.grab.cancel();
		}
		if(action==10)x.layout.resolutionTier=(x.layout.resolutionTier+1)%3;
		// GeneralsX @feature Codex 14/09/2026 XR text switches immediately;
		// native game strings are validated and staged for a clean restart.
		if(action==11) {
			x.layout.language=x.layout.language==XrLanguage::German ? XrLanguage::English:XrLanguage::German;
			g_xrLanguage=x.layout.language;XrGameBoot_SetLanguage(int(x.layout.language));
		}
		x.layoutDirty=true;saveLayout(x);return;
	}
	auto &s=x.surfaces[x.menu.target];
	const auto head=xrScale(xrAdd(views[0].pose.position,views[1].pose.position),.5f);
	auto away=xrSub(s.pose.position,head);const float distance=xrLength(away);
	if(distance>.01f) away=xrScale(away,1/distance);
	switch(action) {
	case 0:x.menu.target=x.splitVisible ? 1:0;break;
	case 1:if(x.splitVisible) x.menu.target=2;break;
	case 2:case 3:s.width=std::clamp(s.width*(action==2 ? .9f:1.1f),.45f,x.menu.target==1 ? 4.0f:2.5f);break;
	case 4:case 5:if(action==5 || distance>.35f) s.pose.position=xrAdd(s.pose.position,xrScale(away,action==4 ? -.1f:.1f));break;
	case 6:case 7:s.pose.position.y+=action==6 ? .08f:-.08f;break;
	case 8:case 9:s.pose.orientation=xrMul(s.pose.orientation,xrAxisAngle({1,0,0},action==8 ? -.174533f:.174533f));x.layout.snap[x.menu.target]=false;break;
	case 10:case 11:s.pose.orientation=xrMul(xrAxisAngle({0,1,0},action==10 ? .174533f:-.174533f),s.pose.orientation);break;
	case 12:case 13:if(XrGameBoot_CanAdjustWorld()) x.worldZoom=std::clamp(x.worldZoom*(action==12 ? 1.2f:1/1.2f),.5f,3.0f);break;
	case 14:x.menu.open=false;x.arranging=true;x.arrangeSlot=x.menu.target==2 ? 2:1;x.grab.cancel();x.controlsArmed=false;break;
	case 15:{float fx=0,fz=-1;yawForwardFromQuat(views[0].pose.orientation,&fx,&fz);
		const XrPosef anchor={xrAxisAngle({0,1,0},atan2f(-fx,-fz)),head};XrLayout defaults;
		s.pose=xrPoseMul(anchor,defaults.relative[x.menu.target].pose);break;}
	case 16:x.menu.page=5;x.scene.step=XrScene::Step::Choice;return;
	case 17:x.menu.open=false;x.controlsArmed=false;break;
	}
	// Same recoverable placement bounds as controller arrangement.
	auto offset=xrSub(s.pose.position,x.layoutAnchor.position);
	if(xrLength(offset)>4.5f) s.pose.position=xrAdd(x.layoutAnchor.position,xrScale(offset,4.5f/xrLength(offset)));
	x.layoutDirty=true;saveLayout(x);
}
static bool updateXrMenu(XrHello &x,const XrControllerState &c,const XrView *views,XrTime time) {
	const bool tracked=x.state==XR_SESSION_STATE_FOCUSED && c.aimValid;
	if(!tracked) {x.menu.click.cancel();x.grab.cancel();x.controlsArmed=false;}
	int hit=-1;float nearest=10;XrVector3f endpoint=xrAdd(c.aim.position,xrRotate(c.aim.orientation,{0,0,-2.5f}));
	if(tracked && x.panelLatched) for(int piece=0;piece<(x.menu.open ? 3:2);++piece) {
		if(piece==1 && !groundButtonAvailable(x))continue;
		const auto s=piece==0 ? uiButtonSurface(x):piece==1 ? groundButtonSurface(x):x.menu.surface;
		const float aspect=piece<2 ? 128.0f/192:float(kXrMenuHeight)/kXrMenuWidth;
		float m[16],u=0,v=0;surfaceMatrix(s,m);if(!panelRayUV(m,aspect,c.aim,&u,&v)) continue;
		const auto point=xrAdd(s.pose.position,xrRotate(s.pose.orientation,{(u-.5f)*s.width,(v-.5f)*s.width*aspect,0}));
		const float distance=xrLength(xrSub(point,c.aim.position));
		if(distance<nearest) {nearest=distance;endpoint=point;hit=piece==0 ? 100:piece==1 ? 101:
			(x.menu.page==4 ? xrCommandHit(u,v,true):x.menu.page==5 ? xrSceneMenuHit(u,v):xrMenuHit(u,v,x.menu.page));}
	}
	const bool captured=x.menu.open || hit==100 || hit==101;
	const bool fire=x.menu.update(c.select,hit,tracked);x.menu.hover=hit;
	if(!captured) return false;
	updateControls(x,XrControllerState{},time);
	x.rayVisible=tracked;x.rayStart=c.aim.position;x.rayEnd=endpoint;x.rayHit=hit>=0;
	x.pointerPressed=hit>=0 && c.select;x.hoverVisible=false;
	if(x.menu.open && c.back) {finishArrangement(x);return true;}
	if(fire && hit==100) {
		if(x.arranging) {finishArrangement(x);return true;}
		x.menu.open=!x.menu.open;x.controlsArmed=false;
		if(x.menu.open) {
			x.menu.page=0; // Always expose the direct workspace recovery action.
			x.menu.target=x.splitVisible ? 1:0;
			float fx=0,fz=-1;yawForwardFromQuat(views[0].pose.orientation,&fx,&fz);
			x.menu.surface.pose.orientation=xrAxisAngle({0,1,0},atan2f(-fx,-fz));
			x.menu.surface.pose.position=xrAdd(xrScale(xrAdd(views[0].pose.position,views[1].pose.position),.5f),{fx*.9f,-.16f,fz*.9f});
			x.menu.surface.width=.64f;
		}
	} else if(fire && hit==101) armGroundView(x);
	else if(fire && hit>=0) applyMenuAction(x,hit,views);
	return true;
}
