// GeneralsX @test Codex 13/09/2026 Render production P6 mesh/shaders on GLES.
// NDK clang++ -std=c++17 -Wall -Wextra -Werror -static-libstdc++
// -IGeneralsMD/Code/Main -I<openxr-include> this-file.cpp -lEGL -lGLESv3 -o <test>
// Optional argument: new PPM output path (stereo pair). No OpenXR runtime needed.
#include "XrDiorama.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstdio>
#include <cstdlib>
static int checks=0;
static void check(bool b,const char *why) {++checks;if(!b){fprintf(stderr,"FAIL %s GL=%x EGL=%x\n",why,glGetError(),eglGetError());exit(1);}}
static GLuint shader(GLenum type,const char *src) {
	const GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);
	GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
	if(!ok){char log[2048];glGetShaderInfoLog(s,sizeof(log),nullptr,log);fprintf(stderr,"%s\n",log);}
	check(ok,"production shader");return s;
}
int main(int argc,char **argv) {
	EGLDisplay display=eglGetDisplay(EGL_DEFAULT_DISPLAY);check(display!=EGL_NO_DISPLAY,"display");
	check(eglInitialize(display,nullptr,nullptr),"initialize");
	const EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
		EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
	EGLConfig cfg;EGLint count=0;check(eglChooseConfig(display,attrs,&cfg,1,&count)&&count==1,"config");
	const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE},pa[]={EGL_WIDTH,2,EGL_HEIGHT,2,EGL_NONE};
	EGLContext ctx=eglCreateContext(display,cfg,EGL_NO_CONTEXT,ca);check(ctx!=EGL_NO_CONTEXT,"context");
	EGLSurface surface=eglCreatePbufferSurface(display,cfg,pa);check(surface!=EGL_NO_SURFACE,"surface");
	check(eglMakeCurrent(display,surface,surface,ctx),"current");
	printf("GPU %s\n",glGetString(GL_RENDERER));
	GLuint vs=shader(GL_VERTEX_SHADER,kXrDioramaVert),fs=shader(GL_FRAGMENT_SHADER,kXrDioramaFrag);
	GLuint program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);
	GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);check(ok,"link");
	glUseProgram(program);const GLint uniform=glGetUniformLocation(program,"uMVP");check(uniform>=0,"MVP");
	const auto mesh=xrBuildDiorama();GLuint vao,vbo;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
	glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
	glBufferData(GL_ARRAY_BUFFER,mesh.vertices.size()*sizeof(XrDioramaVertex),mesh.vertices.data(),GL_STATIC_DRAW);
	for(GLuint i=0;i<3;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,3,GL_FLOAT,GL_FALSE,sizeof(XrDioramaVertex),reinterpret_cast<const void *>(i*sizeof(XrVector3f)));}
	const int size=600;GLuint fbo,tex,depth;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
	glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,size,size,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
	glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,size,size);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"eye-like FBO");
	glViewport(0,0,size,size);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
	std::vector<unsigned char> pixels[3];
	for(int pass=0;pass<3;++pass) {
		XrSurface board;board.width=1;board.pose={xrAxisAngle({1,0,0},-1.570796327f),{0,-.45f,-.7f}};
		const XrPosef eye={xrAxisAngle({1,0,0},-.55f),{pass==1 ? .032f:-.032f,0,0}};
		float m[16],v[16],p[16],vm[16],mvp[16];xrDioramaMatrix(board,m);matViewFromPose(v,eye);
		matPerspectiveFromFov(p,{-.70f,.70f,.70f,-.70f},.05f,100);matMultiply(vm,v,m);matMultiply(mvp,p,vm);
		glUniformMatrix4fv(uniform,1,GL_FALSE,mvp);glClearColor(.014f,.021f,.026f,0);glClearDepthf(1);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		if(pass==2) glDisable(GL_DEPTH_TEST);
		glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(mesh.vertices.size()));
		pixels[pass].resize(size*size*4);glReadPixels(0,0,size,size,GL_RGBA,GL_UNSIGNED_BYTE,pixels[pass].data());
		check(glGetError()==GL_NO_ERROR,"draw and readback");
		int covered=0;for(int i=3;i<size*size*4;i+=4) covered+=pixels[pass][i]==255;
		check(covered>size*size/8 && covered<size*size*3/4,"bounded visible scene");
	}
	int stereoDifference=0,depthDifference=0;
	for(int i=0;i<size*size*4;i+=4) {
		if(std::abs(int(pixels[0][i])-int(pixels[1][i]))>10) ++stereoDifference;
		if(std::abs(int(pixels[0][i])-int(pixels[2][i]))>10) ++depthDifference;
	}
	check(stereoDifference>1000,"distinct left/right view");check(depthDifference>1000,"depth test changes occlusion");
	if(argc==2) {
		FILE *f=fopen(argv[1],"wb");check(f!=nullptr,"open new PPM");
		check(fprintf(f,"P6\n%d %d\n255\n",size*2,size)>0,"PPM header");
		bool written=true;
		for(int y=size-1;y>=0;--y) for(int eye=0;eye<2;++eye) for(int x=0;x<size;++x)
			written=(fwrite(&pixels[eye][(y*size+x)*4],1,3,f)==3) && written;
		check(written,"PPM pixels");check(fclose(f)==0,"PPM close");
	}
	glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);glDeleteTextures(1,&tex);
	glDeleteRenderbuffers(1,&depth);glDeleteFramebuffers(1,&fbo);glDeleteProgram(program);glDeleteShader(vs);glDeleteShader(fs);
	eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroySurface(display,surface);eglDestroyContext(display,ctx);eglTerminate(display);
	printf("PASS %d P6 hardware checks; %d stereo pixels, %d depth-sensitive pixels\n",checks,stereoDifference,depthDifference);
}
