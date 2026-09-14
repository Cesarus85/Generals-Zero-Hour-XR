// GeneralsX @test Codex 14/09/2026 Real cache methods, fixture decoder/files/AL.
#include <map>
#include <list>
#include <limits>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
using ALuint=unsigned;using UnsignedInt=unsigned;using Int=int;using Bool=bool;
constexpr bool TRUE=true,FALSE=false;
constexpr int AL_BUFFER=1;
static int checks=0,decoded=0,closed=0,stopped=0,detached=0;
static void check(bool b){++checks;if(!b){fprintf(stderr,"audio check %d failed\n",checks);exit(1);}}
#define DEBUG_CRASH(x) ((void)0)
#define DEBUG_ASSERTLOG(x,y) ((void)0)
#define DEBUG_LOG(x) ((void)0)
static std::map<ALuint,bool> buffers;static std::map<ALuint,ALuint> sources;
static void alGenBuffers(int,ALuint *b){static ALuint id=1;buffers[*b=id++]=true;}
static void alDeleteBuffers(int,const ALuint *b){for(auto s:sources)if(s.second==*b)throw std::runtime_error("bound buffer deletion");buffers.erase(*b);}
static void alSourceStop(ALuint){++stopped;}
static void alSourcei(ALuint s,int,ALuint b){sources[s]=b;++detached;}
struct AsciiString:std::string {using std::string::string;const char *str()const{return c_str();}bool isEmpty()const{return empty();}};
struct AudioEventInfo {int m_priority=0;};
enum {PP_Attack,PP_Sound,PP_Decay,PP_Done};
struct AudioEventRTS {
 int portion=PP_Sound;AsciiString name="sound",attack="attack",decay="decay";AudioEventInfo info;
 int getNextPlayPortion(){return portion;}AsciiString getFilename(){return name;}
 AsciiString getAttackFilename(){return attack;}AsciiString getDecayFilename(){return decay;}
 const AudioEventInfo *getAudioEventInfo(){return &info;}bool isPositionalAudio(){return false;}
};
struct File {bool valid=true;unsigned size(){return 10;}};
struct FileSystem {File *openFile(const char *n){if(std::string(n)=="missing")return nullptr;return new File{std::string(n)!="bad"};}} fs;
static FileSystem *TheFileSystem=&fs;
struct FFmpegFile {bool open(File *f){bool valid=f->valid;delete f;return valid;}int getNumChannels(){return 1;}void close(){++closed;}};
struct OpenAudioFile {ALuint m_buffer=0;FFmpegFile *m_ffmpegFile=nullptr;unsigned m_openCount=0,m_fileSize=0;const AudioEventInfo *m_eventInfo=nullptr;};
struct OpenFileInfo {AsciiString *filename=nullptr;AudioEventRTS *event=nullptr;explicit OpenFileInfo(AsciiString *s):filename(s){} explicit OpenFileInfo(AudioEventRTS *e):event(e){}};
using OpenFilesHash=std::map<AsciiString,OpenAudioFile>;using OpenFilesHashIt=OpenFilesHash::iterator;
struct OpenALAudioFileCache {
 OpenFilesHash m_openFiles;unsigned m_currentlyUsedSize=0,m_maxSize=20;bool failDecode=false;
 ALuint getBufferForFile(const OpenFileInfo &);void closeBuffer(ALuint);void releaseOpenAudioFile(OpenAudioFile *);
 Bool freeEnoughSpaceForSample(const OpenAudioFile &);
 bool decodeFFmpeg(OpenAudioFile *){++decoded;return !failDecode;}
};
struct PlayingAudio {ALuint m_source=0,m_bufferHandle=0;};
struct OpenALAudioManager {
 OpenALAudioFileCache *cache;PlayingAudio *active=nullptr;int evictions=0;
 void closeBuffer(ALuint b){cache->closeBuffer(b);}
 void releaseSampleBuffer(PlayingAudio *);
 void closeAnySamplesUsingFile(const void *p){++evictions;if(active && active->m_bufferHandle==uintptr_t(p))releaseSampleBuffer(active);}
};
static OpenALAudioManager *TheAudio=nullptr;
#include "audio-production.inc"
int main(){
 OpenALAudioFileCache c;OpenALAudioManager manager{&c};TheAudio=&manager;
 AsciiString a="a",b="b",other="other",bad="bad",missing="missing";
 auto open=[&](AsciiString &name){return c.getBufferForFile(OpenFileInfo(&name));};
 ALuint first=open(a);check(first && c.m_openFiles[a].m_openCount==1);
 check(open(a)==first && c.m_openFiles[a].m_openCount==2 && decoded==1);
 c.closeBuffer(first);check(c.m_openFiles[a].m_openCount==1);
 c.closeBuffer(first);c.closeBuffer(first);c.closeBuffer(0);c.closeBuffer(999);check(c.m_openFiles[a].m_openCount==0);
 PlayingAudio sample{42,open(a)};sources[42]=sample.m_bufferHandle;
 bool rejected=false;try{alDeleteBuffers(1,&sample.m_bufferHandle);}catch(const std::runtime_error &){rejected=true;}check(rejected);
 manager.releaseSampleBuffer(&sample);check(sample.m_bufferHandle==0 && sources[42]==0 && c.m_openFiles[a].m_openCount==0 && stopped==1 && detached==1);
 manager.releaseSampleBuffer(&sample);check(stopped==1 && detached==1);
 ALuint second=open(b);check(second && c.m_currentlyUsedSize==20);
 ALuint third=open(other);check(third && c.m_openFiles.count(a)==0 && buffers.count(first)==0 && c.m_currentlyUsedSize==20);
 check(!open(bad) && !open(missing) && c.m_currentlyUsedSize==20 && buffers.size()==2);
 AsciiString failure="decode-fail";c.failDecode=true;check(!open(failure) && buffers.size()==2);c.failDecode=false;
 AsciiString full="full";check(!open(full) && buffers.size()==2 && c.m_currentlyUsedSize==20);
 c.closeBuffer(second);c.closeBuffer(third);
 // Attack/sound/decay changes release the bound buffer before acquiring the
 // next lease; a one-sample budget forces actual eviction at every transition.
 for(auto &entry:c.m_openFiles)c.releaseOpenAudioFile(&entry.second);c.m_openFiles.clear();c.m_currentlyUsedSize=0;c.m_maxSize=10;
 AudioEventRTS event;
 for(int i=0;i<300;++i){
  event.portion=i%3;sample.m_bufferHandle=c.getBufferForFile(OpenFileInfo(&event));sources[42]=sample.m_bufferHandle;
  check(sample.m_bufferHandle && c.m_openFiles.size()==1 && buffers.size()==1);
  manager.releaseSampleBuffer(&sample);check(sample.m_bufferHandle==0 && sources[42]==0 && c.m_openFiles.begin()->second.m_openCount==0);
 }
 // Active lower-priority sounds may be stopped by eviction, but their
 // source must be detached before the cache destroys the buffer.
 event.portion=PP_Sound;event.info.m_priority=1;sample.m_bufferHandle=c.getBufferForFile(OpenFileInfo(&event));sources[42]=sample.m_bufferHandle;manager.active=&sample;
 AudioEventRTS important;important.name="important";important.info.m_priority=2;
 ALuint high=c.getBufferForFile(OpenFileInfo(&important));check(high && manager.evictions==1 && sample.m_bufferHandle==0 && sources[42]==0);
 c.closeBuffer(high);for(auto &entry:c.m_openFiles)c.releaseOpenAudioFile(&entry.second);
 check(buffers.empty() && closed>300);
 printf("PASS %d production audio lease/eviction/rebind checks; %d decodes\n",checks,decoded);
}
