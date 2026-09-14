// GeneralsX @test Codex 13/09/2026 Standalone GLES3 hardware test for the P5 MRT contract.
// Android NDK: clang++ this-file.cpp -lEGL -lGLESv3 -o xr-mrt-device-test
// This checks GPU copy/blend/draw-buffer semantics, not W3D's UI boundary or headset UX.
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "XrColor.h"
#include <initializer_list>
static int checks=0;
static void check(bool b,const char *why) {++checks;if(!b){fprintf(stderr,"FAIL %s (GL=%x EGL=%x)\n",why,glGetError(),eglGetError());exit(1);}}
static GLuint shader(GLenum type,const char *src) {
	GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);
	GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);check(ok,"compile");return s;
}
static void pixel(GLuint fbo,GLenum attachment,float r,float g,float b,float a) {
	glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);glReadBuffer(attachment);
	unsigned char p[4]={};glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);
	const float expected[]={r,g,b,a};
	for(int i=0;i<4;++i) check(std::abs(int(p[i])-int(std::lround(expected[i]*255)))<=2,"pixel channel");
}
int main() {
	EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);check(d!=EGL_NO_DISPLAY,"display");
	check(eglInitialize(d,nullptr,nullptr),"initialize");
	EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
		EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
	EGLConfig cfg;EGLint count=0;check(eglChooseConfig(d,attrs,&cfg,1,&count)&&count==1,"config");
	EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE},pa[]={EGL_WIDTH,2,EGL_HEIGHT,2,EGL_NONE};
	EGLContext ctx=eglCreateContext(d,cfg,EGL_NO_CONTEXT,ca);check(ctx!=EGL_NO_CONTEXT,"context");
	EGLSurface surface=eglCreatePbufferSurface(d,cfg,pa);check(surface!=EGL_NO_SURFACE,"pbuffer");
	check(eglMakeCurrent(d,surface,surface,ctx),"current");
	printf("GPU %s; GLES %s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
	GLuint tex[3],fbo[2];glGenTextures(3,tex);glGenFramebuffers(2,fbo);
	for(int i=0;i<3;++i) {glBindTexture(GL_TEXTURE_2D,tex[i]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);}
	glBindFramebuffer(GL_FRAMEBUFFER,fbo[0]);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex[0],0);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,tex[1],0);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"MRT framebuffer");
	glClearColor(.2f,.4f,.6f,1);glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER,fbo[1]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex[2],0);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"world framebuffer");
	glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo[0]);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo[1]);
	glBlitFramebuffer(0,0,2,2,0,0,2,2,GL_COLOR_BUFFER_BIT,GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER,fbo[0]);
	const GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,buffers);
	const float clear[]={0,0,0,0};glClearBufferfv(GL_COLOR,1,clear);
	GLuint vs=shader(GL_VERTEX_SHADER,"#version 300 es\nvoid main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));gl_Position=vec4(p*2.0-1.0,0,1);}");
	GLuint fs=shader(GL_FRAGMENT_SHADER,"#version 300 es\nprecision mediump float;uniform vec4 color;layout(location=0)out vec4 composed;layout(location=1)out vec4 ui;void main(){composed=color;ui=color;}");
	GLuint prog=glCreateProgram();glAttachShader(prog,vs);glAttachShader(prog,fs);glLinkProgram(prog);
	GLint ok=0;glGetProgramiv(prog,GL_LINK_STATUS,&ok);check(ok,"link MRT");
	glUseProgram(prog);GLint color=glGetUniformLocation(prog,"color");glViewport(0,0,2,2);
	glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
	glUniform4f(color,1,0,0,.5f);glDrawArrays(GL_TRIANGLES,0,3);
	glUniform4f(color,0,1,0,.5f);glDrawArrays(GL_TRIANGLES,0,3);
	pixel(fbo[0],GL_COLOR_ATTACHMENT0,.30f,.60f,.15f,1);
	pixel(fbo[0],GL_COLOR_ATTACHMENT1,.25f,.50f,0,.75f);
	pixel(fbo[1],GL_COLOR_ATTACHMENT0,.2f,.4f,.6f,1);
	// Present/reset must stop future world/clear writes from contaminating the UI.
	glBindFramebuffer(GL_FRAMEBUFFER,fbo[0]);glDrawBuffers(1,buffers);
	glDisable(GL_BLEND);glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);
	pixel(fbo[0],GL_COLOR_ATTACHMENT0,0,0,1,1);
	pixel(fbo[0],GL_COLOR_ATTACHMENT1,.25f,.50f,0,.75f);
	check(glGetError()==GL_NO_ERROR,"no GL errors");
	// P5.1: reproduce the actual legacy-RGBA8 -> sRGB-eye transfer with known
	// display values, then apply the same GLSL decoder used by the XR shader.
	GLuint transferFS=shader(GL_FRAGMENT_SHADER,"#version 300 es\nprecision highp float;"
		"uniform sampler2D image;uniform int decode;out vec4 result;"
		XR_DISPLAY_TO_LINEAR_GLSL
		"void main(){vec4 c=texture(image,vec2(0.5));result=vec4(decode!=0 ? xrDisplayToLinear(c.rgb):c.rgb,c.a);}");
	GLuint transfer=glCreateProgram();glAttachShader(transfer,vs);glAttachShader(transfer,transferFS);glLinkProgram(transfer);
	glGetProgramiv(transfer,GL_LINK_STATUS,&ok);check(ok,"link transfer");
	GLuint source,destination;glGenTextures(1,&source);glGenTextures(1,&destination);
	glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,source);
	const unsigned char gray[]={128,128,128,255};
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,gray);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D,destination);glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
	glBindFramebuffer(GL_FRAMEBUFFER,fbo[1]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,destination,0);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"sRGB eye framebuffer");
	glBindTexture(GL_TEXTURE_2D,source);glUseProgram(transfer);
	glUniform1i(glGetUniformLocation(transfer,"image"),0);
	GLint decode=glGetUniformLocation(transfer,"decode");
	glUniform1i(decode,0);glDrawArrays(GL_TRIANGLES,0,3);
	pixel(fbo[1],GL_COLOR_ATTACHMENT0,188.0f/255,188.0f/255,188.0f/255,1);
	glUniform1i(decode,1);glDrawArrays(GL_TRIANGLES,0,3);
	pixel(fbo[1],GL_COLOR_ATTACHMENT0,128.0f/255,128.0f/255,128.0f/255,1);
	printf("P5.1 gamma reproduction: source gray 128 -> old output ~188 -> corrected output ~128\n");
	for(int value : {0,10,32,64,192,230,255}) {
		const unsigned char p[]={static_cast<unsigned char>(value),static_cast<unsigned char>(value),static_cast<unsigned char>(value),255};
		glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);glDrawArrays(GL_TRIANGLES,0,3);
		pixel(fbo[1],GL_COLOR_ATTACHMENT0,float(value)/255,float(value)/255,float(value)/255,1);
	}
	check(glGetError()==GL_NO_ERROR,"no color transfer errors");
	glDeleteProgram(transfer);glDeleteShader(transferFS);glDeleteTextures(1,&source);glDeleteTextures(1,&destination);
	glDeleteProgram(prog);glDeleteShader(vs);glDeleteShader(fs);glDeleteTextures(3,tex);glDeleteFramebuffers(2,fbo);
	eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroySurface(d,surface);eglDestroyContext(d,ctx);eglTerminate(d);
	printf("PASS %d hardware MRT checks\n",checks);
}
