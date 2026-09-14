// GeneralsX @test Codex 14/09/2026 Extracted production uniform application.
// Compare the old non-XR cache policy with per-program XR caches using the
// same programs, dynamic D3D state, real multiview targets and GPU readbacks.
#include <d3d8.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include "XRMultiview.h"
static unsigned checks=0,uploads=0;
static void check(bool b,const char *why){++checks;if(!b){fprintf(stderr,"FAIL %s GL=%x\n",why,glGetError());exit(1);}}
static unsigned SDL_GetTicks(){return 0;}
// Exclude the opt-in diagnostic camera wobble marker, not normal uniforms.
static FILE *noCameraMarker(const char *,const char *){return nullptr;}
static void matrix(GLint a,GLsizei b,GLboolean c,const GLfloat *d){++uploads;glUniformMatrix4fv(a,b,c,d);}
static void fourv(GLint a,GLsizei b,const GLfloat *c){++uploads;glUniform4fv(a,b,c);}
static void threev(GLint a,GLsizei b,const GLfloat *c){++uploads;glUniform3fv(a,b,c);}
static void oneiv(GLint a,GLsizei b,const GLint *c){++uploads;glUniform1iv(a,b,c);}
static void onei(GLint a,GLint b){++uploads;glUniform1i(a,b);}
static void onef(GLint a,GLfloat b){++uploads;glUniform1f(a,b);}
static void twof(GLint a,GLfloat b,GLfloat c){++uploads;glUniform2f(a,b,c);}
static void fourf(GLint a,GLfloat b,GLfloat c,GLfloat d,GLfloat e){++uploads;glUniform4f(a,b,c,d,e);}
#define glUniformMatrix4fv matrix
#define glUniform4fv fourv
#define glUniform3fv threev
#define glUniform1iv oneiv
#define glUniform1i onei
#define glUniform1f onef
#define glUniform2f twof
#define glUniform4f fourf
#define fopen noCameraMarker
struct WebGLDevice {
 static constexpr unsigned kMaxLights=8;
 DWORD state[256]={};D3DVIEWPORT8 viewport{0,0,32,32,0,1};
 D3DMATRIX transforms[512]={};D3DMATERIAL8 material{};D3DLIGHT8 lights[8]={};bool enabled[8]={};
 DWORD getRenderState(unsigned i)const{return state[i];}
 const D3DVIEWPORT8 &getViewport()const{return viewport;}
 const D3DMATRIX &getTransform(unsigned i)const{return transforms[i];}
 const D3DMATERIAL8 &getMaterial()const{return material;}
 bool isLightEnabled(unsigned i)const{return enabled[i];}
 const D3DLIGHT8 &getLight(unsigned i)const{return lights[i];}
};
struct WebGLPipeline {
 struct ProgramInfo;
#include "uniform-members.inc"
 bool m_xrMode=true,m_ctxReady=true,m_haveLastVAOKey=false,m_haveFixedStateKey=false;
 GLuint m_lastProgram=0,m_viewProjUBO=0,m_curFBO=0,m_offFBO=0,m_lastArrayBuffer=0,m_lastBoundTex[2]={};
 float m_yFlip=1;
 int m_perfUniformCacheHits=0,m_perfUniformCacheMisses=0;
 int m_perfUniformViewProjHits=0,m_perfUniformViewProjMisses=0,m_perfUniformTexMatHits=0,m_perfUniformTexMatMisses=0;
 int m_perfUniformMiscHits=0,m_perfUniformMiscMisses=0,m_perfUniformMaterialHits=0,m_perfUniformMaterialMisses=0,m_perfUniformLightingHits=0,m_perfUniformLightingMisses=0;
 void applyUniforms(WebGLDevice *,ProgramInfo *,unsigned);
 void invalidateCachedGLState();
};
#include "uniform-program.inc"
#include "uniform-production.inc"
#undef fopen
static GLuint shader(GLenum type,const std::string &source){
 GLuint s=glCreateShader(type);const char *v=source.c_str();glShaderSource(s,1,&v,nullptr);glCompileShader(s);
 GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
 if(!ok){char log[4096];glGetShaderInfoLog(s,sizeof(log),nullptr,log);fprintf(stderr,"%s\n",log);}
 check(ok,"fixture shader");return s;
}
static WebGLPipeline::ProgramInfo makeProgram(int kind){
 std::string vs="#version 300 es\n";
 if(kind==1)vs+="#extension GL_OVR_multiview2 : require\nlayout(num_views=2) in;\n";
 vs+="precision highp float;uniform mat4 uWorld,uTexMat0,uTexMat1;layout(std140) uniform ViewProjBlock {mat4 uView;mat4 uProj;};out vec4 uv;void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));gl_Position=uProj*uView*uWorld*vec4(p*2.-1.,0,1);uv=(uTexMat0+uTexMat1)*vec4(.1,.2,.3,1);";
 if(kind==1)vs+="gl_Position.x+=float(gl_ViewID_OVR)*.025;";
 vs+="}\n";
 std::string fs="#version 300 es\nprecision highp float;in vec4 uv;out vec4 color;uniform vec4 uViewportPos,uTFactor,uFogColor,uGlobalAmbient;uniform float uYFlip,uAlphaRef;uniform vec2 uFogParams;uniform int uNumLights,uLightType[4];uniform vec3 uLightDir[4],uLightPos[4];uniform vec4 uLightDiffuse[4],uLightAmbient[4],uLightAtten[4];";
 if(kind!=2)fs+="uniform vec4 uMatDiffuse,uMatAmbient,uMatEmissive;";
 fs+="void main(){vec4 x=uv+uTFactor+uFogColor+uGlobalAmbient+uViewportPos*.01+vec4(uYFlip,uAlphaRef,uFogParams);";
 if(kind!=2)fs+="x+=uMatDiffuse+uMatAmbient+uMatEmissive;";
 fs+="for(int i=0;i<4;++i)x+=vec4(uLightDir[i]+uLightPos[i],float(uLightType[i]))*.03+(uLightDiffuse[i]+uLightAmbient[i]+uLightAtten[i])*.02;color=vec4(fract(x.rgb*.17+float(uNumLights)*.03),1);}\n";
 GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs),program=glCreateProgram();
 glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);check(ok,"fixture link");
 glDeleteShader(v);glDeleteShader(f);glUniformBlockBinding(program,glGetUniformBlockIndex(program,"ViewProjBlock"),0);
 WebGLPipeline::ProgramInfo p;p.prog=program;
