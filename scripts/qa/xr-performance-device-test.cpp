// GeneralsX @test Codex 14/09/2026 Real Adreno optional timer query dispatch.
// NDK clang++ -std=c++17 -Wall -Wextra -Werror -static-libstdc++
// -IGeneralsMD/Code/Main this-file.cpp -lEGL -lGLESv3 -o <test>
#include "XrGpuTimerGL.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
static int checks=0;
static void check(bool b){++checks;if(!b){fprintf(stderr,"FAIL GPU timer %d GL=%x EGL=%x\n",checks,glGetError(),eglGetError());exit(1);}}
int main(){
	EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);check(eglInitialize(d,nullptr,nullptr));
	const EGLint a[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};
	EGLConfig config;EGLint count=0;check(eglChooseConfig(d,a,&config,1,&count) && count==1);
	const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE},sa[]={EGL_WIDTH,256,EGL_HEIGHT,256,EGL_NONE};
	auto c=eglCreateContext(d,config,EGL_NO_CONTEXT,ca);auto s=eglCreatePbufferSurface(d,config,sa);
	check(c!=EGL_NO_CONTEXT && s!=EGL_NO_SURFACE);check(eglMakeCurrent(d,s,s,c));
	printf("GPU %s\n",glGetString(GL_RENDERER));XrGpuTimer timer;
	check(xrInitGpuTimer(timer));check(glGetError()==GL_NO_ERROR);
	unsigned samples=0;double ms=0;
	for(int frame=0;frame<80;++frame){
		auto r=timer.poll(1);samples+=r.count;ms+=r.totalMs;
		if(timer.begin(1)){
			glClearColor(float(frame%3)/2,.2f,.3f,1);glClear(GL_COLOR_BUFFER_BIT);timer.end();
		}
		glFlush();check(glGetError()==GL_NO_ERROR);usleep(2000); // Fixture only, not production.
	}
	check(samples>0 && ms>0);check(timer.begin(2));timer.end(false);
	glFlush();usleep(10000);check(timer.poll(3).count==0);check(glGetError()==GL_NO_ERROR);
	timer.shutdown();check(glGetError()==GL_NO_ERROR);
	eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroySurface(d,s);eglDestroyContext(d,c);eglTerminate(d);
	printf("PASS %d real GLES timer checks, %u valid samples, mean %.4f ms; no glFinish\n",checks,samples,ms/samples);
}
