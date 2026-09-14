// GeneralsX @feature Codex 13/09/2026 Camera preset/profile validation regressions.
#include "XrCameraProfile.h"
#include <cstdlib>
static int checks=0;
static void check(bool b){++checks;if(!b){fprintf(stderr,"camera check %d failed\n",checks);exit(1);}}
static void near(float a,float b){check(std::isfinite(a) && fabsf(a-b)<.0001f);}
int main(int argc,char **argv)
{
	check(argc==2);const char *path=argv[1];
	near(xrCameraPitch(0,.6f),.6f);near(xrCameraPitch(1,0)/kXrCameraRadians,78);
	near(xrCameraPitch(2,0)/kXrCameraRadians,65);near(xrCameraPitch(3,0)/kXrCameraRadians,85);
	check(std::string(xrCameraName(1))=="TABLE 78");check(std::string(xrCameraName(4))=="FAVORITE");
	XrCameraProfile profile;check(profile.valid());
	profile.yaw=-1.2f;profile.pitch=72*kXrCameraRadians;profile.height=240;
	check(profile.save(path));XrCameraProfile loaded;check(loaded.load(path));
	near(loaded.yaw,profile.yaw);near(loaded.pitch,profile.pitch);near(loaded.height,profile.height);
	const char *bad[]={"GENERALS_XR_CAMERA 2\n0 1 300\n","GENERALS_XR_CAMERA 1\nnan 1 300\n",
		"GENERALS_XR_CAMERA 1\n0 1 -1\n","GENERALS_XR_CAMERA 1\n0 2 300\n",
		"GENERALS_XR_CAMERA 1\n0 1 300 trailing\n","GENERALS_XR_CAMERA 1\n0 1\n"};
	for(auto text:bad){FILE *f=fopen(path,"w");check(f!=nullptr);fputs(text,f);fclose(f);
		check(!loaded.load(path));near(loaded.height,240);near(loaded.yaw,-1.2f);}
	check(!profile.save(""));check(!profile.load(""));
	profile.pitch=90*kXrCameraRadians;check(!profile.valid());check(!profile.save(path));
	check(remove(path)==0);
	// Local differential ground foreshortening at the camera's target:
	// steepening from 65 to 78 improves it by ~8%, not a new depth dimension.
	const float before=sinf(65*kXrCameraRadians),after=sinf(78*kXrCameraRadians);
	check(after/before>1.07f && after/before<1.09f);
	printf("PASS %d camera profile checks; center-ground scale gain %.1f percent\n",checks,(after/before-1)*100);
}