#define LOC(name) p.name=glGetUniformLocation(program,#name)
 LOC(uWorld);LOC(uTexMat0);LOC(uTexMat1);LOC(uViewportPos);LOC(uYFlip);LOC(uTFactor);LOC(uAlphaRef);LOC(uFogColor);LOC(uFogParams);
 LOC(uMatDiffuse);LOC(uMatAmbient);LOC(uMatEmissive);LOC(uGlobalAmbient);LOC(uNumLights);
 LOC(uLightType);LOC(uLightDir);LOC(uLightPos);LOC(uLightDiffuse);LOC(uLightAmbient);LOC(uLightAtten);
#undef LOC
 return p;
}
static std::vector<float> values(GLuint program){
 GLint n=0;glGetProgramiv(program,GL_ACTIVE_UNIFORMS,&n);std::vector<float> out;
 for(GLint i=0;i<n;++i){
  char name[128];GLint size=0;GLenum type=0;glGetActiveUniform(program,i,sizeof(name),nullptr,&size,&type,name);
  const int components=type==GL_FLOAT_MAT4 ? 16:type==GL_FLOAT_VEC4 ? 4:type==GL_FLOAT_VEC3 ? 3:type==GL_FLOAT_VEC2 ? 2:1;
  std::string base=name;const auto bracket=base.find('[');if(bracket!=std::string::npos)base.resize(bracket);
  for(int j=0;j<size;++j){
   const std::string element=size>1 ? base+"["+std::to_string(j)+"]":base;
   const GLint loc=glGetUniformLocation(program,element.c_str());if(loc<0)continue; // UBO is checked by pixels.
   float v[16]={};glGetUniformfv(program,loc,v);out.insert(out.end(),v,v+components);
  }
 }
 return out;
}
static DWORD bits(float v){DWORD d;memcpy(&d,&v,4);return d;}
static void mutate(WebGLDevice &d,int step){
 for(auto t:{D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION,D3DTS_TEXTURE0,D3DTRANSFORMSTATETYPE(D3DTS_TEXTURE0+1)}){
  float *v=reinterpret_cast<float *>(&d.transforms[t]);for(int k=0;k<16;++k)v[k]=k%5==0 ? 1:0;
 }
 d.transforms[D3DTS_WORLD]._41=float(step%7)*.02f;
 d.transforms[D3DTS_TEXTURE0]._41=float((step/3)%7)*.13f;
 d.transforms[D3DTS_TEXTURE0+1]._42=float((step/5)%7)*.17f;
 d.transforms[D3DTS_VIEW]._42=float((step/7)%7)*.01f;
 d.transforms[D3DTS_PROJECTION]._11=1+float((step/11)%7)*.01f;
 d.viewport.X=(step/13)%4;d.viewport.Y=(step/17)%4;
 d.state[D3DRS_TEXTUREFACTOR]=step%4 ? 0xff102030u:0;d.state[D3DRS_ALPHAREF]=(step/3)%256;
 d.state[D3DRS_FOGCOLOR]=step%3 ? 0x12345678u:0;d.state[D3DRS_FOGSTART]=bits(float(step%5));d.state[D3DRS_FOGEND]=bits(float(step%9));
 d.state[D3DRS_AMBIENT]=step%7 ? 0xff345678u:0;
 d.material.Diffuse={float(step%4)*.1f,.2f,.3f,1};d.material.Ambient={.1f,float(step%3)*.1f,.2f,1};d.material.Emissive={.1f,.2f,float(step%5)*.1f,1};
 for(unsigned i=0;i<8;++i){
  d.enabled[i]=((step+i)%3)!=0;auto &l=d.lights[i];l.Type=i%2 ? D3DLIGHT_POINT:D3DLIGHT_DIRECTIONAL;
  l.Direction={.1f*float(i),.2f,.3f};l.Position={float(step%7),float(i),.5f};
  l.Diffuse={.1f,.2f,.3f,1};l.Ambient={.2f,.3f,.4f,1};l.Range=10+step%13;
  l.Attenuation0=step%2 ? .5f:0;l.Attenuation1=.2f;l.Attenuation2=.01f;
 }
}
int main(){
 EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);check(eglInitialize(d,nullptr,nullptr),"EGL");
 const EGLint a[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_NONE};
 EGLConfig cfg;EGLint n=0;check(eglChooseConfig(d,a,&cfg,1,&n)&&n==1,"config");
 const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE},sa[]={EGL_WIDTH,2,EGL_HEIGHT,2,EGL_NONE};
 EGLContext context=eglCreateContext(d,cfg,EGL_NO_CONTEXT,ca);EGLSurface surface=eglCreatePbufferSurface(d,cfg,sa);
 check(eglMakeCurrent(d,surface,surface,context),"current");printf("GPU %s\n",glGetString(GL_RENDERER));
 GXMultiview mv;mv.resolver=[](const char *name)->void *{return reinterpret_cast<void *>(eglGetProcAddress(name));};check(mv.available(),"OVR multiview");
 GLuint fbo[4],tex[2];glGenFramebuffers(4,fbo);glGenTextures(2,tex);
 glBindTexture(GL_TEXTURE_2D,tex[0]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,32,32,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
 glBindFramebuffer(GL_FRAMEBUFFER,fbo[0]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex[0],0);
 glBindTexture(GL_TEXTURE_2D_ARRAY,tex[1]);mv.storage(GL_TEXTURE_2D_ARRAY,1,GL_RGBA8,32,32,2);
 glBindFramebuffer(GL_FRAMEBUFFER,fbo[1]);mv.attach(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,tex[1],0,0,2);
 for(int eye=0;eye<2;++eye){glBindFramebuffer(GL_FRAMEBUFFER,fbo[eye+2]);mv.layer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,tex[1],0,eye);}
 for(auto f:fbo){glBindFramebuffer(GL_FRAMEBUFFER,f);check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"FBO");}
 GLuint ubo;glGenBuffers(1,&ubo);glBindBuffer(GL_UNIFORM_BUFFER,ubo);glBufferData(GL_UNIFORM_BUFFER,128,nullptr,GL_DYNAMIC_DRAW);glBindBufferBase(GL_UNIFORM_BUFFER,0,ubo);
 auto p0=makeProgram(0),p1=makeProgram(1),p2=makeProgram(2);WebGLPipeline::ProgramInfo *programs[]={&p0,&p1,&p2};
 check(p2.uMatDiffuse<0 && p2.uNumLights>=0,"independently optimized-out material uniforms");
 glViewport(0,0,32,32);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glClearColor(0,0,0,0);
 std::vector<std::vector<float>> referenceValues;std::vector<std::vector<unsigned char>> referencePixels;
 unsigned counts[2]={};
 for(int cached=0;cached<2;++cached){
  WebGLPipeline pipe;pipe.m_xrMode=cached;pipe.m_offFBO=fbo[0];pipe.m_viewProjUBO=ubo;
  for(auto p:programs)p->xrUniforms={};
  const unsigned before=uploads;
  for(int step=0;step<96;++step){
   WebGLDevice device;mutate(device,step);pipe.m_yFlip=step%2 ? -1:1;
   for(int rep=0;rep<3;++rep)for(int kind:{0,1,0,2,0}){
    auto &p=*programs[kind];
    if(rep==1 && kind==2){ // Simulate foreign uniform changes and explicit ownership handoff.
     glUseProgram(p0.prog);glUniform1f(p0.uAlphaRef,.987f);pipe.invalidateCachedGLState();
    }
    pipe.applyUniforms(&device,&p,0);
    GLint current=0;glGetIntegerv(GL_CURRENT_PROGRAM,&current);check(GLuint(current)==p.prog,"program binding");
    auto actual=values(p.prog);
    const size_t index=referenceValues.size();
    if(!cached)referenceValues.push_back(actual);
    else {static size_t pos=0;check(actual==referenceValues[pos++],"all GPU uniform values match legacy policy");}
    (void)index;
    glBindFramebuffer(GL_FRAMEBUFFER,fbo[kind==1 ? 1:0]);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);
    std::vector<unsigned char> pixels((kind==1 ? 2:1)*32*32*4);
    for(int eye=0;eye<(kind==1 ? 2:1);++eye){
     glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo[kind==1 ? eye+2:0]);
     glReadPixels(0,0,32,32,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data()+eye*32*32*4);
    }
    if(!cached)referencePixels.push_back(pixels);
    else {static size_t pos=0;check(pixels==referencePixels[pos++],"ordinary/UI/multiview eye pixels unchanged");}
    check(glGetError()==GL_NO_ERROR,"uniform/draw GL state");
   }
  }
  counts[cached]=uploads-before;
 }
 check(counts[1]<counts[0],"actual uniform upload reduction");
 printf("PASS %u uniform-state/pixel checks; legacy=%u cached=%u glUniform calls (%.1f percent fewer)\n",checks,counts[0],counts[1],100.0*(counts[0]-counts[1])/counts[0]);
 // Isolate submission cost with static materials, moving objects and repeated
 // ordinary -> multiview -> ordinary bindings. Wall time is NOT game FPS.
 for(int round=0;round<4;++round)for(int order=0;order<2;++order){
  const bool cached=(round+order)%2;WebGLPipeline pipe;pipe.m_xrMode=cached;pipe.m_viewProjUBO=ubo;
  for(auto p:programs)p->xrUniforms={};
  WebGLDevice device;mutate(device,17);
  const unsigned before=uploads;glFinish();const auto start=std::chrono::steady_clock::now();
  for(int draw=0;draw<1800;++draw){device.transforms[D3DTS_WORLD]._41=float(draw%31)*.001f;
   auto &p=*programs[draw%3==1 ? 1:0];pipe.applyUniforms(&device,&p,0);
   glBindFramebuffer(GL_FRAMEBUFFER,fbo[draw%3==1 ? 1:0]);glDrawArrays(GL_TRIANGLES,0,3);
  }
  glFinish();const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  check(glGetError()==GL_NO_ERROR,"benchmark GL");printf("BENCH round=%d cache=%d 1800 submissions wall=%.3f ms uploads=%u\n",round,cached,ms,uploads-before);
 }
 glDeleteProgram(p0.prog);glDeleteProgram(p1.prog);glDeleteProgram(p2.prog);glDeleteBuffers(1,&ubo);glDeleteTextures(2,tex);glDeleteFramebuffers(4,fbo);
 eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroySurface(d,surface);eglDestroyContext(d,context);eglTerminate(d);
 printf("PASS %u total uniform-cache checks\n",checks);
}
