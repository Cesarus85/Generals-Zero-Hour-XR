// GeneralsX @feature Codex 13/09/2026 Versioned relative layouts, not room anchors.
#pragma once
#include "XrPlacement.h"
#include "XrStrings.h"
#include <cstdio>
#include <string>

struct XrLayout {
	XrSurface relative[3]; // composed screen, board, detached UI; relative to launch heading
	float worldZoom=1;
	bool startStereo=true,healthBars=true,unitRings=true,boardFrame=true;
	bool commandsVisible=true,leftHanded=false;
	// GeneralsX @feature Codex 14/09/2026 P15 agreed first-run quality.
	// 0 Balanced, 1 High, 2 Ultra+ (opt-in); old v7-v10 boolean values migrate unchanged.
	int resolutionTier=0;
	int formatVersion=11;
	XrLanguage language=XrLanguage::German;
	// GeneralsX @feature Codex 14/09/2026 Only a fresh layout adopts the OS
	// preference. Even migrated old layouts retain their previous language.
	void initializeLanguage(bool restored,int systemCode) {
		if(!restored)language=systemCode==0 ? XrLanguage::German:XrLanguage::English;
	}
	// GeneralsX @bugfix Codex 13/09/2026 Windows stay at the user's chosen tilt.
	bool snap[3]={false,true,false};
	XrLayout() {
		applyFreeStandingStart();
	}
	// GeneralsX @safety Codex 15/09/2026 Spatial poses are deliberately not
	// trusted at process start. We do not own a persistent room anchor, so a
	// layout saved at home can be oversized or behind the player elsewhere.
	// Keep non-spatial preferences loaded from disk, but begin every XR session
	// with the complete, reachable presentation relative to the current HMD.
	void applyFreeStandingStart() {
		snap[0]=false;
		relative[0]={};relative[0].width=1.35f;relative[0].pose.position={0,-.02f,-1.1f};
		applyTabletopPreset();
	}
	// GeneralsX @feature Codex 14/09/2026 Photo-inspired defaults from the
	// user's measured v6 arrangement, rounded and centered on launch heading.
	void applyTabletopPreset() {
		relative[1]={};relative[2]={};
		relative[1].width=1.65f;relative[1].pose.position={0,-.54f,-.55f};
		relative[1].pose.orientation=xrAxisAngle({1,0,0},-1.57079633f);
		// GeneralsX @tweak Codex 14/09/2026 Requested 10 cm more table/UI clearance.
		relative[2].width=1.8f;relative[2].pose.position={0,-.38f,-1.18f};
		relative[2].pose.orientation=xrAxisAngle({1,0,0},-.41887902f);
		snap[1]=true;snap[2]=false;startStereo=true;commandsVisible=true;
	}
	bool upgradeDefaults() {
		if(formatVersion>=11)return false;
		if(formatVersion<7)applyTabletopPreset();
		else if(formatVersion<10) {
			// GeneralsX @tweak Codex 14/09/2026 Once only, away from launch heading,
			// not along the tilted panel normal. Preserve height, tilt and size.
			auto farther=relative[2].pose.position;farther.z-=.1f;
			if(xrLength(farther)<5)relative[2].pose.position=farther;
		}
		// P15 quality migration stays limited to pre-v9; deliberate v9 choices survive.
		if(formatVersion<9) {resolutionTier=0;startStereo=true;}
		formatVersion=11;return true;
	}
	bool load(const char *path) {
		FILE *f=fopen(path,"r"); if(!f) return false;
		XrLayout parsed;parsed.startStereo=false;int version=0;
		bool ok=fscanf(f,"GENERALS_XR_LAYOUT %d",&version)==1 && version>=1 && version<=11;
		for(int i=0;i<(version==1 ? 2:3) && ok;++i) {
			auto &s=parsed.relative[i]; auto &p=s.pose.position; auto &q=s.pose.orientation; int snapFlag=0;
			ok=fscanf(f,"%f %f %f %f %f %f %f %f %d",&s.width,&p.x,&p.y,&p.z,&q.x,&q.y,&q.z,&q.w,&snapFlag)==9;
			const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
			ok=ok && std::isfinite(s.width) && s.width>=.45f && s.width<=(i==1 ? 4.0f:2.5f) &&
				std::isfinite(xrLength(p)) && xrLength(p)<5 && std::isfinite(norm) && norm>.9f && norm<1.1f &&
				(snapFlag==0 || snapFlag==1);
			q=xrNormalize(q); parsed.snap[i]=snapFlag!=0;
		}
		if(ok && version>=3) ok=fscanf(f," %f",&parsed.worldZoom)==1 && std::isfinite(parsed.worldZoom) && parsed.worldZoom>=.5f && parsed.worldZoom<=3;
		if(ok && version>=4) {
			int a=0,b=0,c=0,d=0;ok=fscanf(f," %d %d %d %d",&a,&b,&c,&d)==4 && (a==0 || a==1) && (b==0 || b==1) && (c==0 || c==1) && (d==0 || d==1);
			parsed.startStereo=a;parsed.healthBars=b;parsed.unitRings=c;parsed.boardFrame=d;
		}
		if(ok && version>=5) {int visible=0;ok=fscanf(f," %d",&visible)==1 && (visible==0 || visible==1);parsed.commandsVisible=visible;}
		if(ok && version>=6) {int left=0;ok=fscanf(f," %d",&left)==1 && (left==0 || left==1);parsed.leftHanded=left;}
		if(ok && version>=7) {int tier=0;ok=fscanf(f," %d",&tier)==1 && tier>=0 && tier<=(version>=11 ? 2:1);parsed.resolutionTier=tier;}
		if(ok && version>=8) {int language=0;ok=fscanf(f," %d",&language)==1 && language>=0 && language<=1;parsed.language=static_cast<XrLanguage>(language);}
		parsed.formatVersion=version;
		fclose(f); if(ok) *this=parsed; return ok;
	}
	bool save(const char *path) const {
		if(!path || !*path) return false;
		const std::string temp=std::string(path)+".tmp";
		FILE *f=fopen(temp.c_str(),"w"); if(!f) return false;
		bool ok=fprintf(f,"GENERALS_XR_LAYOUT 11\n")>0;
		for(int i=0;i<3;++i) {
			const auto &s=relative[i];const auto &p=s.pose.position;const auto &q=s.pose.orientation;
			ok=(fprintf(f,"%.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d\n",s.width,p.x,p.y,p.z,q.x,q.y,q.z,q.w,(int)snap[i])>0)&&ok;
		}
		ok=(fprintf(f,"%.9g\n",worldZoom)>0)&&ok;
		ok=(fprintf(f,"%d %d %d %d\n",int(startStereo),int(healthBars),int(unitRings),int(boardFrame))>0)&&ok;
		ok=(fprintf(f,"%d\n",int(commandsVisible))>0)&&ok;
		ok=(fprintf(f,"%d\n",int(leftHanded))>0)&&ok;
		ok=(fprintf(f,"%d\n",resolutionTier)>0)&&ok;
		ok=(fprintf(f,"%d\n",int(language))>0)&&ok;
		ok=fclose(f)==0 && ok;
		return ok && rename(temp.c_str(),path)==0;
	}
};
