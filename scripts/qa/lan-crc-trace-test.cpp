// GeneralsX @feature Codex 16/09/2026 Host checks for bounded LAN CRC diagnostics.
#include "GXLanCRCTrace.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

int main()
{
	using namespace GXLanCRCTrace;
	unsetenv("GX_LAN_CRC");
	beginMatch(true, 0x1234u, 7, 5);
	assert(!state().enabled); // No marker and no environment override.
	FILE *marker = fopen("gx_lan_crc.txt", "w");
	assert(marker);
	fclose(marker);
	beginMatch(false, 0x1234u, 7, 5);
	assert(!state().enabled);
	armGeneration();
	assert(!captureGeneration());

	beginMatch(true, 0x1234u, 7, 5);
	assert(state().enabled && state().generated == 0 && state().validated == 0);
	int connected[] = { 0, 1 };
	// Player indices intentionally differ from network slots.
	PeerCRC matching[] = { { 2, 0, 0xABCDu }, { 3, 1, 0xABCDu } };
	PeerCRC differing[] = { { 2, 0, 0xABCDu }, { 3, 1, 0xABCEu } };
	PeerCRC absent[] = { { 2, 0, 0xABCDu } };
	PeerCRC disconnectedExtra[] = { { 2, 0, 0xABCDu }, { 4, 3, 0xABCDu } };
	assert(classify(connected, 2, matching, 2) == no_mismatch);
	assert(classify(connected, 2, differing, 2) == different_crc);
	assert(classify(connected, 2, absent, 1) == missing_crc);
	assert(classify(connected, 2, disconnectedExtra, 2) == missing_crc);
	assert(std::strcmp(reasonName(missing_crc), "missing_crc") == 0);

	const unsigned int engineCRC = 0xABCDu;
	const Stages stages = { 1u, 2u, 3u, 4u, 5u };
	ObjectCRC objects[2] = {};
	int capturedObjects = 0;
	int totalObjects = 0;
	observeObject(objects, 2, capturedObjects, totalObjects, 0x10u, 0x100u);
	observeObject(objects, 2, capturedObjects, totalObjects, 0x20u, 0x200u);
	observeObject(objects, 2, capturedObjects, totalObjects, 0x30u, 0x300u);
	assert(capturedObjects == 2 && totalObjects == 3);
	assert(objects[0].id == 0x10u && objects[0].crc == 0x100u);
	assert(objects[1].id == 0x20u && objects[1].crc == 0x200u);
	armGeneration();
	beginObjectDetail(0, 14, kDetailObjectID, "TestObject", 0x01u);
	assert(objectDetailActive());
	objectField("private_status", 0x02u);
	const unsigned int transformWords[] = { 0x3F800000u, 0u, 0u, 0x41200000u };
	objectTransform(transformWords, sizeof(transformWords));
	const RailroadStep railroad = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f,
		10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f };
	railroadStep(100, 0x10u, railroad);
	railroadStep(106, 0x10u, railroad);
	endObjectDetail();
	assert(!objectDetailActive());
	int scheduled = 0;
	for (int frame = 0; frame < 100; ++frame) {
		if (frame % 5 != 0) continue; // The existing caller owns this interval.
		++scheduled;
		armGeneration();
		if (captureGeneration()) generated(frame, 0, engineCRC, 42u, stages,
			objects, capturedObjects, totalObjects);
		checkpoint(frame + 1, 0, connected, 2, matching, 2, no_mismatch);
	}
	assert(scheduled == 20 && state().generated == kFirstCheckpoints && state().validated == kFirstCheckpoints);
	assert(engineCRC == 0xABCDu && matching[0].crc == 0xABCDu);
	checkpoint(101, 0, connected, 2, absent, 1, missing_crc);
	assert(state().failureWritten && state().validated == kFirstCheckpoints);
	checkpoint(102, 0, connected, 2, differing, 2, different_crc);
	assert(state().failureWritten && state().validated == kFirstCheckpoints);

	endMatch();
	assert(!state().enabled && !state().failureWritten);
	assert(remove("gx_lan_crc.txt") == 0);
	setenv("GX_LAN_CRC", "1", 1);
	beginMatch(true, 0x5678u, 11, 10);
	assert(state().enabled && state().generated == 0 && state().validated == 0);
	checkpoint(1, 0, connected, 2, differing, 2, different_crc);
	assert(state().failureWritten && state().validated == 1);
	endMatch();
	unsetenv("GX_LAN_CRC");
	return 0;
}
