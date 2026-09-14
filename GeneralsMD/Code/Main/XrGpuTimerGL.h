// GeneralsX @performance Codex 14/09/2026 Optional, context-local EGL dispatch.
// Do not use the backend's interposed gl* wrappers from the OpenXR host.
#pragma once
#include "XrGpuTimer.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstring>
inline bool xrInitGpuTimer(XrGpuTimer &timer) {
	auto integer=reinterpret_cast<PFNGLGETINTEGERVPROC>(eglGetProcAddress("glGetIntegerv"));
	auto stringi=reinterpret_cast<PFNGLGETSTRINGIPROC>(eglGetProcAddress("glGetStringi"));
	if(!integer || !stringi)return false;
	int count=0;integer(GL_NUM_EXTENSIONS,&count);bool extension=false;
	for(int i=0;i<count;++i){const auto *s=stringi(GL_EXTENSIONS,unsigned(i));
		if(s && !strcmp(reinterpret_cast<const char *>(s),"GL_EXT_disjoint_timer_query"))extension=true;}
	if(!extension)return false;
	XrGpuTimerApi api;api.integer=integer;
#define XR_TIMER_PROC(member,name) api.member=reinterpret_cast<decltype(api.member)>(eglGetProcAddress(name))
	XR_TIMER_PROC(gen,"glGenQueriesEXT");XR_TIMER_PROC(destroy,"glDeleteQueriesEXT");
	XR_TIMER_PROC(begin,"glBeginQueryEXT");XR_TIMER_PROC(end,"glEndQueryEXT");
	XR_TIMER_PROC(query,"glGetQueryivEXT");XR_TIMER_PROC(available,"glGetQueryObjectuivEXT");
	XR_TIMER_PROC(result,"glGetQueryObjectui64vEXT");
#undef XR_TIMER_PROC
	return timer.init(api);
}
