#pragma once


enum class ELogVerbosity
{
	LOG_DEBUG = 0,
	LOG_RELEASE = 1
};

#define CHECK_MAIN_THREAD assert(std::this_thread::get_id() == NGMP_OnlineServicesManager::g_MainThreadID);
#define CHECK_WORKER_THREAD assert(std::this_thread::get_id() != NGMP_OnlineServicesManager::g_MainThreadID);

static const ELogVerbosity g_LogVerbosity =
#if _DEBUG
ELogVerbosity::LOG_DEBUG;
#else
ELogVerbosity::LOG_RELEASE;
#endif

void NetworkLog(ELogVerbosity logVerbosity, const char* fmt, ...);

std::string to_utf8(const std::wstring& wstr);
std::wstring from_utf8(const std::string& utf8_str);

int RoundUpLatencyToFrameInterval(int latency, int frameInterval);
int ConvertMSLatencyToFrames(int ms);
int ConvertMSLatencyToGenToolFrames(int ms);
// NGMP_NOTE: Plr Templates look like this:
/*
| NAME							|	GAME INDEX	|	SERVICE INDEX	|	PLAYABLE	|
-------------------------------------------------------------------------------------
Civilian								0				-1					No
Observer								1				-1					No
America									2				0					Yes
China									3				1					Yes
GLA										4				2					Yes
AmericaSuperWeaponGeneral				5				3					Yes
AmericaLaserGeneral						6				4					Yes
AmericaAirForceGeneral					7				5					Yes
ChinaTankGeneral						8				6					Yes
ChinaInfantryGeneral					9				7					Yes
ChinaNukeGeneral						10				8					Yes
ChinaNukeGeneral						11				9					Yes
GLAToxinGeneral							12				10					Yes
GLADemolitionGeneral					13				11					Yes
GLAStealthGeneral						14				12					Yes
Boss									15				-1					No
*/

static std::unordered_map<int, std::string> g_mapServiceIndexToPlayerTemplateString =
{
	{ 0, "USA" }, // NOTE: Gamespy used "America", the game uses "USA"
	{ 1, "China" },
	{ 2, "GLA" },
	{ 3, "AmericaSuperWeaponGeneral" },
	{ 4, "AmericaLaserGeneral" },
	{ 5, "AmericaAirForceGeneral" },
	{ 6, "ChinaTankGeneral" },
	{ 7, "ChinaInfantryGeneral" },
	{ 8, "ChinaNukeGeneral" },
	{ 9, "GLAToxinGeneral" },
	{ 10, "GLADemolitionGeneral" },
	{ 11, "GLAStealthGeneral" }
};

// common game engine includes
#include "Common/UnicodeString.h"
#include "Common/AsciiString.h"
// standard libs
#include <fstream>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>

#include "GameNetwork/GeneralsOnline/NextGenMP_defines.h"
// GeneralsX @feature Android port 12/07/2026 P2P match-transport layer,
// previously deferred: now ported from the same go_int snapshot (32ae5135)
// the rest of this module came from, on top of vcpkg's GameNetworkingSockets
// 1.6.0 (native ICE client) instead of upstream's Windows-only prebuilt
// Vendor/ValveNetworkingSockets binaries.
#include "GameNetwork/GeneralsOnline/NetworkPacket.h"
#include "GameNetwork/GeneralsOnline/NetworkBitstream.h"
#include "GameNetwork/GeneralsOnline/NGMP_types.h"
#include "GameNetwork/GeneralsOnline/NGMPGame.h"

class NetworkMesh;

#if defined(GENERALS_ONLINE_BRANCH_JMARSHALL)
#include "../Console/Console.h"
#endif

#include "GameNetwork/GeneralsOnline/json.hpp"

std::string Base64Encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> Base64Decode(const std::string& encodedData);
