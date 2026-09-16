// GeneralsX @bugfix Codex 16/09/2026 Exercise the production live-CRC validator with network-slot spies.
#include "GXNetworkCRCValidation.h"

#include <cassert>
#include <map>
#include <string>

struct NetworkFixture
{
	std::map<int, unsigned int> cache;
	std::map<int, std::string> playerNames;
	std::map<int, std::string> slotNames;
	bool connected[4] = {};

	GXNetworkCRCValidation::Reason validate() const
	{
		return GXNetworkCRCValidation::evaluate(cache, 4,
			[this](int slot) { return connected[slot]; },
			[this](int playerIndex) {
				const std::map<int, std::string>::const_iterator player = playerNames.find(playerIndex);
				if (player == playerNames.end()) return -1;
				for (int slot = 0; slot < 4; ++slot) {
					const std::map<int, std::string>::const_iterator name = slotNames.find(slot);
					if (name != slotNames.end() && name->second == player->second) return slot;
				}
				return -1;
			});
	}
};

int main()
{
	using namespace GXNetworkCRCValidation;
	NetworkFixture match;
	match.slotNames[0] = "host";
	match.slotNames[1] = "join";
	match.slotNames[3] = "former-peer";
	match.playerNames[2] = "host";
	match.playerNames[3] = "join";
	match.playerNames[4] = "former-peer";
	match.connected[0] = true;
	match.connected[1] = true;
	match.cache[2] = 0; // A valid zero CRC is present, not missing.
	match.cache[3] = 0;
	assert(match.validate() == no_mismatch);

	// Both Quest-host and Quest-join arrangements use non-identity player IDs.
	match.cache[3] = 0xEB80E220u;
	assert(match.validate() == different_crc);
	match.cache[2] = 0xFF3C9DF3u;
	assert(match.validate() == different_crc);
	match.cache[3] = match.cache[2];
	assert(match.validate() == no_mismatch);

	// A stale disconnected entry must not conceal the absence of slot 1.
	match.cache.erase(3);
	match.cache[4] = match.cache[2];
	assert(match.validate() == missing_crc);
	match.cache[3] = match.cache[2];
	assert(match.validate() == no_mismatch);
	match.cache[4] = 0;
	assert(match.validate() == no_mismatch); // Disconnected values cannot disagree.

	// Changing which endpoint is local does not change the slot-based result.
	match.playerNames[2] = "join";
	match.playerNames[3] = "host";
	assert(match.validate() == no_mismatch);
	match.cache[3] = 1;
	assert(match.validate() == different_crc);

	// Ambiguous names do not let two player IDs satisfy one connected slot.
	match.playerNames[3] = "join";
	assert(match.validate() == missing_crc);

	// A disconnected match with no connected peers yields no false alarm. The
	// production caller never invokes this validator for offline or replay CRCs.
	match.connected[0] = false;
	match.connected[1] = false;
	assert(match.validate() == no_mismatch);
	return 0;
}
