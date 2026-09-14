// GeneralsX @test Codex 14/09/2026 Real OpenAL stopped-source lifetime.
// Uses a silent loopback context and the production release helper.
#define AL_ALEXT_PROTOTYPES 1
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <cstdio>
#include <cstdlib>
static int checks=0;
static void check(bool b){++checks;if(!b){fprintf(stderr,"OpenAL device check %d failed\n",checks);exit(1);}}
struct PlayingAudio{ALuint m_source=0,m_bufferHandle=0;};
struct OpenALAudioManager {
 int leases=0;ALuint source=0;
 void releaseSampleBuffer(PlayingAudio *);
 void closeBuffer(ALuint){ALint bound=-1;alGetSourcei(source,AL_BUFFER,&bound);check(bound==0 && leases>0);--leases;}
};
#include "audio-release.inc"
int main(){
 ALCdevice *device=alcLoopbackOpenDeviceSOFT(nullptr);check(device!=nullptr);
 const ALCint attrs[]={ALC_FORMAT_CHANNELS_SOFT,ALC_STEREO_SOFT,ALC_FORMAT_TYPE_SOFT,ALC_SHORT_SOFT,ALC_FREQUENCY,48000,0};
 ALCcontext *context=alcCreateContext(device,attrs);check(context && alcMakeContextCurrent(context));
 printf("OpenAL %s / %s\n",alGetString(AL_VERSION),alGetString(AL_RENDERER));
 PlayingAudio p;alGenSources(1,&p.m_source);OpenALAudioManager manager;manager.source=p.m_source;
 const short pcm[128]={};
 for(int i=0;i<300;++i){
  alGenBuffers(1,&p.m_bufferHandle);const ALuint old=p.m_bufferHandle;
  alBufferData(old,AL_FORMAT_MONO16,pcm,sizeof(pcm),48000);alSourcei(p.m_source,AL_BUFFER,old);
  alSourcePlay(p.m_source);alSourceStop(p.m_source);check(alGetError()==AL_NO_ERROR);
  alDeleteBuffers(1,&old);check(alGetError()==AL_INVALID_OPERATION && alIsBuffer(old));
  manager.leases=1;manager.releaseSampleBuffer(&p);check(!p.m_bufferHandle && !manager.leases && alGetError()==AL_NO_ERROR);
  manager.releaseSampleBuffer(&p);check(!manager.leases && alGetError()==AL_NO_ERROR);
  alDeleteBuffers(1,&old);check(alGetError()==AL_NO_ERROR && !alIsBuffer(old));
 }
 alDeleteSources(1,&p.m_source);alcMakeContextCurrent(nullptr);alcDestroyContext(context);alcCloseDevice(device);
 printf("PASS %d real OpenAL source-release/eviction checks\n",checks);
}
