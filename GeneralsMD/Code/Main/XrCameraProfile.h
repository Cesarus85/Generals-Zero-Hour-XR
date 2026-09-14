// GeneralsX @feature Codex 13/09/2026 Reproducible camera comparison and personal default.
#pragma once
#include <cmath>
#include <cstdio>
#include <string>

constexpr int kXrCameraPresetCount=4;
constexpr int kXrCameraFavorite=4;
constexpr float kXrCameraRadians=0.01745329252f;
inline float xrCameraPitch(int preset,float classic) {
	return preset==1 ? 78*kXrCameraRadians : preset==2 ? 65*kXrCameraRadians :
		preset==3 ? 85*kXrCameraRadians : classic;
}
inline const char *xrCameraName(int preset) {
	return preset==1 ? "TABLE 78" : preset==2 ? "OBLIQUE 65" : preset==3 ? "TOP 85" :
		preset==kXrCameraFavorite ? "FAVORITE" : "CLASSIC";
}
struct XrCameraProfile {
	float yaw=0,pitch=78*kXrCameraRadians,height=300;
	bool valid() const {
		return std::isfinite(yaw) && fabsf(yaw)<=3.141593f && std::isfinite(pitch) &&
			pitch>=.1f*kXrCameraRadians && pitch<=89.9f*kXrCameraRadians &&
			std::isfinite(height) && height>=1 && height<=10000;
	}
	bool load(const char *path) {
		if(!path || !*path) return false;
		FILE *f=fopen(path,"r"); if(!f) return false;
		XrCameraProfile candidate; int version=0; char extra=0;
		const bool ok=fscanf(f,"GENERALS_XR_CAMERA %d %f %f %f",&version,&candidate.yaw,&candidate.pitch,&candidate.height)==4 &&
			version==1 && candidate.valid() && fscanf(f," %c",&extra)==EOF;
		fclose(f); if(ok) *this=candidate; return ok;
	}
	bool save(const char *path) const {
		if(!path || !*path || !valid()) return false;
		const auto temp=std::string(path)+".tmp";
		FILE *f=fopen(temp.c_str(),"w"); if(!f) return false;
		const bool written=fprintf(f,"GENERALS_XR_CAMERA 1\n%.9g %.9g %.9g\n",yaw,pitch,height)>0;
		const bool closed=fclose(f)==0;
		return written && closed && rename(temp.c_str(),path)==0;
	}
};
