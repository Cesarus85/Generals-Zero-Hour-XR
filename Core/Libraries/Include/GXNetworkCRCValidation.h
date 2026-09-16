// GeneralsX @bugfix Codex 16/09/2026 Compare live CRCs by network slot, not engine player index.
#pragma once

namespace GXNetworkCRCValidation
{
enum Reason { no_mismatch, missing_crc, different_crc };

// Cache keys remain engine player indices. The caller supplies the same
// player-name-to-network-slot lookup used when accepting each CRC message.
// GeneralsX @bugfix Codex 16/09/2026 Require exactly one CRC for every connected slot.
template <typename Cache, typename IsConnected, typename SlotForPlayer>
Reason evaluate(const Cache &cachedCRCs, int slotCount, IsConnected isConnected, SlotForPlayer slotForPlayer)
{
	bool haveReference = false;
	bool different = false;
	unsigned int reference = 0;
	for (int slot = 0; slot < slotCount; ++slot)
	{
		if (!isConnected(slot)) continue;
		int matches = 0;
		unsigned int crc = 0;
		for (typename Cache::const_iterator it = cachedCRCs.begin(); it != cachedCRCs.end(); ++it)
		{
			if (slotForPlayer(it->first) != slot) continue;
			++matches;
			crc = it->second;
		}
		// A disconnected entry cannot stand in for a missing connected peer.
		// Ambiguous duplicate mappings must not silently count as one peer.
		if (matches != 1) return missing_crc;
		if (!haveReference) { reference = crc; haveReference = true; }
		else if (reference != crc) different = true;
	}
	return different ? different_crc : no_mismatch;
}
}
