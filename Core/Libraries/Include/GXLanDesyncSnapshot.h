// GeneralsX @feature Codex 22/09/2026 Opt-in bounded observers of the existing LAN CRC and command paths.
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <new>

namespace GXLanDesyncSnapshot
{
enum { kFrames = 8, kObjects = 2048, kFields = 11, kCommands = 4096, kArgs = 32, kName = 96 };
inline const char *fieldName(int i)
{
	static const char *names[kFields] = { "private_status", "transform", "id", "upgrades",
		"experience", "health", "weapon_bonus", "damage_scalar", "weapon_0", "weapon_1", "weapon_2" };
	return names[i];
}
struct Object
{
	unsigned int id, start, crc, fields[kFields], mask, transform[12];
	int transformCount, nameCut;
	char name[kName];
};
struct Frame
{
	int frame, slot, total, captured;
	unsigned int crc, stages[5], rngCRC, rng[6];
	bool rngPresent;
	unsigned long long commandsBefore;
	Object objects[kObjects];
};
struct Arg { int type, count; unsigned int words[4]; };
struct Command
{
	unsigned long long sequence;
	int frame, player, type, argc, captured, nameCut;
	char name[kName];
	Arg args[kArgs];
};
struct Storage
{
	Frame frames[kFrames];
	Command commands[kCommands];
};
struct State
{
	Storage *storage;
	bool armed, frozen;
	int nextFrame, frameCount, activeFrame, activeObject, nextCommand, commandCount, interval, seed;
	unsigned int mapCRC;
	unsigned long long generations, commands;
	State() : storage(NULL), armed(false), frozen(false), nextFrame(0), frameCount(0), activeFrame(-1),
		activeObject(-1), nextCommand(0), commandCount(0), interval(0), seed(0), mapCRC(0), generations(0), commands(0) {}
};
inline State &state() { static State s; return s; }
inline bool enabled() { return state().storage != NULL; }
inline bool recording() { return enabled() && !state().frozen; }
inline bool captureGeneration() { return recording() && state().armed; }
inline const char *mathProfile()
{
#ifdef USE_DETERMINISTIC_MATH
	return "gamemath";
#else
	return "native";
#endif
}
inline bool requested()
{
	const char *env = getenv("GX_LAN_SNAPSHOT");
	if (env && *env) return strcmp(env, "1") == 0;
	FILE *marker = fopen("gx_lan_snapshot.txt", "r");
	if (!marker) return false;
	fclose(marker);
	return true;
}
inline int copyName(char *out, const char *name)
{
	if (!name) name = "unknown";
	int i = 0;
	while (i < kName - 1 && name[i]) { out[i] = name[i]; ++i; }
	out[i] = 0;
	return name[i] != 0;
}
// One stdio call per record prevents unrelated audio/worker logs interleaving
// between fields. Fixed storage also bounds formatting at mismatch time.
struct Line
{
	char text[4096];
	int used;
	bool valid;
	Line() : used(0), valid(true) { text[0] = 0; }
	void add(const char *format, ...)
	{
		if (!valid) return;
		va_list args;
		va_start(args, format);
		const int count = vsnprintf(text + used, sizeof(text) - used, format, args);
		va_end(args);
		if (count < 0 || count >= static_cast<int>(sizeof(text)) - used) { valid = false; return; }
		used += count;
	}
	void name(const char *value)
	{
		if (!*value) { add("-"); return; }
		for (const unsigned char *p = reinterpret_cast<const unsigned char *>(value); *p; ++p) add("%02X", *p);
	}
	void write() const
	{
		fprintf(stderr, "%s\n", valid ? text : "[GX-LAN-SNAPSHOT] invalid reason=line_capacity");
	}
};
// Dump once per match, only after mismatch or orderly reset. No file I/O per object/tick.
inline void dump(const char *reason, int validationFrame)
{
	State &s = state();
	if (!recording() || s.frameCount == 0) return;
	s.frozen = true;
	fprintf(stderr, "[GX-LAN-SNAPSHOT] begin schema=1 math=%s reason=%s validation_frame=%d map_crc=%08X game_seed=%d crc_interval=%d frames=%d generations=%llu commands=%d command_total=%llu object_limit=%d arg_limit=%d\n",
		mathProfile(), reason, validationFrame, s.mapCRC, s.seed, s.interval, s.frameCount, s.generations,
		s.commandCount, s.commands, kObjects, kArgs);
	for (int n = 0; n < s.frameCount; ++n) {
		const Frame &f = s.storage->frames[(s.nextFrame - s.frameCount + n + kFrames) % kFrames];
		fprintf(stderr, "[GX-LAN-SNAPSHOT] frame frame=%d slot=%d crc=%08X stages=%08X,%08X,%08X,%08X,%08X rng_crc=%08X rng_present=%d rng=%08X,%08X,%08X,%08X,%08X,%08X total=%d captured=%d commands_before=%llu\n",
			f.frame, f.slot, f.crc, f.stages[0], f.stages[1], f.stages[2], f.stages[3], f.stages[4],
			f.rngCRC, f.rngPresent ? 1 : 0, f.rng[0], f.rng[1], f.rng[2], f.rng[3], f.rng[4], f.rng[5],
			f.total, f.captured, f.commandsBefore);
		for (int i = 0; i < f.captured; ++i) {
			const Object &o = f.objects[i];
			Line line;
			line.add("[GX-LAN-SNAPSHOT] object frame=%d order=%d id=%08X name=", f.frame, i, o.id);
			line.name(o.name);
			line.add(" name_cut=%d start=%08X crc=%08X mask=%03X fields=", o.nameCut, o.start, o.crc, o.mask);
			for (int j = 0; j < kFields; ++j) line.add("%s%08X", j ? "," : "", o.fields[j]);
			line.add(" transform_count=%d transform=", o.transformCount);
			for (int j = 0; j < 12; ++j) line.add("%s%08X", j ? "," : "", o.transform[j]);
			line.write();
		}
	}
	for (int n = 0; n < s.commandCount; ++n) {
		const Command &c = s.storage->commands[(s.nextCommand - s.commandCount + n + kCommands) % kCommands];
		Line line;
		line.add("[GX-LAN-SNAPSHOT] command seq=%llu frame=%d player=%d type=%d name=", c.sequence, c.frame, c.player, c.type);
		line.name(c.name);
		line.add(" name_cut=%d argc=%d captured=%d args=", c.nameCut, c.argc, c.captured);
		if (!c.captured) line.add("-");
		for (int j = 0; j < c.captured; ++j) {
			const Arg &a = c.args[j];
			line.add("%s%d:%d:%08X:%08X:%08X:%08X", j ? "," : "", a.type, a.count,
				a.words[0], a.words[1], a.words[2], a.words[3]);
		}
		line.write();
	}
	fprintf(stderr, "[GX-LAN-SNAPSHOT] end frames=%d commands=%d\n", s.frameCount, s.commandCount);
	fflush(stderr);
}
inline void endMatch()
{
	dump("match_end", -1);
	delete state().storage;
	state() = State();
}
inline void beginMatch(bool lan, unsigned int mapCRC, int seed, int interval)
{
	endMatch();
	if (!lan || !requested()) return;
	State &s = state();
	s.storage = new(std::nothrow) Storage;
	if (!s.storage) { fprintf(stderr, "[GX-LAN-SNAPSHOT] unavailable reason=allocation\n"); return; }
	s.mapCRC = mapCRC; s.seed = seed; s.interval = interval;
	fprintf(stderr, "[GX-LAN-SNAPSHOT] armed schema=1 bytes=%lu frames=%d objects=%d commands=%d args=%d\n",
		static_cast<unsigned long>(sizeof(Storage)), kFrames, kObjects, kCommands, kArgs);
}
inline void armGeneration() { state().armed = recording(); }
inline void prepare(int frame)
{
	State &s = state();
	if (!captureGeneration()) return;
	s.activeFrame = s.nextFrame; s.activeObject = -1;
	Frame &f = s.storage->frames[s.activeFrame];
	f.frame = frame; f.total = f.captured = 0; f.rngPresent = false;
	memset(f.rng, 0, sizeof(f.rng));
	f.commandsBefore = s.commands;
}
inline bool objectActive() { return captureGeneration() && state().activeFrame >= 0 && state().activeObject >= 0; }
inline void beginObject(unsigned int id, const char *name, unsigned int crc)
{
	State &s = state();
	if (!captureGeneration() || s.activeFrame < 0) return;
	Frame &f = s.storage->frames[s.activeFrame];
	s.activeObject = -1;
	++f.total;
	if (f.captured == kObjects) return;
	s.activeObject = f.captured++;
	Object &o = f.objects[s.activeObject];
	memset(&o, 0, sizeof(o));
	o.id = id; o.start = crc; o.nameCut = copyName(o.name, name);
}
inline void field(const char *name, unsigned int crc)
{
	if (!objectActive()) return;
	Object &o = state().storage->frames[state().activeFrame].objects[state().activeObject];
	for (int i = 0; i < kFields; ++i) if (strcmp(name, fieldName(i)) == 0) {
		o.fields[i] = crc; o.mask |= 1u << i; return;
	}
}
inline void transform(const void *data, int bytes)
{
	if (!objectActive() || !data || bytes != 48) return;
	Object &o = state().storage->frames[state().activeFrame].objects[state().activeObject];
	memcpy(o.transform, data, 48); o.transformCount = 12;
}
inline void endObject(unsigned int crc)
{
	if (objectActive()) state().storage->frames[state().activeFrame].objects[state().activeObject].crc = crc;
	state().activeObject = -1;
}
inline void randomState(const unsigned int (&words)[6])
{
	if (!captureGeneration() || state().activeFrame < 0) return;
	Frame &f = state().storage->frames[state().activeFrame];
	memcpy(f.rng, words, sizeof(f.rng)); f.rngPresent = true;
}
inline void generated(int slot, unsigned int crc, unsigned int rngCRC, const unsigned int (&stages)[5])
{
	State &s = state();
	if (!captureGeneration() || s.activeFrame < 0) return;
	Frame &f = s.storage->frames[s.activeFrame];
	f.slot = slot; f.crc = crc; f.rngCRC = rngCRC; memcpy(f.stages, stages, sizeof(f.stages));
	s.nextFrame = (s.nextFrame + 1) % kFrames;
	if (s.frameCount < kFrames) ++s.frameCount;
	++s.generations; s.activeFrame = s.activeObject = -1; s.armed = false;
}
inline Command *beginCommand(int frame, int player, int type, const char *name, int argc)
{
	State &s = state();
	if (!recording()) return NULL;
	Command &c = s.storage->commands[s.nextCommand];
	memset(&c, 0, sizeof(c));
	c.sequence = s.commands++; c.frame = frame; c.player = player; c.type = type;
	c.argc = argc; c.nameCut = copyName(c.name, name);
	s.nextCommand = (s.nextCommand + 1) % kCommands;
	if (s.commandCount < kCommands) ++s.commandCount;
	return &c;
}
inline void argument(Command *c, int type, int count, unsigned int a = 0, unsigned int b = 0,
	unsigned int d = 0, unsigned int e = 0)
{
	if (!c || c->captured == kArgs) return;
	Arg &v = c->args[c->captured++]; v.type = type; v.count = count;
	v.words[0] = a; v.words[1] = b; v.words[2] = d; v.words[3] = e;
}
inline unsigned int floatBits(float value) { unsigned int bits; memcpy(&bits, &value, 4); return bits; }
}
