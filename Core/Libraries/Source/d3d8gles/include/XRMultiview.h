// GeneralsX @performance Codex 14/09/2026 Optional current-context multiview API.
// The XR host supplies its EGL resolver; never mix ANGLE and system dispatch.
#pragma once
#include <GLES3/gl3.h>
#include <cstring>
struct GXMultiview {
	using Resolver=void *(*)(const char *);
	Resolver resolver=nullptr;
	void (GL_APIENTRY *storage)(GLenum,GLsizei,GLenum,GLsizei,GLsizei,GLsizei)=nullptr;
	void (GL_APIENTRY *layer)(GLenum,GLenum,GLuint,GLint,GLint)=nullptr;
	void (GL_APIENTRY *attach)(GLenum,GLenum,GLuint,GLint,GLint,GLsizei)=nullptr;
	bool checked=false,supported=false;
	bool available() {
		if(checked)return supported;
		if(!resolver)return false;
		checked=true;
		auto getStringi=reinterpret_cast<const GLubyte *(GL_APIENTRY *)(GLenum,GLuint)>(resolver("glGetStringi"));
		if(!getStringi)return false;
		GLint count=0;glGetIntegerv(GL_NUM_EXTENSIONS,&count);
		bool extension=false;
		for(GLint i=0;i<count;++i) {
			const auto *name=reinterpret_cast<const char *>(getStringi(GL_EXTENSIONS,i));
			extension=extension || (name && std::strcmp(name,"GL_OVR_multiview2")==0);
		}
		if(!extension)return false;
		GLint views=0;glGetIntegerv(0x9631 /* GL_MAX_VIEWS_OVR */,&views);
		storage=reinterpret_cast<decltype(storage)>(resolver("glTexStorage3D"));
		layer=reinterpret_cast<decltype(layer)>(resolver("glFramebufferTextureLayer"));
		attach=reinterpret_cast<decltype(attach)>(resolver("glFramebufferTextureMultiviewOVR"));
		return supported=views>=2 && storage && layer && attach;
	}
};
