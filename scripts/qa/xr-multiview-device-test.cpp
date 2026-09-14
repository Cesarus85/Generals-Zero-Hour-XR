// GeneralsX @test Codex 14/09/2026 Actual multiview allocation/lifetime and
// production stereo shader contracts. No game assets, OpenXR or timer queries.
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include "XRMultiview.h"
#include "XRStereoShader.h"
static unsigned checks=0;
static void check(bool v,const char *why){++checks;if(!v){fprintf(stderr,"FAIL %s GL=%x\n",why,glGetError());exit(1);}}
struct WebGLPipeline {
 bool m_xrMode=true,m_ctxReady=true,m_xrSplitRequested=true,m_xrStereoActive=false,m_xrStereoReady=false;
 bool m_xrStereoCoverage=false,m_xrStereoSeen=false,m_xrStereoAtlas=false,m_xrAtlasRequested=false;
 bool m_xrStereoMultiview=false,m_xrMultiviewRequested=false,m_xrMultiviewFailed=false;
 GXMultiview m_xrMultiview;
 GLuint m_curFBO=0,m_offFBO=0,m_xrMultiviewFBO=0,m_xrMultiviewDepth=0;
 GLuint m_xrStereoFBO[2]={},m_xrStereoTex[2]={},m_xrStereoDepth[2]={};
 GLuint m_xrDecorProgram=0,m_xrDecorVAO=0,m_xrDecorVBO=0;
 int m_xrStereoW=0,m_xrStereoH=0;
 unsigned m_xrStereoDraws=0,m_xrTerrainDraws=0,m_xrModelDraws=0,m_xrEffectDraws=0,m_xrShadowDraws=0,m_xrStereoProbeWait=0;
 float m_xrCamera[16]={},m_xrEyeClip[2][16]={},m_xrBoard[16]={},m_xrStereoAspect=1;
 void invalidateCachedGLState(){glBindFramebuffer(GL_FRAMEBUFFER,m_offFBO);}
 void destroyXRStereo();
 bool beginXRStereo(int,int,const float *,const float *,const float *,float,const float *,bool,bool);
 void endXRStereo();
#include "multiview-getters.inc"
};
#include "multiview-production.inc"
static GLuint shader(GLenum kind,const char *src){
 GLuint s=glCreateShader(kind);glShaderSource(s,1,&src,nullptr);glCompileShader(s);
 GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
 if(!ok){char log[4096];glGetShaderInfoLog(s,sizeof(log),nullptr,log);fprintf(stderr,"%s\n",log);}
 check(ok,"production contract shader");return s;
}
static GLuint program(bool mv){
 const char *v[2]={
 "#version 300 es\nprecision highp float;\n" GX_XR_STEREO_VERTEX_DECL
 "layout(location=0) in vec3 pos;void main(){vec4 wpos=vec4(pos,1);gl_Position=wpos;vXrBoard=vec3(0);"
 GX_XR_STEREO_VERTEX_BODY "}\n",
 "#version 300 es\n" GX_XR_MULTIVIEW_PREAMBLE "precision highp float;\n" GX_XR_MULTIVIEW_VERTEX_DECL
 "layout(location=0) in vec3 pos;void main(){vec4 wpos=vec4(pos,1);gl_Position=wpos;vXrBoard=vec3(0);"
 GX_XR_MULTIVIEW_VERTEX_BODY "}\n"};
 const char *f="#version 300 es\nprecision highp float;\n" GX_XR_STEREO_FRAGMENT_DECL
 "uniform float alpha,cutout;out vec4 color;void main(){" GX_XR_STEREO_FRAGMENT_BODY
 "vec4 cur=vec4(.8,.5,.2,alpha);if(cur.a<cutout)discard;" GX_XR_STEREO_COVERAGE_BODY "color=cur;}\n";
 GLuint vs=shader(GL_VERTEX_SHADER,v[mv]),fs=shader(GL_FRAGMENT_SHADER,f),p=glCreateProgram();
 glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);check(ok,"link");
 glDeleteShader(vs);glDeleteShader(fs);return p;
}
int main(){
 EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);check(eglInitialize(d,nullptr,nullptr),"EGL");
 const EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_NONE};
 EGLConfig cfg;EGLint n=0;check(eglChooseConfig(d,attrs,&cfg,1,&n)&&n==1,"config");
 const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE},sa[]={EGL_WIDTH,2,EGL_HEIGHT,2,EGL_NONE};
 EGLContext c=eglCreateContext(d,cfg,EGL_NO_CONTEXT,ca);EGLSurface surface=eglCreatePbufferSurface(d,cfg,sa);
 check(eglMakeCurrent(d,surface,surface,c),"current");
 printf("GPU %s\n",glGetString(GL_RENDERER));
 WebGLPipeline p;p.m_xrMultiview.resolver=[](const char *name)->void * {return reinterpret_cast<void *>(eglGetProcAddress(name));};
 check(p.m_xrMultiview.available(),"OVR_multiview2");
 float identity[16]={},eyes[2][16]={};for(int k=0;k<16;++k)identity[k]=eyes[0][k]=eyes[1][k]=k%5==0 ? 1:0;
 eyes[0][12]=-.13f;eyes[1][12]=.13f;
 // Actual allocator: repeated mode/extent changes, reused targets, unsupported
 // extension, permanent shader failure, and invalid arguments.
 for(int pass=0;pass<18;++pass){
  const bool requested=pass%3==2,atlas=pass%3==1;
  p.m_xrMultiviewFailed=pass>=12;
  p.m_xrMultiview.supported=pass<6 || pass>=12;
  const int w=pass%2 ? 1920:1536,h=pass%2 ? 2010:1609;
  check(p.beginXRStereo(w,h,eyes[0],eyes[1],identity,1,identity,atlas,requested),"production allocation");
  check(p.m_xrStereoMultiview==(requested && pass<6),"supported/failure fallback policy");
  check(p.m_xrStereoAtlas==atlas,"atlas retained only when selected");
  const GLuint old=p.m_xrStereoFBO[0];
  check(p.beginXRStereo(w,h,eyes[0],eyes[1],identity,1,identity,atlas,requested) && old==p.m_xrStereoFBO[0],"no per-frame reallocation");
  check(!p.xrStereoTexture(0) && !p.xrStereoMultiview(),"not published before coverage");
  for(int eye=0;eye<2;++eye){
   glBindFramebuffer(GL_FRAMEBUFFER,p.m_xrStereoFBO[atlas ? 0:eye]);
   unsigned char px[4]={};glReadPixels(atlas ? eye*w:0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
   check(px[3]==0 && glGetError()==GL_NO_ERROR,"both fresh layers transparent");
  }
 }
 check(!p.beginXRStereo(63,64,eyes[0],eyes[1],identity,1,identity,false,true),"invalid extent rejected");
 check(!p.beginXRStereo(64,64,nullptr,eyes[1],identity,1,identity,false,true),"null matrix rejected");
 p.destroyXRStereo();p.m_xrMultiview.supported=true;p.m_xrMultiviewFailed=false;
 GLuint prog[2]={program(false),program(true)},vao,vbo;
 glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
 const float vertices[]={-.35f,-.35f,0,.35f,-.35f,0,0,.35f,0};
 glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,12,nullptr);
 for(int scenario=0;scenario<128;++scenario){
  std::vector<unsigned char> reference;
  const float altitude=scenario&64 ? 300.0f:scenario&32 ? 150.0f:0;
  eyes[0][14]=eyes[1][14]=-altitude;
  for(int mode=0;mode<2;++mode){
   check(p.beginXRStereo(64,64,eyes[0],eyes[1],identity,1,identity,false,mode),"small eye allocation");
   glUseProgram(prog[mode]);glViewport(0,0,64,64);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
   float board[16],camera[16];memcpy(board,identity,sizeof(board));memcpy(camera,identity,sizeof(camera));
   if(scenario&1)board[12]=10;
   if(scenario&2)camera[12]=.09f;
   if(altitude) {board[0]=board[5]=board[10]=1.0f/200;camera[14]=altitude;}
   const int active=(scenario%4)==3 ? 3:1;
   glUniform1i(glGetUniformLocation(prog[mode],"uXrActive"),active);
   glUniform1i(glGetUniformLocation(prog[mode],"uXrViewSpace"),(scenario&2)!=0 || altitude!=0);
   glUniform1i(glGetUniformLocation(prog[mode],"uXrOpaque"),(scenario&4)!=0);
   glUniform1f(glGetUniformLocation(prog[mode],"uXrAspect"),1);
   glUniform1f(glGetUniformLocation(prog[mode],"alpha"),(scenario&8) ? 0:.5f);
   glUniform1f(glGetUniformLocation(prog[mode],"cutout"),(scenario&16) ? .3f:0);
   glUniformMatrix4fv(glGetUniformLocation(prog[mode],"uXrBoard"),1,GL_FALSE,board);
   glUniformMatrix4fv(glGetUniformLocation(prog[mode],"uXrCamera"),1,GL_FALSE,camera);
   if(mode){
    glBindFramebuffer(GL_FRAMEBUFFER,p.m_xrMultiviewFBO);
    glUniformMatrix4fv(glGetUniformLocation(prog[mode],"uXrEyes[0]"),2,GL_FALSE,eyes[0]);glDrawArrays(GL_TRIANGLES,0,3);
   }else for(int eye=0;eye<2;++eye){
    glBindFramebuffer(GL_FRAMEBUFFER,p.m_xrStereoFBO[eye]);
    glUniformMatrix4fv(glGetUniformLocation(prog[mode],"uXrEyeClip"),1,GL_FALSE,eyes[eye]);glDrawArrays(GL_TRIANGLES,0,3);
   }
   std::vector<unsigned char> pixels(2*64*64*4);bool covered=true;
   for(int eye=0;eye<2;++eye){
    glBindFramebuffer(GL_FRAMEBUFFER,p.m_xrStereoFBO[eye]);
    glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data()+eye*64*64*4);
    unsigned alpha=0;for(int i=0;i<64*64;++i)alpha+=pixels[(eye*64*64+i)*4+3]>0;
    covered=covered && alpha>=64;
   }
   // Use actual coverage probe and public getters, not draw count alone.
   if(altitude && !(scenario&1) && !(scenario&8))
    check(covered==(altitude==150),"P18 high terrain visible, above-ceiling geometry clipped in both eyes");
   p.m_xrStereoDraws=1;p.endXRStereo();
   check(bool(p.xrStereoTexture(0))==covered && bool(p.xrStereoTexture(1))==covered,"publish both eyes only after coverage");
   check(p.xrStereoMultiview()==(covered && mode),"actual texture target contract");
   if(mode)check(pixels==reference,"production MV projection/clip/particles/cutout/coverage equal reference");else reference=pixels;
   check(glGetError()==GL_NO_ERROR,"production multiview error-free");
  }
 }
 p.destroyXRStereo();check(!p.m_xrMultiviewFBO && !p.m_xrMultiviewDepth && !p.xrStereoTexture(0),"resource teardown");
 glDeleteProgram(prog[0]);glDeleteProgram(prog[1]);glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);
 eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroySurface(d,surface);eglDestroyContext(d,c);eglTerminate(d);
 printf("PASS %u production multiview allocation/shader/coverage checks\n",checks);
}
