// GeneralsX @test Codex 22/09/2026 Exercise production snapshot lifecycle, ring bounds and paired output.
#include "GXLanCRCTrace.h"
#include <cassert>

using namespace GXLanDesyncSnapshot;

static void frame(int tick, int count, bool different)
{
	GXLanCRCTrace::armGeneration();
	assert(GXLanCRCTrace::captureGeneration());
	prepare(tick);
	unsigned int crc = 1;
	for (int i = 0; i < count; ++i) {
		GXLanCRCTrace::beginObjectDetail(tick, i, 0xD2 + i, i == 1 ? "Test Building" : "TestUnit", crc);
		const unsigned int matrix[12] = { 0x3F800000u, 0, 0, 0, 0, 0x3F800000u, 0, 0, 0, 0, 0x3F800000u, 0 };
		GXLanCRCTrace::objectTransform(matrix, sizeof(matrix));
		for (int j = 0; j < kFields; ++j) {
			crc += 7;
			if (different && i == 1 && j == 5) ++crc;
			GXLanCRCTrace::objectField(fieldName(j), crc);
		}
		GXLanCRCTrace::endObjectDetail(crc);
	}
	const unsigned int rng[6] = { 1, 2, 3, 4, 5, 6 };
	randomState(rng);
	const GXLanCRCTrace::Stages stages = { crc, crc + 1, crc + 2, crc + 3, crc + 4 };
	GXLanCRCTrace::generated(tick, 0, crc + 4, 42, stages, NULL, 0, count);
}

int main(int argc, char **argv)
{
	Line line;
	line.add("%04095d", 1); assert(line.valid && line.used == 4095);
	line.add("x"); assert(!line.valid); // Fail closed; never emit a silently truncated record.
	unsetenv("GX_LAN_CRC"); unsetenv("GX_LAN_SNAPSHOT");
	GXLanCRCTrace::beginMatch(true, 123, 7, 100);
	assert(!enabled());
	GXLanCRCTrace::armGeneration(); assert(!captureGeneration());
	assert(!beginCommand(0, 0, 1001, "ignored", 0));
	FILE *marker = fopen("gx_lan_snapshot.txt", "w"); assert(marker); fclose(marker);
	GXLanCRCTrace::beginMatch(false, 123, 7, 100); assert(!enabled());
	setenv("GX_LAN_SNAPSHOT", "0", 1);
	GXLanCRCTrace::beginMatch(true, 123, 7, 100); assert(!enabled()); // Explicit OFF overrides marker.
	unsetenv("GX_LAN_SNAPSHOT");
	GXLanCRCTrace::beginMatch(true, 123, 7, 100); assert(enabled());
	assert(remove("gx_lan_snapshot.txt") == 0);
	GXLanCRCTrace::endMatch(); assert(!enabled());
	setenv("GX_LAN_SNAPSHOT", "1", 1);
	setenv("GX_LAN_CRC", "1", 1);
	GXLanCRCTrace::beginMatch(true, 123, 7, 100);
	assert(enabled() && !GXLanCRCTrace::state().enabled); // Universal mode wins over legacy probes.
	assert(sizeof(Storage) < 8 * 1024 * 1024);
	if (argc > 1) {
		const bool different = strcmp(argv[1], "different") == 0;
		const bool late = strcmp(argv[1], "late") == 0;
		const char *actions[] = { "construct", "produce", "move", "attack", "ability" };
		const int last = late ? 2000 : 500;
		for (int tick = 0; tick <= last; tick += 100) {
			Command *c = beginCommand(tick, 2 + (tick / 100) % 2, 1001 + (tick / 100) % 5, actions[(tick / 100) % 5], 1);
			argument(c, 0, 1, tick);
			frame(tick, 3, (different && tick >= 400) || (late && tick >= 1900));
		}
		if (different || late) {
			GXLanCRCTrace::checkpoint(last + 5, 0, NULL, 0, NULL, 0, GXLanCRCTrace::different_crc);
			assert(state().frozen && !captureGeneration());
			GXLanCRCTrace::checkpoint(last + 6, 0, NULL, 0, NULL, 0, GXLanCRCTrace::different_crc);
		}
		GXLanCRCTrace::endMatch();
		return 0;
	}
	for (int i = 0; i < kCommands + 17; ++i) {
		Command *c = beginCommand(i, 2, 1001, "test", kArgs + 2);
		for (int j = 0; j < kArgs + 2; ++j) argument(c, 6, 3, floatBits(1.5f), 2, 3);
		assert(c->captured == kArgs);
	}
	assert(state().commandCount == kCommands && state().commands == kCommands + 17);
	for (int tick = 0; tick <= 2000; tick += 100) frame(tick, tick == 2000 ? kObjects + 1 : 3, false);
	assert(state().frameCount == kFrames && state().generations == 21);
	const Frame &last = state().storage->frames[(state().nextFrame + kFrames - 1) % kFrames];
	assert(last.total == kObjects + 1 && last.captured == kObjects);
	assert(last.objects[1].id == 0xD3 && last.objects[2].mask == 0x7FF);
	dump("different_crc", 2005);
	assert(state().frozen && !beginCommand(2006, 2, 1001, "ignored", 0));
	GXLanCRCTrace::endMatch();
	assert(!enabled() && state().commands == 0 && state().generations == 0);
	GXLanCRCTrace::beginMatch(true, 456, 8, 100);
	assert(enabled() && state().frameCount == 0);
	GXLanCRCTrace::endMatch();
	return 0;
}
