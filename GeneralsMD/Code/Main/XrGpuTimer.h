// GeneralsX @performance Codex 14/09/2026 Nonblocking EXT timer query ring.
// No GL dependency here: production scheduling also runs with host API spies.
#pragma once
#include <cstdint>

struct XrGpuTimerApi {
	void (*gen)(int,unsigned *)=nullptr;
	void (*destroy)(int,const unsigned *)=nullptr;
	void (*begin)(unsigned,unsigned)=nullptr;
	void (*end)(unsigned)=nullptr;
	void (*integer)(unsigned,int *)=nullptr;
	void (*query)(unsigned,unsigned,int *)=nullptr;
	void (*available)(unsigned,unsigned,unsigned *)=nullptr;
	void (*result)(unsigned,unsigned,std::uint64_t *)=nullptr;
};
struct XrGpuTimer {
	static constexpr unsigned elapsed=0x88BF,disjoint=0x8FBB,counterBits=0x8864;
	static constexpr unsigned resultAvailable=0x8867,queryResult=0x8866;
	static constexpr int capacity=8;
	struct Slot {unsigned id=0,epoch=0;bool pending=false,valid=false;} slots[capacity];
	XrGpuTimerApi api;
	int active=-1;bool supported=false;unsigned dropped=0,disjoints=0;
	struct Reading {double totalMs=0;unsigned count=0;};
	bool init(XrGpuTimerApi functions) {
		shutdown();api=functions;
		if(!api.gen || !api.destroy || !api.begin || !api.end || !api.integer ||
			!api.query || !api.available || !api.result)return false;
		int bits=0;api.query(elapsed,counterBits,&bits);if(bits<30)return false;
		unsigned ids[capacity]={};api.gen(capacity,ids);
		for(int i=0;i<capacity;++i)slots[i].id=ids[i];
		for(auto &s:slots)if(!s.id){shutdown();return false;}
		int ignored=0;api.integer(disjoint,&ignored);supported=true;return true;
	}
	bool begin(unsigned epoch) {
		if(!supported || active>=0)return false;
		for(int i=0;i<capacity;++i)if(!slots[i].pending) {
			active=i;slots[i].epoch=epoch;slots[i].valid=true;
			api.begin(elapsed,slots[i].id);return true;
		}
		++dropped;return false; // Never wait for or overwrite an in-flight result.
	}
	void end(bool valid=true) {
		if(active<0)return;
		api.end(elapsed);slots[active].pending=true;slots[active].valid=valid;active=-1;
	}
	Reading poll(unsigned epoch,bool enabled=true) {
		// GeneralsX @performance Codex 14/09/2026 OFF must issue no driver
		// queries. Retain in-flight slots until re-enabled; epoch checks reject
		// their stale values. Never wait for them or reuse a pending query.
		Reading reading;if(!enabled || !supported || active>=0)return reading;
		bool pending=false;for(const auto &s:slots)pending=pending || s.pending;
		if(!pending)return reading;
		int before=0;api.integer(disjoint,&before);
		if(before){++disjoints;for(auto &s:slots)s.valid=false;}
		for(auto &s:slots)if(s.pending) {
			unsigned ready=0;api.available(s.id,resultAvailable,&ready);if(!ready)continue;
			// Available was checked first; GL_QUERY_RESULT never blocks this thread.
			if(s.valid && s.epoch==epoch) {
				std::uint64_t ns=0;api.result(s.id,queryResult,&ns);
				// >=1s samples may overflow the minimum 30-bit counter; reject stalls.
				if(ns>0 && ns<1000000000ULL){reading.totalMs+=ns/1000000.0;++reading.count;}
			}
			s.pending=false;
		}
		int after=0;api.integer(disjoint,&after);
		if(after){++disjoints;for(auto &s:slots)s.valid=false;reading={};}
		return reading;
	}
	void shutdown() {
		end(false);
		for(auto &s:slots){if(s.id && api.destroy)api.destroy(1,&s.id);s={};}
		supported=false;
	}
};
