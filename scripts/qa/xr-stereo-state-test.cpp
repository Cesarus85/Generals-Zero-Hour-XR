// GeneralsX @test Codex 14/09/2026 Actual drawCommon/fixed-state on real GLES.
// Shader/resource setup is a fixture; draw dispatch, eligibility, restoration,
// D3D conversion and FixedStateKey are extracted verbatim from production.
#include <d3d8.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <chrono>
#include "XRStereoPolicy.h"
#include "XRStereoRestore.h"
#include "XrColor.h"
#include "XRMultiview.h"
#include "XRWorldElision.h"
#include "quad-production.inc"
static int checks=0;
static void check(bool b,const char *why){++checks;if(!b){fprintf(stderr,"FAIL %d %s GL=%x\n",checks,why,glGetError());exit(1);}}
#define WARN_ONCE(id, ...) do {fprintf(stderr,__VA_ARGS__);exit(1);} while(0)
#include "state-helpers.inc"
struct WebGLDevice {
	DWORD state[256]={};D3DVIEWPORT8 vp{5,9,50,42,.15f,.85f};D3DMATRIX transforms[512]={};
	DWORD getRenderState(unsigned i)const{return state[i];}
	const D3DVIEWPORT8 &getViewport()const{return vp;}
	const D3DMATRIX &getTransform(unsigned i)const{return transforms[i];}
};
struct FVFLayout{bool xyzrhw=false;unsigned stride=12;};
static bool parseFVF(unsigned fvf,FVFLayout *l){l->xyzrhw=fvf==D3DFVF_XYZRHW;return true;}
struct WebGLPipeline {
#include "state-key.inc"
	struct ProgramInfo {
		GLuint prog=0;GLint uXrActive=-1,uXrEyeClip=-1,uXrBoard=-1,uXrAspect=-1,uXrOpaque=-1,uXrCamera=-1,uXrViewSpace=-1;
	} program,multiviewProgram;
	bool m_haveFixedStateKey=false,m_xrUI=false,m_xrStereoActive=true,m_xrStereoCoverage=true;
	bool m_xrCaptureEnabled=false;
	bool m_xrStereoAtlas=false,m_xrStereoMultiview=false,m_xrMultiviewFailed=false;
	GLuint m_xrMultiviewFBO=0;
	GXWorldElision m_xrElision;
	GLuint m_xrWorldFBO=1,m_xrUITex=1;
	FixedStateKey m_lastFixedStateKey{};
	int m_perfStateCacheHits=0,m_perfStateCacheMisses=0,m_curRTHeight=64,m_xrStereoW=64,m_xrStereoH=64;
	GLuint m_curFBO=0,m_offFBO=0,m_xrStereoFBO[2]={};
	unsigned m_xrStereoDraws=0,m_xrStereoProbeWait=0,m_xrShadowDraws=0,m_xrEffectDraws=0,m_xrTerrainDraws=0,m_xrModelDraws=0;
	unsigned m_xrRestores=0,m_xrRestoreCalls=0,m_perfDrawsThisFrame=0;
	float m_xrCamera[16]={},m_xrBoard[16]={},m_xrEyeClip[2][16]={},m_xrStereoAspect=1;
	ProgramInfo *getProgram(WebGLDevice *,unsigned,bool multiview=false){return multiview ? &multiviewProgram:&program;}
	void applyUniforms(WebGLDevice *,ProgramInfo *p,unsigned){glUseProgram(p->prog);}
	void bindTextures(WebGLDevice *,ProgramInfo *){}
	void bindVertexLayout(const FVFLayout &,GLuint,GLuint,unsigned,unsigned,int){}
	void applyFixedState(WebGLDevice *);
	void drawCommon(WebGLDevice *,unsigned,unsigned,GLuint,unsigned,unsigned,GLuint,unsigned,unsigned,int,unsigned);
	void drawReference(WebGLDevice *,unsigned,unsigned,GLuint,unsigned,unsigned,GLuint,unsigned,unsigned,int,unsigned);
};
static unsigned actualDrawCalls=0;
static void countedArrays(GLenum mode,GLint first,GLsizei count) {++actualDrawCalls;glDrawArrays(mode,first,count);}
static void countedElements(GLenum mode,GLsizei count,GLenum type,const void *indices) {++actualDrawCalls;glDrawElements(mode,count,type,indices);}
#define glDrawArrays countedArrays
#define glDrawElements countedElements
#include "state-production.inc"
#undef glDrawArrays
#undef glDrawElements
static GLuint shader(GLenum type,const char *s){
	GLuint id=glCreateShader(type);glShaderSource(id,1,&s,nullptr);glCompileShader(id);GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);check(ok,"shader");return id;
}
static std::vector<GLint> snapshot(){
	const GLenum names[]={GL_DEPTH_TEST,GL_DEPTH_WRITEMASK,GL_DEPTH_FUNC,GL_BLEND,GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,
		GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,GL_CULL_FACE,GL_CULL_FACE_MODE,GL_POLYGON_OFFSET_FILL,GL_STENCIL_TEST,
		GL_STENCIL_FUNC,GL_STENCIL_REF,GL_STENCIL_VALUE_MASK,GL_STENCIL_FAIL,GL_STENCIL_PASS_DEPTH_FAIL,
		GL_STENCIL_PASS_DEPTH_PASS,GL_STENCIL_WRITEMASK,GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_FRAMEBUFFER_BINDING,GL_SCISSOR_TEST};
	std::vector<GLint> result;for(auto name:names){GLint v=0;glGetIntegerv(name,&v);result.push_back(v);}
	GLint v[4]={};glGetIntegerv(GL_VIEWPORT,v);result.insert(result.end(),v,v+4);
	glGetIntegerv(GL_COLOR_WRITEMASK,v);result.insert(result.end(),v,v+4);
	GLfloat f[2];glGetFloatv(GL_DEPTH_RANGE,f);for(float x:f)result.push_back(GLint(x*100000));
	glGetFloatv(GL_POLYGON_OFFSET_FACTOR,f);result.push_back(GLint(f[0]*100));
	glGetFloatv(GL_POLYGON_OFFSET_UNITS,f);result.push_back(GLint(f[0]*100));return result;
}
int main(){
	EGLDisplay display=eglGetDisplay(EGL_DEFAULT_DISPLAY);check(eglInitialize(display,nullptr,nullptr),"EGL initialize");
	const EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_NONE};
	EGLConfig cfg;EGLint count=0;check(eglChooseConfig(display,attrs,&cfg,1,&count)&&count==1,"EGL config");
	const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE},pa[]={EGL_WIDTH,2,EGL_HEIGHT,2,EGL_NONE};
	EGLContext ctx=eglCreateContext(display,cfg,EGL_NO_CONTEXT,ca);EGLSurface surface=eglCreatePbufferSurface(display,cfg,pa);
	check(eglMakeCurrent(display,surface,surface,ctx),"EGL current");printf("GPU %s\n",glGetString(GL_RENDERER));
	GXMultiview mv;mv.resolver=[](const char *name)->void * {return reinterpret_cast<void *>(eglGetProcAddress(name));};
	check(mv.available(),"Quest supports OVR_multiview2");
	printf("P15 OVR_multiview2 supported\n");
	GLuint vs=shader(GL_VERTEX_SHADER,"#version 300 es\nlayout(location=0) in vec3 p;uniform highp int xrMode;uniform mat4 eyeClip;void main(){gl_Position=xrMode>0?eyeClip*vec4(p,1):vec4(p,1);}");
	GLuint fs=shader(GL_FRAGMENT_SHADER,"#version 300 es\nprecision highp float;uniform highp int xrMode;out vec4 c;void main(){c=xrMode==0?vec4(.3,.6,.8,.5):vec4(.8,.4,.2,1);}");
	GLuint program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);
	GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);check(ok,"link");glUseProgram(program);
	GLuint mvs=shader(GL_VERTEX_SHADER,"#version 300 es\n#extension GL_OVR_multiview2 : require\nlayout(num_views=2) in;\nlayout(location=0) in vec3 p;uniform highp int xrMode;uniform mat4 eyeClip[2];void main(){gl_Position=xrMode>0?eyeClip[gl_ViewID_OVR]*vec4(p,1):vec4(p,1);}");
	GLuint mp=glCreateProgram();glAttachShader(mp,mvs);glAttachShader(mp,fs);glLinkProgram(mp);
	glGetProgramiv(mp,GL_LINK_STATUS,&ok);check(ok,"multiview program link");
	GLuint mf[3],mt[2];glGenFramebuffers(3,mf);glGenTextures(2,mt);
	for(int t=0;t<2;++t){glBindTexture(GL_TEXTURE_2D_ARRAY,mt[t]);mv.storage(GL_TEXTURE_2D_ARRAY,1,t ? GL_DEPTH24_STENCIL8:GL_RGBA8,64,64,2);}
	glBindFramebuffer(GL_FRAMEBUFFER,mf[0]);
	mv.attach(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,mt[0],0,0,2);
	mv.attach(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,mt[1],0,0,2);
	check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"multiview FBO");
	for(int eye=0;eye<2;++eye){
		glBindFramebuffer(GL_FRAMEBUFFER,mf[eye+1]);
		mv.layer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,mt[0],0,eye);
		mv.layer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,mt[1],0,eye);
		check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"single layer FBO");
	}
	GLuint fbo[4],tex[4],depth[4];glGenFramebuffers(4,fbo);glGenTextures(4,tex);glGenRenderbuffers(4,depth);
	for(int i=0;i<4;++i){
		glBindTexture(GL_TEXTURE_2D,tex[i]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,i==3 ? 128:64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
		glBindRenderbuffer(GL_RENDERBUFFER,depth[i]);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,i==3 ? 128:64,64);
		glBindFramebuffer(GL_FRAMEBUFFER,fbo[i]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex[i],0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth[i]);check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"target");
	}
	GLuint vao,vbo,ibo;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
	const float vertices[]={-.8f,-.8f,0,.8f,-.8f,0,0,.8f,0};glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,12,nullptr);
	glGenBuffers(1,&ibo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);const unsigned short indices[]={0,1,2};glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
	unsigned savedMisses=0,totalRestores=0;
	for(int scenario=0;scenario<1024;++scenario){
		WebGLDevice d;auto &s=d.state;
		s[D3DRS_ZENABLE]=1;s[D3DRS_ZWRITEENABLE]=(scenario>>3)&1;s[D3DRS_ZFUNC]=D3DCMP_LESSEQUAL;s[D3DRS_ZBIAS]=(scenario>>4)&1;
		s[D3DRS_ALPHABLENDENABLE]=scenario&1;s[D3DRS_SRCBLEND]=scenario&8 ? D3DBLEND_SRCALPHA:D3DBLEND_ZERO;
		s[D3DRS_DESTBLEND]=scenario&8 ? D3DBLEND_INVSRCALPHA:D3DBLEND_ONE;
		s[D3DRS_CULLMODE]=scenario&16 ? D3DCULL_CW:D3DCULL_NONE;s[D3DRS_COLORWRITEENABLE]=(scenario>>2)&15;
		s[D3DRS_STENCILENABLE]=(scenario>>1)&1;s[D3DRS_STENCILFUNC]=scenario&32 ? D3DCMP_ALWAYS:D3DCMP_GREATEREQUAL;
		s[D3DRS_STENCILREF]=0x80808080u;s[D3DRS_STENCILMASK]=scenario&64 ? 0:255;s[D3DRS_STENCILWRITEMASK]=scenario&128 ? 0:255;
		s[D3DRS_STENCILFAIL]=D3DSTENCILOP_KEEP;s[D3DRS_STENCILZFAIL]=D3DSTENCILOP_INCRSAT;s[D3DRS_STENCILPASS]=D3DSTENCILOP_REPLACE;
		d.transforms[D3DTS_PROJECTION]._44=0;d.transforms[D3DTS_VIEW]._11=d.transforms[D3DTS_VIEW]._22=d.transforms[D3DTS_VIEW]._33=d.transforms[D3DTS_VIEW]._44=1;
		s_gxDrawCategory=(scenario>>6)%GX_DRAWCAT_COUNT;
		std::vector<GLint> states[7];std::vector<unsigned char> pixels[7];int misses[7]={};unsigned restored[7]={},omitted[7]={},calls[7]={};
		for(int reference=0;reference<7;++reference){
			WebGLPipeline p;p.program.prog=program;p.program.uXrActive=glGetUniformLocation(program,"xrMode");
			p.m_xrElision.configure(reference>=5);
			p.m_offFBO=fbo[0];p.m_xrStereoFBO[0]=fbo[1];p.m_xrStereoFBO[1]=fbo[2];p.m_xrUI=(scenario>>8)&1;
			p.m_xrStereoAtlas=reference==2;if(p.m_xrStereoAtlas)p.m_xrStereoFBO[0]=fbo[3];
			p.program.uXrEyeClip=glGetUniformLocation(program,"eyeClip");
			if(reference>=3){
				p.m_xrStereoMultiview=true;p.m_xrMultiviewFBO=mf[0];p.m_xrStereoFBO[0]=mf[1];p.m_xrStereoFBO[1]=mf[2];
				p.multiviewProgram.prog=(reference==4 || reference==6) ? 0:mp; // Deliberate shader failure, with and without omission.
				p.multiviewProgram.uXrActive=glGetUniformLocation(mp,"xrMode");
				p.multiviewProgram.uXrEyeClip=glGetUniformLocation(mp,"eyeClip[0]");
			}
			for(int eye=0;eye<2;++eye){for(int k=0;k<16;++k)p.m_xrEyeClip[eye][k]=k%5==0 ? 1:0;p.m_xrEyeClip[eye][12]=eye ? .125f:-.125f;}
			p.m_xrStereoActive=(scenario%17)!=0;p.m_curFBO=scenario%19==0 ? fbo[0]:0;
			glUniform1i(p.program.uXrActive,0);glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glDepthMask(1);glStencilMask(255);
			glClearColor(.1f,.2f,.3f,.4f);glClearDepthf(1);glClearStencil(0);
			for(auto target:fbo){glBindFramebuffer(GL_FRAMEBUFFER,target);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);}
			glBindFramebuffer(GL_FRAMEBUFFER,mf[0]);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
			glBindFramebuffer(GL_FRAMEBUFFER,fbo[0]);
			const unsigned fvf=scenario&512 ? D3DFVF_XYZRHW:D3DFVF_XYZ;
			const unsigned beforeCalls=actualDrawCalls;
			for(int draw=0;draw<2;++draw){
				if(reference==1)p.drawReference(&d,D3DPT_TRIANGLELIST,1,vbo,12,fvf,ibo,scenario&4 ? D3DFMT_INDEX16:0,0,0,3);
				else p.drawCommon(&d,D3DPT_TRIANGLELIST,1,vbo,12,fvf,ibo,scenario&4 ? D3DFMT_INDEX16:0,0,0,3);
			}
			states[reference]=snapshot();misses[reference]=p.m_perfStateCacheMisses;restored[reference]=p.m_xrRestores;
			omitted[reference]=p.m_xrElision.pendingSkipped;
			calls[reference]=actualDrawCalls-beforeCalls;
			check(p.m_haveFixedStateKey,"fixed-state cache valid");
			GLint active=-1;glGetUniformiv(program,p.program.uXrActive,&active);check(active==0,"ordinary shader restored");
			// A later ordinary UI draw must see identical depth/stencil/blend state.
			glDrawArrays(GL_TRIANGLES,0,3);
			pixels[reference].resize(3*64*64*4);
			for(int i=0;i<3;++i){glBindFramebuffer(GL_FRAMEBUFFER,p.m_xrStereoMultiview && i>0 ? mf[i]:fbo[p.m_xrStereoAtlas && i>0 ? 3:i]);glReadPixels(p.m_xrStereoAtlas && i==2 ? 64:0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels[reference].data()+i*64*64*4);}
			check(glGetError()==GL_NO_ERROR,"production draw/readback");
		}
		check(states[0]==states[1],"optimized/reference GL state identical");
		check(pixels[0]==pixels[1],"all three targets and following UI pixel-identical");
		check(states[0]==states[2],"atlas restores ordinary state including disabled scissor");
		check(pixels[0]==pixels[2],"atlas eye regions and following UI pixel-identical to separate targets");
		check(restored[0]==restored[2],"atlas preserves stereo dispatch");
		for(int mode=3;mode<5;++mode){
			check(states[0]==states[mode],"multiview/fallback restores exact ordinary GL state");
			check(pixels[0]==pixels[mode],"multiview/fallback both eyes and later UI pixel identical");
			check(restored[0]==restored[mode],"multiview preserves eligibility and dispatch");
		}
		for(int mode=5;mode<7;++mode) {
			check(calls[mode]+omitted[mode]==calls[mode-2],"real GL submissions decrease by exactly the skipped ordinary draws");
			check(states[0]==states[mode],"omitting ordinary world restores exact fixed state");
			check(memcmp(pixels[0].data()+64*64*4,pixels[mode].data()+64*64*4,2*64*64*4)==0,"omission keeps both eyes pixel-identical");
			check(restored[0]==restored[mode],"omission never changes stereo eligibility");
			check(omitted[mode]==unsigned((scenario>>8)&1 ? 0:mode==5 ? restored[mode]:std::min(restored[mode],1u)),"only eligible pre-UI draws omitted; shader failure stops further omissions");
			if(!omitted[mode])check(pixels[0]==pixels[mode],"no omission means ordinary fallback remains identical");
		}
		check(restored[0]==restored[1],"same stereo dispatch");
		check(misses[1]-misses[0]==int(restored[0]),"one full-state application saved per stereo source draw");
		savedMisses+=misses[1]-misses[0];totalRestores+=restored[0];
	}
	check(totalRestores>100,"exercise eligible stereo paths");
	printf("PASS %d production stereo state checks; %u full-state applications avoided; 1024 scenarios\n",checks,savedMisses);
	// Actual XR compositing shaders: two colored eye regions with a linear
	// sampler and UVs beyond the eye rectangle must never bleed at the seam.
	GLuint qv=shader(GL_VERTEX_SHADER,kQuadVertShader),qf=shader(GL_FRAGMENT_SHADER,kQuadFragShader);
	GLuint qp=glCreateProgram();glAttachShader(qp,qv);glAttachShader(qp,qf);glLinkProgram(qp);
	glGetProgramiv(qp,GL_LINK_STATUS,&ok);check(ok,"production quad link");glUseProgram(qp);
	glUniform1i(glGetUniformLocation(qp,"uStereoArray"),3);glUniform1i(glGetUniformLocation(qp,"uArrayEye"),-1);
	const float quad[]={-1,-1,0,-.01f,-.01f, 1,-1,0,1.01f,-.01f, -1,1,0,-.01f,1.01f,
		1,-1,0,1.01f,-.01f, 1,1,0,1.01f,1.01f, -1,1,0,-.01f,1.01f};
	glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);
	const GLuint pos=glGetAttribLocation(qp,"aPos"),uv=glGetAttribLocation(qp,"aUV");
	glEnableVertexAttribArray(pos);glVertexAttribPointer(pos,3,GL_FLOAT,GL_FALSE,20,nullptr);
	glEnableVertexAttribArray(uv);glVertexAttribPointer(uv,2,GL_FLOAT,GL_FALSE,20,(void*)12);
	float identity[16]={};for(int k=0;k<16;++k)identity[k]=k%5==0 ? 1:0;
	glUniformMatrix4fv(glGetUniformLocation(qp,"uMVP"),1,GL_FALSE,identity);
	glUniform1i(glGetUniformLocation(qp,"uLayer"),3);glUniform4f(glGetUniformLocation(qp,"uPointer"),0,0,0,0);
	glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tex[3]);glUniform1i(glGetUniformLocation(qp,"uTex"),0);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	std::vector<unsigned char> colors(128*64*4,0);for(int y=0;y<64;++y)for(int x=0;x<128;++x){colors[(y*128+x)*4+(x<64 ? 0:1)]=255;colors[(y*128+x)*4+3]=255;}
	glTexSubImage2D(GL_TEXTURE_2D,0,0,0,128,64,GL_RGBA,GL_UNSIGNED_BYTE,colors.data());
	glBindFramebuffer(GL_FRAMEBUFFER,fbo[0]);glViewport(0,0,64,64);glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);
	for(int eye=0;eye<2;++eye){
		glUniform4f(glGetUniformLocation(qp,"uUVRect"),eye*.5f,0,.5f,1);glDrawArrays(GL_TRIANGLES,0,6);
		std::vector<unsigned char> out(64*64*4);glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,out.data());
		for(int pixel=0;pixel<64*64;++pixel)check(out[pixel*4+eye]==255 && out[pixel*4+1-eye]==0 && out[pixel*4+3]==255,"eye region filtering isolation");
	}
	// Actual compositor samples each array layer without an extra full-eye copy.
	glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D_ARRAY,mt[0]);
	glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	for(int eye=0;eye<2;++eye){
		glBindFramebuffer(GL_FRAMEBUFFER,mf[eye+1]);glClearColor(eye==0,eye==1,0,1);glClear(GL_COLOR_BUFFER_BIT);
		glBindFramebuffer(GL_FRAMEBUFFER,fbo[0]);glUniform1i(glGetUniformLocation(qp,"uArrayEye"),eye);
		glUniform4f(glGetUniformLocation(qp,"uUVRect"),0,0,1,1);glDrawArrays(GL_TRIANGLES,0,6);
		std::vector<unsigned char> out(64*64*4);glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,out.data());
		for(int pixel=0;pixel<64*64;++pixel)check(out[pixel*4+eye]==255 && out[pixel*4+1-eye]==0 && out[pixel*4+3]==255,"array eye filtering isolation");
	}
	glBindTexture(GL_TEXTURE_2D_ARRAY,0);glActiveTexture(GL_TEXTURE0);
	// GeneralsX @test Codex 14/09/2026 Orange outline does not sample a
	// game texture and leaves the center (including passthrough alpha) intact.
	glUniform1i(glGetUniformLocation(qp,"uArrayEye"),-1);
	glUniform1i(glGetUniformLocation(qp,"uLayer"),7);
	glUniform4f(glGetUniformLocation(qp,"uPointer"),.07f,.07f,0,0);
	glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,6);
	unsigned char center[4],edge[4];glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,center);
	glReadPixels(1,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,edge);
	check(center[0]==0 && center[3]==0,"edit outline leaves world/passthrough center untouched");
	check(edge[0]==255 && edge[1]>85 && edge[1]<110 && edge[2]<10 && edge[3]==255,"edit outline uses display-linear orange");
	// GeneralsX @test Codex 14/09/2026 Detection and invalid-fit colors share
	// the production texture-free compositor, not a screenshot approximation.
	for(int layer:{8,9}) {
		glUniform1i(glGetUniformLocation(qp,"uLayer"),layer);
		glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,6);
		glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,center);
		glReadPixels(1,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,edge);
		check(center[3]==0,"scene outline center stays transparent");
		check(layer==8 ? edge[0]<10 && edge[1]>210 && edge[2]==255 :
			edge[0]==255 && edge[1]<15 && edge[2]<10,"scene outline cyan/red in linear display");
	}
	check(glGetError()==GL_NO_ERROR,"production composite GL state");
	glDeleteProgram(qp);glDeleteShader(qv);glDeleteShader(qf);glDisableVertexAttribArray(uv);glUseProgram(program);
	glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,12,nullptr);
	printf("PASS %d cumulative checks including actual XR quad sampling\n",checks);
	// GeneralsX @test Codex 14/09/2026 Bounded target-switch microbenchmark,
	// not game FPS: same production dispatch and per-eye pixels, small geometry.
	for(int i=0;i<4;++i){
		glBindTexture(GL_TEXTURE_2D,tex[i]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,i==3 ? 3072:1536,1609,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
		glBindRenderbuffer(GL_RENDERBUFFER,depth[i]);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,i==3 ? 3072:1536,1609);
		glBindFramebuffer(GL_FRAMEBUFFER,fbo[i]);check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Balanced target");
	}
	const float small[]={-.01f,-.01f,0,.01f,-.01f,0,0,.01f,0};glBufferData(GL_ARRAY_BUFFER,sizeof(small),small,GL_STATIC_DRAW);
	for(int round=0;round<4;++round)for(int order=0;order<2;++order){
		const bool atlas=(order+(round%2))%2;WebGLPipeline p;p.program.prog=program;
		p.program.uXrActive=glGetUniformLocation(program,"xrMode");p.program.uXrEyeClip=glGetUniformLocation(program,"eyeClip");
		p.m_xrStereoAtlas=atlas;p.m_offFBO=fbo[0];p.m_xrStereoFBO[0]=fbo[atlas ? 3:1];p.m_xrStereoFBO[1]=fbo[2];
		p.m_curRTHeight=1609;p.m_xrStereoW=1536;p.m_xrStereoH=1609;
		for(int eye=0;eye<2;++eye)for(int k=0;k<16;++k)p.m_xrEyeClip[eye][k]=k%5==0 ? 1:0;
		WebGLDevice d;d.vp={0,0,1536,1609,0,1};d.state[D3DRS_ZENABLE]=1;d.state[D3DRS_ZWRITEENABLE]=1;
		d.state[D3DRS_ZFUNC]=D3DCMP_LESSEQUAL;d.state[D3DRS_COLORWRITEENABLE]=15;d.state[D3DRS_CULLMODE]=D3DCULL_NONE;
		s_gxDrawCategory=GX_DRAWCAT_TERRAIN;
		glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glDepthMask(1);glStencilMask(255);
		for(auto target:fbo){glBindFramebuffer(GL_FRAMEBUFFER,target);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);}
		glBindFramebuffer(GL_FRAMEBUFFER,fbo[0]);glFinish();const auto start=std::chrono::steady_clock::now();
		for(int draw=0;draw<600;++draw)p.drawCommon(&d,D3DPT_TRIANGLELIST,1,vbo,12,D3DFVF_XYZ,0,0,0,0,3);
		glFinish();const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
		check(p.m_xrRestores==600 && glGetError()==GL_NO_ERROR,"benchmark stereo draws");
		printf("BENCH round=%d stereo=%s 600 source draws, 1536x1609/eye, wall=%.3f ms\n",round,atlas ? "atlas":"separate",ms);
	}
	glDeleteBuffers(1,&vbo);glDeleteBuffers(1,&ibo);glDeleteVertexArrays(1,&vao);glDeleteFramebuffers(4,fbo);glDeleteTextures(4,tex);glDeleteRenderbuffers(4,depth);
	glDeleteFramebuffers(3,mf);glDeleteTextures(2,mt);glDeleteProgram(mp);glDeleteShader(mvs);
	glDeleteProgram(program);glDeleteShader(vs);glDeleteShader(fs);eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
	eglDestroySurface(display,surface);eglDestroyContext(display,ctx);eglTerminate(display);
}
