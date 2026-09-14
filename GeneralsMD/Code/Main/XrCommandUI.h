// GeneralsX @feature Codex 13/09/2026 Nonmodal army console beside the HUD.
#pragma once
static bool commandsAvailable(const XrHello &x) {
	return x.interactiveGame && x.splitVisible && !x.arranging && !x.diorama && !x.menu.open && !XrGameBoot_ExpandedUI() && XrGameBoot_CanStereoWorld();
}
static XrSurface commandButtonSurface(const XrHello &x) {
	auto s=uiButtonSurface(x);s.pose.position=xrAdd(s.pose.position,xrRotate(s.pose.orientation,{.18f,0,0}));s.width=.20f;return s;
}
static XrSurface commandSurface(const XrHello &x) {
	auto s=x.surfaces[2];const float width=.72f;
	// GeneralsX @tweak Codex 14/09/2026 Left console faces inward toward player.
	s.pose.position=xrAdd(s.pose.position,xrRotate(s.pose.orientation,{-(s.width+width)*.5f-.08f,.22f,.10f}));
	// GeneralsX @bugfix Codex 14/09/2026 Dock position follows the build bar,
	// but console gravity does not. Strip pitch AND roll before inward yaw.
	float fx=0,fz=-1;yawForwardFromQuat(s.pose.orientation,&fx,&fz);
	s.pose.orientation=xrAxisAngle({0,1,0},atan2f(-fx,-fz)+.78539816f);
	s.width=width;
	s.pose.position=xrAdd(s.pose.position,xrRotate(s.pose.orientation,{0,-width*(xrCommandHeight(x.commands)-1024)/1536.0f,0}));
	return s;
}
static void applyCommandAction(XrHello &x,int hit) {
	if(hit==37){x.commands.tactics=!x.commands.tactics;x.commands.bookmarkSave=false;x.commands.groupOperation=0;return;}
	if(hit>=40 && hit<=47) {
		if(!x.commands.tactics || x.commands.help || !XrGameBoot_CanAdjustWorld())return;
		if(hit==43){x.commands.bookmarkSave=!x.commands.bookmarkSave;x.commands.groupOperation=0;return;}
		if(hit>=44){XrGameBoot_Bookmark(hit-44,x.commands.bookmarkSave);x.commands.bookmarkSave=false;return;}
		if(!XrGameBoot_TacticalReason(hit).empty())return;
		x.commands.bookmarkSave=false;x.commands.groupOperation=0;x.stereoWorld=true;XrGameBoot_TacticalAction(hit);return;
	}
	x.commands.bookmarkSave=false;
	if(hit==33) {x.commands.groupOperation=0;x.layout.commandsVisible=false;x.layoutDirty=true;saveLayout(x);return;}
	if(hit==34){x.commands.help=!x.commands.help;x.commands.groupOperation=0;return;}
	if(hit==36 && x.commands.help){x.commands.helpPage=(x.commands.helpPage+1)%4;return;}
	if(!XrGameBoot_CanAdjustWorld())return;
	if(hit==35){x.commands.groupOperation=0;XrGameBoot_Communicator();return;}
	if(hit>=30 && hit<=32) {x.commands.groupOperation=x.commands.groupOperation==hit-29 ? 0:hit-29;return;}
	if(hit>=20 && hit<30) {
		XrGameBoot_TacticalGroup(hit-20,x.commands.groupOperation);x.commands.groupOperation=0;return;
	}
	const int action=xrCommandAction(hit);if(action<0)return;
	x.commands.groupOperation=0;
	if(action<=9)x.stereoWorld=true;
	XrGameBoot_TacticalAction(action);
}
static bool updateCommands(XrHello &x,const XrControllerState &c,XrTime time) {
	const bool shown=commandsAvailable(x),tracked=shown && x.panelLatched && c.aimValid && x.state==XR_SESSION_STATE_FOCUSED;
	if(!shown){x.commands.groupOperation=0;x.commands.bookmarkSave=false;}
	if(c.back) {x.commands.input.update(c.select,-1,false);return false;} // B still pauses over the console.
	int hit=-1;bool surfaceHit=false;float nearest=10;XrVector3f endpoint={};
	if(tracked)for(int piece=0;piece<(x.layout.commandsVisible ? 2:1);++piece) {
		const auto s=piece==0 ? commandButtonSurface(x):commandSurface(x);
		const float aspect=piece==0 ? 128.0f/192:float(xrCommandHeight(x.commands))/768;
		float m[16],u=0,v=0;surfaceMatrix(s,m);if(!panelRayUV(m,aspect,c.aim,&u,&v))continue;
		const auto point=xrAdd(s.pose.position,xrRotate(s.pose.orientation,{(u-.5f)*s.width,(v-.5f)*s.width*aspect,0}));
		const float distance=xrLength(xrSub(point,c.aim.position));
		if(distance<nearest){nearest=distance;surfaceHit=true;endpoint=point;hit=piece==0 ? 100:xrCommandHit(u,v,x.commands.help,x.commands.tactics);}
	}
	const bool fire=x.commands.input.update(c.select,hit,tracked);x.commands.input.hover=hit;
	if(!surfaceHit)return false;
	updateControls(x,XrControllerState{},time);
	x.rayVisible=true;x.rayStart=c.aim.position;x.rayEnd=endpoint;x.rayHit=hit>=0;x.hoverVisible=false;
	if(fire) {
		if(hit==100){x.commands.groupOperation=0;x.layout.commandsVisible=!x.layout.commandsVisible;x.layoutDirty=true;saveLayout(x);}
		else applyCommandAction(x,hit);
	}
	return true;
}
