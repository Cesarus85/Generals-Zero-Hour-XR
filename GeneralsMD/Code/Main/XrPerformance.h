// GeneralsX @performance Codex 14/09/2026 Session-only shadow A/B and timing.
#pragma once
#include "XrStrings.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>

inline double xrPerfNow() {
	return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
struct XrPerfMean {
	double sum=0,maximum=0;unsigned count=0;
	void add(double ms){if(ms>=0 && ms<1000){sum+=ms;maximum=std::max(maximum,ms);++count;}}
	double mean() const {return count ? sum/count:0;}
};
struct XrPerformance {
	// Intentionally not persisted: experiments must not silently alter a later run.
	bool volumeShadows=false,enabled=false,atlasStereo=false;
	// GeneralsX @feature Codex 14/09/2026 P16 preferred Quest path; native capability/shader fallback retained.
	bool multiviewStereo=true;
	bool elideWorldCopy=true;
	// P26-1 is deliberately session-only and opt-in until its visual and
	// performance gates pass on a physical Quest.
	bool cosmeticCulling=false;
	unsigned epoch=1;int settle=30;
	std::string key,report;
	double started=0;
	XrPerfMean engine,eyes,wait,frame,gpu;
	void invalidate() {
		++epoch;settle=30;started=0;report.clear();
		engine={};eyes={};wait={};frame={};gpu={};
	}
	bool prepare(const std::string &settings,bool stable) {
		if(key!=settings){key=settings;invalidate();}
		if(!enabled || !stable){invalidate();return false;}
		if(settle>0){--settle;return false;}
		return true;
	}
	std::string status() const {
		if(!enabled)return xrTr("Schatten A/B nur diese Sitzung; B ohne Volumen");
		return report.empty() ? xrTr("Messung: Tabletop öffnen, kurz ruhig halten"):report;
	}
};
