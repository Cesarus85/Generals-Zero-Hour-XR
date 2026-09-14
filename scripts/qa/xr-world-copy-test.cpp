// GeneralsX @test Codex 14/09/2026 Production copy gate, MRT and fallback.
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <chrono>
#include "XrViewMode.h"
#include "XRWorldElision.h"
#include <d3d8.h>
struct WebGLDevice {
 D3DVIEWPORT8 viewport{0,0,64,64,0,1};
 const D3DVIEWPORT8 &getViewport() const {return viewport;}
};
static unsigned checks=0,blits=0;
static void check(bool b,const char *why){++checks;if(!b){fprintf(stderr,"FAIL %u %s GL=%x\n",checks,why,glGetError());exit(1);}}
static void countedBlit(GLint a,GLint b,GLint c,GLint d,GLint e,GLint f,GLint g,GLint h,GLbitfield mask,GLenum filter){
 ++blits;glBlitFramebuffer(a,b,c,d,e,f,g,h,mask,filter);
}
#define glBlitFramebuffer countedBlit
#define WARN_ONCE(id,...) do{fprintf(stderr,__VA_ARGS__);}while(0)
struct WebGLPipeline {
	GXWorldElision m_xrElision;
	GLuint m_offColorTex=99;
 bool m_xrMode=true,m_ctxReady=true,m_xrSplitRequested=true,m_xrUI=false,m_xrSplitReady=false;
 bool m_xrStereoReady=false,m_xrStereoActive=false,m_xrStereoSeen=false,m_xrStereoCoverage=false;
 bool m_xrWorldSnapshotValid=false,offscreenAvailable=true;unsigned m_xrStereoProbeWait=0,invalidations=0;
 GLuint m_curFBO=0,m_offFBO=0,m_xrWorldFBO=0,m_xrWorldTex=0,m_xrUITex=0;
 int m_offW=64,m_offH=64;
 int m_curRTWidth=64,m_curRTHeight=64,m_fbWidth=64,m_fbHeight=64;
 bool m_haveFixedStateKey=false;
 void clear(WebGLDevice *,unsigned,uint32_t,float,unsigned);
 bool ensureOffscreenTarget(){return offscreenAvailable;}
 void invalidateCachedGLState(){++invalidations;}
 void destroyXRLayers(){glDeleteFramebuffers(1,&m_xrWorldFBO);glDeleteTextures(1,&m_xrWorldTex);glDeleteTextures(1,&m_xrUITex);m_xrWorldFBO=m_xrWorldTex=m_xrUITex=0;m_xrWorldSnapshotValid=false;}
 void beginXRFrame(bool,bool=false);bool beginXRUI(bool);void finishXRFrame();
#include "world-copy-getter.inc"
};
#include "world-copy-production.inc"
#undef glBlitFramebuffer
static GLuint shader(GLenum type,const char *s){GLuint id=glCreateShader(type);glShaderSource(id,1,&s,nullptr);glCompileShader(id);GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);check(ok,"shader");return id;}
static std::vector<unsigned char> pixels(GLuint fbo,GLenum attachment,int w=64,int h=64){
 glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);glReadBuffer(attachment);std::vector<unsigned char> p(size_t(w)*h*4);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,p.data());glReadBuffer(GL_COLOR_ATTACHMENT0);return p;
}
int main(){
 EGLDisplay display=eglGetDisplay(EGL_DEFAULT_DISPLAY);check(eglInitialize(display,nullptr,nullptr),"EGL");
 const EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_NONE};
 EGLConfig cfg;EGLint count=0;check(eglChooseConfig(display,attrs,&cfg,1,&count)&&count==1,"config");
 const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE},pa[]={EGL_WIDTH,2,EGL_HEIGHT,2,EGL_NONE};
 EGLContext context=eglCreateContext(display,cfg,EGL_NO_CONTEXT,ca);EGLSurface surface=eglCreatePbufferSurface(display,cfg,pa);
 check(eglMakeCurrent(display,surface,surface,context),"current");printf("GPU %s\n",glGetString(GL_RENDERER));
 WebGLPipeline p;GLuint composed;glGenFramebuffers(1,&p.m_offFBO);glGenTextures(1,&composed);
 glBindTexture(GL_TEXTURE_2D,composed);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
 glBindFramebuffer(GL_FRAMEBUFFER,p.m_offFBO);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,composed,0);
 check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"composed target");
 GLuint vs=shader(GL_VERTEX_SHADER,"#version 300 es\nvoid main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));gl_Position=vec4(p*2.-1.,0,1);}");
 GLuint fs=shader(GL_FRAGMENT_SHADER,"#version 300 es\nprecision mediump float;uniform vec4 color;layout(location=0)out vec4 a;layout(location=1)out vec4 b;void main(){a=color;b=color;}");
 GLuint program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);check(linked,"MRT link");glUseProgram(program);
 GLint color=glGetUniformLocation(program,"color");glViewport(0,0,64,64);
 WebGLDevice dev;
 auto world=[&](int variant){
  glBindFramebuffer(GL_FRAMEBUFFER,p.m_offFBO);const GLenum one=GL_COLOR_ATTACHMENT0;glDrawBuffers(1,&one);
  glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glDisable(GL_BLEND);
  p.clear(&dev,D3DCLEAR_TARGET,variant%2 ? 0xffcc6699:0xff336699,1,0);
 };
 for(int scenario=0;scenario<128;++scenario){
  std::vector<unsigned char> referenceComposed,referenceUI;
  for(int mode=0;mode<2;++mode){
   p.beginXRFrame(true);check(!p.m_xrStereoReady,"new frame never trusts previous stereo readiness");
   const bool ready=scenario&1;p.m_xrStereoReady=ready;world(scenario);
   const auto source=pixels(p.m_offFBO,GL_COLOR_ATTACHMENT0);
   const unsigned before=blits;check(p.beginXRUI(mode==1),"production UI boundary");
   check(blits-before==unsigned(!(mode && ready)),"exactly zero/one world copies");
   check(bool(p.xrWorldTexture())==!(mode && ready),"omitted snapshot cannot escape via getter");
   GLint draw=0,read=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
   check(GLuint(draw)==p.m_offFBO && GLuint(read)==p.m_offFBO,"MRT bound for both modes");
   GLint output=0;glGetIntegerv(GL_DRAW_BUFFER1,&output);check(output==GL_COLOR_ATTACHMENT1,"isolated UI output retained");
   check(pixels(p.m_offFBO,GL_COLOR_ATTACHMENT0)==source,"world in composed fallback preserved");
   auto empty=pixels(p.m_offFBO,GL_COLOR_ATTACHMENT1);bool transparent=true;for(auto c:empty)transparent=transparent && c==0;check(transparent,"UI attachment cleared");
   if(p.xrWorldTexture())check(pixels(p.m_xrWorldFBO,GL_COLOR_ATTACHMENT0)==source,"fresh planar snapshot");
   glBindFramebuffer(GL_FRAMEBUFFER,p.m_offFBO);glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
   glEnable(GL_SCISSOR_TEST);glScissor(scenario%16,0,32,64);glUniform4f(color,1,0,0,.5f);glDrawArrays(GL_TRIANGLES,0,3);
   glScissor(0,scenario%16,64,32);glUniform4f(color,0,1,0,.5f);glDrawArrays(GL_TRIANGLES,0,3);
   const auto combined=pixels(p.m_offFBO,GL_COLOR_ATTACHMENT0),ui=pixels(p.m_offFBO,GL_COLOR_ATTACHMENT1);
   if(!mode){referenceComposed=combined;referenceUI=ui;}else {check(combined==referenceComposed,"fallback pixel-identical");check(ui==referenceUI,"detached UI pixel-identical");}
   check(!p.beginXRUI(true),"duplicate boundary rejected");
   struct Host{bool splitVisible=true,stereoVisible=true,stereoWorld=true;} host;
   xrResolveCapturedView(host,true,false,p.xrWorldTexture()!=0);
   check(!host.stereoVisible && host.splitVisible==bool(p.xrWorldTexture()),"late stereo loss never samples omitted world");
   check(glGetError()==GL_NO_ERROR,"no GL errors");
  }
 }
 // Ineligible split, external target, missing context/offscreen and duplicate
 // entry must not blit or change readiness. These cover native/flat callers.
 for(int reason=0;reason<5;++reason){
  p.beginXRFrame(true);p.m_xrMode=reason!=0;p.m_ctxReady=reason!=1;p.m_xrSplitRequested=reason!=2;p.m_curFBO=reason==3 ? 7:0;p.offscreenAvailable=reason!=4;
  const unsigned before=blits;check(!p.beginXRUI(true) && blits==before,"ineligible call unchanged");
  p.m_xrMode=p.m_ctxReady=p.offscreenAvailable=true;p.m_curFBO=0;
 }
 p.beginXRFrame(true);world(0);check(p.beginXRUI(true) && p.xrWorldTexture(),"normal view recovers current snapshot");
 // P17: even forced-copy requests cannot publish a partially omitted world.
 for(bool eyes:{false,true})for(bool uiReady:{false,true}) {
  p.m_xrElision={};p.beginXRFrame(true,true);world(0);p.m_xrStereoReady=eyes;
  p.m_xrElision.omit();const auto before=blits;
  check(p.beginXRUI(false) && blits==before,"omitted world never copied even in Always request");
  p.m_xrUI=uiReady;p.finishXRFrame();
  check(!p.offscreenTexture() && !p.xrWorldTexture(),"incomplete composed and planar getters blocked");
  check(p.m_xrElision.blocked==(!eyes || !uiReady),"late failure latches ordinary recovery");
  struct Host {bool splitVisible=true,stereoVisible=false,stereoWorld=true;} h;
  const bool recovery=xrResolveCapturedView(h,uiReady,eyes,false,false);
  check(recovery==(!eyes || !uiReady),"host never displays partial world on late failure");
  p.beginXRFrame(true,true);
  check(!p.offscreenTexture(),"begin without a native Present cannot relabel old partial contents");
  p.finishXRFrame();check(!p.offscreenTexture(),"empty Present cannot relabel old partial contents");
  dev.viewport.Width=32;p.clear(&dev,D3DCLEAR_TARGET,0,1,0);p.finishXRFrame();
  check(!p.offscreenTexture(),"partial viewport cannot repair missing world");dev.viewport.Width=64;
  p.clear(&dev,D3DCLEAR_ZBUFFER,0,1,0);p.finishXRFrame();check(!p.offscreenTexture(),"depth clear cannot repair missing color");
  // A nested full loading/video Present recovers immediately without scene re-execution.
  world(1);p.finishXRFrame();check(p.offscreenTexture()==99,"next complete native Present recovers composed frame");
 }
 p.m_xrElision={};
 printf("PASS %u production world-copy/MRT/fallback checks\n",checks);
 // Small copy-boundary benchmark, not game FPS. Keep the full source frame
 // and one MRT UI draw, just as production does; alternate order each round.
 p.destroyXRLayers();p.m_offW=1280;p.m_offH=720;
 p.m_fbWidth=1280;p.m_fbHeight=720;dev.viewport.Width=1280;dev.viewport.Height=720;
 glBindTexture(GL_TEXTURE_2D,composed);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1280,720,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glViewport(0,0,1280,720);
 for(int round=0;round<4;++round)for(int order=0;order<2;++order){
  const bool elide=(order+round)%2;
  p.beginXRFrame(true);world(0);p.beginXRUI(elide);glFinish();const auto start=std::chrono::steady_clock::now();const unsigned before=blits;
  for(int frame=0;frame<120;++frame){p.beginXRFrame(true);world(frame);p.m_xrStereoReady=true;p.beginXRUI(elide);glUniform4f(color,.2f,.3f,.4f,.5f);glDrawArrays(GL_TRIANGLES,0,3);}
  glFinish();const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/120;
  check(blits-before==unsigned(elide ? 0:120) && glGetError()==GL_NO_ERROR,"benchmark copy count");
  printf("BENCH round=%d world-copy=%s ms/frame=%.3f\n",round,elide ? "auto":"always",ms);
 }
 p.destroyXRLayers();glDeleteTextures(1,&composed);glDeleteFramebuffers(1,&p.m_offFBO);glDeleteProgram(program);glDeleteShader(vs);glDeleteShader(fs);
 eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroySurface(display,surface);eglDestroyContext(display,context);eglTerminate(display);
 printf("PASS %u total world-copy checks\n",checks);
}
