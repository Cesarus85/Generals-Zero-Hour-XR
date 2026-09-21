// GeneralsX @feature Codex 16/09/2026 Bounded, opt-in LAN CRC observations; no simulation or packet writes.
#pragma once

#include <stdio.h>
#include <stdlib.h>

namespace GXLanCRCTrace
{
enum { kFirstCheckpoints = 8, kMaxSlots = 16, kMaxObjectRecords = 2048 };

enum Reason { no_mismatch, missing_crc, different_crc };

struct PeerCRC
{
	int playerIndex;
	int networkSlot;
	unsigned int crc;
};

struct Stages
{
	unsigned int objects;
	unsigned int rng;
	unsigned int partition;
	unsigned int players;
	unsigned int ai;
};

struct ObjectCRC
{
	unsigned int id;
	unsigned int crc;
};

struct State
{
	bool enabled;
	bool armed;
	unsigned int generated;
	unsigned int validated;
	bool failureWritten;
	State() : enabled(false), armed(false), generated(0), validated(0), failureWritten(false) {}
};

inline State &state()
{
	static State value;
	return value;
}

inline bool markerEnabled()
{
	const char *env = getenv("GX_LAN_CRC");
	if (env && env[0] && env[0] != '0') return true;
	FILE *marker = fopen("gx_lan_crc.txt", "r");
	if (!marker) return false;
	fclose(marker);
	return true;
}

inline const char *buildPlatform()
{
#if defined(__ANDROID__) && defined(__aarch64__)
	return "android-arm64";
#elif defined(__APPLE__) && defined(__aarch64__)
	return "apple-arm64";
#elif defined(__linux__) && defined(__x86_64__)
	return "linux-x86_64";
#elif defined(_WIN32)
	return "windows";
#else
	return "other";
#endif
}

inline void endMatch()
{
	state() = State();
}

inline void beginMatch(bool lan, unsigned int mapCRC, int seed, int interval)
{
	endMatch();
	State &s = state();
	s.enabled = lan && markerEnabled();
	if (s.enabled) {
		fprintf(stderr, "[GX-LAN-CRC] begin game=ZeroHour mode=LAN platform=%s map_crc=%08X game_seed=%d crc_interval=%d first_limit=%d\n",
			buildPlatform(), mapCRC, seed, interval, kFirstCheckpoints);
		fflush(stderr);
	}
}

inline bool captureGeneration()
{
	return state().enabled && state().armed && state().generated < kFirstCheckpoints;
}

inline void armGeneration()
{
	state().armed = state().enabled && state().generated < kFirstCheckpoints;
}

// GeneralsX @feature Codex 21/09/2026 Capture bounded per-object CRC observations in the production traversal.
inline void observeObject(ObjectCRC *records, int capacity, int &captured, int &total,
	unsigned int id, unsigned int crc)
{
	++total;
	if (!records || capacity <= 0 || captured < 0 || captured >= capacity) return;
	records[captured].id = id;
	records[captured].crc = crc;
	++captured;
}

inline void generated(int frame, int localSlot, unsigned int crc, unsigned int rngSeedCRC, const Stages &stages,
	const ObjectCRC *objects, int capturedObjects, int totalObjects)
{
	State &s = state();
	if (!captureGeneration()) return;
	s.armed = false;
	++s.generated;
	fprintf(stderr,
		"[GX-LAN-CRC] generated frame=%d local_slot=%d crc=%08X objects=%08X rng=%08X partition=%08X players=%08X ai=%08X rng_seed_crc=%08X\n",
		frame, localSlot, crc, stages.objects, stages.rng, stages.partition, stages.players, stages.ai, rngSeedCRC);
	if (capturedObjects < 0) capturedObjects = 0;
	if (capturedObjects > kMaxObjectRecords) capturedObjects = kMaxObjectRecords;
	if (totalObjects < capturedObjects) totalObjects = capturedObjects;
	fprintf(stderr,
		"[GX-LAN-CRC] object-summary frame=%d total=%d captured=%d truncated=%d limit=%d\n",
		frame, totalObjects, capturedObjects, totalObjects > capturedObjects ? 1 : 0, kMaxObjectRecords);
	for (int i = 0; objects && i < capturedObjects; ++i)
	{
		fprintf(stderr, "[GX-LAN-CRC] object frame=%d order=%d id=%08X crc=%08X\n",
			frame, i, objects[i].id, objects[i].crc);
	}
	fflush(stderr);
}

inline Reason classify(const int *connected, int connectedCount, const PeerCRC *received, int receivedCount)
{
	if (connectedCount < 0 || receivedCount < 0) return missing_crc;
	unsigned int reference = 0;
	bool haveReference = false;
	bool missing = false;
	bool different = false;
	for (int i = 0; i < connectedCount && i < kMaxSlots; ++i) {
		bool found = false;
		for (int j = 0; j < receivedCount && j < kMaxSlots; ++j) {
			if (received[j].networkSlot != connected[i]) continue;
			found = true;
			if (!haveReference) { reference = received[j].crc; haveReference = true; }
			else if (reference != received[j].crc) different = true;
			break;
		}
		if (!found) missing = true;
	}
	return missing ? missing_crc : different ? different_crc : no_mismatch;
}

inline const char *reasonName(Reason reason)
{
	return reason == missing_crc ? "missing_crc" : reason == different_crc ? "different_crc" : "none";
}

inline void checkpoint(int validationFrame, int localSlot, const int *connected, int connectedCount,
	const PeerCRC *received, int receivedCount, Reason detectorReason)
{
	State &s = state();
	if (!s.enabled) return;
	const Reason reason = classify(connected, connectedCount, received, receivedCount);
	const bool detectorMismatch = detectorReason != no_mismatch;
	const bool terminal = detectorMismatch && !s.failureWritten;
	if (s.validated >= kFirstCheckpoints && !terminal) return;
	if (s.validated < kFirstCheckpoints) ++s.validated;
	if (terminal) s.failureWritten = true;
	fprintf(stderr, "[GX-LAN-CRC] %s validation_frame=%d local_slot=%d reason=%s detector_reason=%s connected=",
		terminal ? "failure" : "checkpoint", validationFrame, localSlot, reasonName(reason), reasonName(detectorReason));
	for (int i = 0; i < connectedCount && i < kMaxSlots; ++i)
		fprintf(stderr, "%s%d", i ? "," : "", connected[i]);
	fprintf(stderr, " received=");
	for (int i = 0; i < receivedCount && i < kMaxSlots; ++i)
		fprintf(stderr, "%ss%d/p%d:%08X", i ? "," : "", received[i].networkSlot,
			received[i].playerIndex, received[i].crc);
	fprintf(stderr, "\n");
	fflush(stderr);
}
}
