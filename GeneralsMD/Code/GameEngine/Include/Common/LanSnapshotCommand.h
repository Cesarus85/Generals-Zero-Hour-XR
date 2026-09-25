// GeneralsX @feature Codex 22/09/2026 Observe commands at dispatch, never local input or packet arrival.
#pragma once
#include "Common/MessageStream.h"
#include "GXLanDesyncSnapshot.h"

inline void ObserveLanSnapshotCommand(int frame, const GameMessage *msg)
{
	using namespace GXLanDesyncSnapshot;
	// CRC payloads are expected to differ on desync; they are not gameplay orders.
	if (!recording() || msg->getType() <= GameMessage::MSG_BEGIN_NETWORK_MESSAGES ||
		msg->getType() >= GameMessage::MSG_END_NETWORK_MESSAGES || msg->getType() == GameMessage::MSG_LOGIC_CRC) return;
	Command *c = beginCommand(frame, msg->getPlayerIndex(), msg->getType(),
		msg->getCommandAsString(), msg->getArgumentCount());
	for (int i = 0; c && i < c->argc && i < kArgs; ++i) {
		const GameMessageArgumentType *a = msg->getArgument(i);
		const GameMessageArgumentDataType type = msg->getArgumentDataType(i);
		// Serialize the active member only: no union padding, addresses or float formatting.
		switch (type) {
		case ARGUMENTDATATYPE_INTEGER: argument(c, type, 1, a->integer); break;
		case ARGUMENTDATATYPE_REAL: argument(c, type, 1, floatBits(a->real)); break;
		case ARGUMENTDATATYPE_BOOLEAN: argument(c, type, 1, a->boolean ? 1u : 0u); break;
		case ARGUMENTDATATYPE_OBJECTID: argument(c, type, 1, a->objectID); break;
		case ARGUMENTDATATYPE_DRAWABLEID: argument(c, type, 1, a->drawableID); break;
		case ARGUMENTDATATYPE_TEAMID: argument(c, type, 1, a->teamID); break;
		case ARGUMENTDATATYPE_LOCATION:
			argument(c, type, 3, floatBits(a->location.x), floatBits(a->location.y), floatBits(a->location.z)); break;
		case ARGUMENTDATATYPE_PIXEL: argument(c, type, 2, a->pixel.x, a->pixel.y); break;
		case ARGUMENTDATATYPE_PIXELREGION:
			argument(c, type, 4, a->pixelRegion.lo.x, a->pixelRegion.lo.y, a->pixelRegion.hi.x, a->pixelRegion.hi.y); break;
		case ARGUMENTDATATYPE_TIMESTAMP: argument(c, type, 1, a->timestamp); break;
		case ARGUMENTDATATYPE_WIDECHAR: argument(c, type, 1, a->wChar); break;
		default: argument(c, type, 0); break; // Explicit unknown coverage; never read inactive union data.
		}
	}
}
