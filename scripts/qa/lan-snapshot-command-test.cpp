// GeneralsX @test Codex 22/09/2026 Real command observer with an asset-free message test double.
#include "Common/LanSnapshotCommand.h"
#include <cassert>

int main()
{
	using namespace GXLanDesyncSnapshot;
	setenv("GX_LAN_SNAPSHOT", "1", 1);
	beginMatch(true, 1, 2, 100);
	GameMessage msg;
	msg.type = 1001; msg.player = 3; msg.argc = 12;
	for (int i = 0; i < 12; ++i) {
		memset(&msg.args[i], 0xA5, sizeof(msg.args[i])); // Dirty inactive bytes must never affect the trace.
		msg.types[i] = static_cast<GameMessageArgumentDataType>(i);
	}
	msg.args[0].integer = -7; msg.args[1].real = -0.0f; msg.args[2].boolean = true;
	msg.args[3].objectID = 0xD3; msg.args[4].drawableID = 19; msg.args[5].teamID = 20;
	msg.args[6].location = { 1.5f, -2.0f, 0.0f }; msg.args[7].pixel = { -9, 10 };
	msg.args[8].pixelRegion = { {1, 2}, {3, 4} }; msg.args[9].timestamp = 31;
	msg.args[10].wChar = 0x20AC;
	const GameMessage before = msg;
	ObserveLanSnapshotCommand(900, &msg);
	assert(memcmp(&before, &msg, sizeof(msg)) == 0); // Observation cannot mutate a dispatched message.
	const Command &c = state().storage->commands[0];
	assert(c.frame == 900 && c.player == 3 && c.argc == 12 && c.captured == 12);
	assert(c.args[0].words[0] == static_cast<unsigned int>(-7) && c.args[0].words[1] == 0);
	assert(c.args[1].words[0] == 0x80000000u && c.args[2].words[0] == 1);
	assert(c.args[3].words[0] == 0xD3 && c.args[4].words[0] == 19 && c.args[5].words[0] == 20);
	assert(c.args[6].count == 3 && c.args[6].words[0] == 0x3FC00000u && c.args[6].words[1] == 0xC0000000u);
	assert(c.args[7].words[0] == static_cast<unsigned int>(-9));
	assert(c.args[8].count == 4 && c.args[8].words[3] == 4);
	assert(c.args[9].words[0] == 31 && c.args[10].words[0] == 0x20AC);
	assert(c.args[11].count == 0 && c.args[11].words[0] == 0); // Unknown is explicit, not garbage.
	msg = before;
	for (int i = 0; i < 12; ++i) { // Different unused union bytes yield identical serialized words.
		const int bytes[] = {4, 4, 1, 4, 4, 4, 12, 8, 16, 4, 4, 0};
		memset(reinterpret_cast<char *>(&msg.args[i]) + bytes[i], 0xCC, sizeof(msg.args[i]) - bytes[i]);
	}
	ObserveLanSnapshotCommand(900, &msg);
	assert(memcmp(c.args, state().storage->commands[1].args, sizeof(c.args)) == 0);
	msg.type = GameMessage::MSG_LOGIC_CRC; ObserveLanSnapshotCommand(901, &msg);
	msg.type = 1; ObserveLanSnapshotCommand(901, &msg);
	assert(state().commands == 2);
	endMatch();
	ObserveLanSnapshotCommand(902, &before); assert(state().commands == 0);
	return 0;
}
