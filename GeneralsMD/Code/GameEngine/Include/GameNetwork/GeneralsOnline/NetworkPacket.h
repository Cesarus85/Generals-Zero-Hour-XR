#pragma once

// GeneralsX @feature Android port 12/07/2026 Intentionally (almost) empty.
// The go_int snapshot (32ae5135) declared an unscoped EPacketReliability enum
// and an abstract NetworkPacket base here, but nothing in the module ever
// used either -- and the enum's name collides with the scoped
// EPacketReliability that PluginInterfaces.h (ported from go_client/main)
// declares. Upstream main reached the same conclusion and emptied this file;
// it survives only so NGMP_include.h can keep upstream's include list shape.

class CBitStream;
