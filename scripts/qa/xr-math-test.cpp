// GeneralsX @bugfix Codex 13/09/2026 Regressions for perceived scale and head motion.
#include "XrMath.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static int checks = 0;
static void near(float actual, float expected)
{
	++checks;
	if (!std::isfinite(actual) || fabsf(actual-expected) > 0.0001f) {
		fprintf(stderr, "check %d: got %.7f, expected %.7f\n", checks, actual, expected);
		exit(1);
	}
}
static void project(const float *m, float x, float y, float z, float *out)
{
	const float w = m[3]*x + m[7]*y + m[11]*z + m[15];
	for (int r=0; r<3; ++r) out[r]=(m[r]*x+m[4+r]*y+m[8+r]*z+m[12+r])/w;
}
int main()
{
	const XrFovf fovs[] = {{-0.78539816f,0.78539816f,0.78539816f,-0.78539816f},
	                       {-0.85f,0.65f,0.73f,-0.62f}};
	for (auto fov : fovs) for (float n : {0.01f,0.05f,0.2f}) {
		float p[16], v[3]; matPerspectiveFromFov(p,fov,n,100.0f);
		for (float distance : {0.3f,1.1f,10.0f}) {
			project(p,distance*tanf(fov.angleLeft),0,-distance,v); near(v[0],-1);
			project(p,distance*tanf(fov.angleRight),0,-distance,v); near(v[0],1);
			project(p,0,distance*tanf(fov.angleUp),-distance,v); near(v[1],1);
			project(p,0,distance*tanf(fov.angleDown),-distance,v); near(v[1],-1);
		}
		project(p,0,0,-n,v); near(v[2],-1);
		project(p,0,0,-100,v); near(v[2],1);
	}
	// Known physical screen subtends the right angular width regardless of nearZ.
	float p[16], v[3]; matPerspectiveFromFov(p,fovs[0],0.05f,100);
	project(p,0.675f,0,-1.1f,v); near(v[0],0.675f/1.1f);
	// Rigid inverse: world points generated independently for varied yaw and translation.
	for (float yaw : {-1.0f,-0.4f,0.0f,0.4f,1.0f}) {
		XrPosef pose={{0,sinf(yaw/2),0,cosf(yaw/2)},{0.3f,1.6f,-0.2f}};
		float view[16], pv[16]; matViewFromPose(view,pose); matMultiply(pv,p,view);
		const float x=0.3f-sinf(yaw)*1.1f, z=-0.2f-cosf(yaw)*1.1f;
		project(pv,x,1.6f,z,v); near(v[0],0); near(v[1],0);
		// Fixed world point under head yaw follows tan(yaw), not 0.05*tan(yaw).
		project(pv,0.3f,1.6f,-1.3f,v); near(v[0],tanf(yaw));
	}
	// Upright and horizontal picking share exactly the rendered panel transform.
	float t[16], s[16], r[16], m[16], tmp[16], u=0, uv=0;
	matTranslate(t,0,1.6f,-1.1f); matScale(s,1.35f,1.35f,1); matMultiply(m,t,s);
	XrPosef aim={{0,0,0,1},{0,1.6f,0}};
	near(panelRayUV(m,0.5625f,aim,&u,&uv),1); near(u,0.5f); near(uv,0.5f);
	aim.position.x=0.675f; aim.position.y=1.6f+1.35f*0.5625f*0.5f;
	near(panelRayUV(m,0.5625f,aim,&u,&uv),1); near(u,1); near(uv,1);
	aim.position.x=2; near(panelRayUV(m,0.5625f,aim,&u,&uv),0);
	matTranslate(t,0,1,-0.7f); matRotX(r,-1.57079632679f);
	matMultiply(tmp,r,s); matMultiply(m,t,tmp);
	aim={{-0.70710678f,0,0,0.70710678f},{0,1.6f,-0.7f}};
	near(panelRayUV(m,0.5625f,aim,&u,&uv),1); near(u,0.5f); near(uv,0.5f);
	aim.orientation={0,0,0,1}; near(panelRayUV(m,0.5625f,aim,&u,&uv),0);
	printf("XR geometry: %d checks passed (FOV edges, scale, depth, head yaw, panel picking)\n",checks);
}
