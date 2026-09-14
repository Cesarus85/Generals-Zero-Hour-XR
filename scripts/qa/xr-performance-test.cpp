// GeneralsX @test Codex 14/09/2026 Actual timer ring with bounded fake driver.
#include "XrPerformance.h"
#include "XrGpuTimer.h"
#include "XrWorld.h"
#include "XRWorldElision.h"
#include <cstdlib>
static int checks=0;
static void check(bool value){++checks;if(!value){fprintf(stderr,"FAIL performance %d\n",checks);exit(1);}}
static unsigned active=0,results=0,deleted=0;
static bool ready[9]={};static int bits=64,disjointChecks=0,disjointAt=0;
static std::uint64_t nanos=20000000;
static void gen(int n,unsigned *ids){for(int i=0;i<n;++i)ids[i]=unsigned(i+1);}
static void destroy(int,const unsigned *){++deleted;}
static void begin(unsigned target,unsigned id){check(target==XrGpuTimer::elapsed && active==0);active=id;ready[id]=false;}
static void end(unsigned target){check(target==XrGpuTimer::elapsed && active!=0);active=0;}
static void integer(unsigned target,int *value){check(target==XrGpuTimer::disjoint);*value=(++disjointChecks==disjointAt);}
static void query(unsigned,unsigned,int *value){*value=bits;}
static void available(unsigned id,unsigned,unsigned *value){*value=ready[id];}
static void result(unsigned id,unsigned,std::uint64_t *value){check(ready[id] && !active);++results;*value=nanos;}
static void allReady(){for(auto &r:ready)r=true;}
int main(){
	// Every current-frame prerequisite is independently necessary; no previous
	// frame's success can enable omission by itself.
	for(unsigned mask=0;mask<64;++mask) {
		GXWorldElision e;e.requested=mask&1;e.blocked=mask&2;
		check(e.canOmit(mask&4,mask&8,mask&16,mask&32)==((mask&1) && !(mask&2) && (mask&60)==60));
	}
	for(bool eyes:{false,true})for(bool ui:{false,true}) {
		GXWorldElision e;e.configure(true);e.omit();e.omit();e.publish(eyes,ui);
		check(!e.composedComplete && e.publishedSkipped==2 && e.blocked==(!eyes || !ui));
		check(!e.pendingMissing && e.pendingSkipped==0);
		e.configure(true);check(!e.composedComplete);
		e.requireFullWorld();check(!e.canOmit(true,true,true,true));
		e.publish(false,false);check(!e.composedComplete && e.publishedSkipped==0 && e.blocked);
		e.fullClear();e.publish(false,false);check(e.composedComplete && e.blocked);
		e.configure(false);check(!e.blocked && !e.requested);
		e.configure(true);check(e.canOmit(true,true,true,true));
	}
	XrPerformance p;check(!p.volumeShadows && !p.enabled && !XrWorldFrame{}.volumeShadows && p.multiviewStereo && XrWorldFrame{}.multiviewStereo);
	check(!p.prepare("A",true));p.enabled=true;
	for(int i=0;i<30;++i)check(!p.prepare("A",true));check(p.prepare("A",true));
	p.report="old";p.engine.add(20);const auto old=p.epoch;
	check(!p.prepare("B",true));check(p.epoch>old && p.report.empty() && p.engine.count==0);
	for(int i=0;i<29;++i)check(!p.prepare("B",true));check(p.prepare("B",true));
	check(!p.prepare("B",false));check(p.settle==30);
	p.enabled=false;check(!p.prepare("B",true));
	XrPerfMean mean;mean.add(-1);mean.add(1000);check(mean.count==0);mean.add(10);mean.add(30);check(mean.mean()==20 && mean.maximum==30);
	XrGpuTimer t;check(!t.init({}));check(!t.begin(1));t.end();check(t.poll(1).count==0);
	XrGpuTimerApi api{gen,destroy,begin,end,integer,query,available,result};
	bits=0;check(!t.init(api));bits=64;check(t.init(api));
	const int idleChecks=disjointChecks;check(!t.poll(1).count && disjointChecks==idleChecks);
	check(t.begin(1));check(!t.begin(1));check(t.poll(1).count==0);t.end();
	check(t.poll(1).count==0 && results==0);ready[1]=true;
	auto r=t.poll(1);check(r.count==1 && r.totalMs==20 && results==1);
	check(t.begin(1));t.end();allReady();check(t.poll(2).count==0 && results==1); // Epoch isolation.
	check(t.begin(2));t.end(false);allReady();check(t.poll(2).count==0 && results==1); // Movie cancellation.
	for(int i=0;i<8;++i){check(t.begin(2));t.end();}check(!t.begin(2) && t.dropped==1);
	allReady();disjointAt=disjointChecks+1;check(t.poll(2).count==0 && t.disjoints==1 && results==1);
	check(t.begin(2));t.end();allReady();disjointAt=disjointChecks+2;
	check(t.poll(2).count==0 && t.disjoints==2); // Disjoint occurring during readout.
	nanos=0;check(t.begin(2));t.end();allReady();check(t.poll(2).count==0);
	nanos=1000000000;check(t.begin(2));t.end();allReady();check(t.poll(2).count==0);
	nanos=5000000;check(t.begin(2));t.end();allReady();check(t.poll(2).totalMs==5);
	check(t.begin(2));t.end();allReady();const int offChecks=disjointChecks;const unsigned offResults=results;
	for(int i=0;i<100;++i)check(t.poll(3,false).count==0);
	check(disjointChecks==offChecks && results==offResults && t.slots[0].pending);
	check(t.poll(3,true).count==0 && !t.slots[0].pending && results==offResults);
	check(t.begin(3));t.shutdown();check(active==0 && deleted==8 && !t.supported);t.shutdown();check(deleted==8);
	printf("PASS %d performance/timer checks\n",checks);
}
