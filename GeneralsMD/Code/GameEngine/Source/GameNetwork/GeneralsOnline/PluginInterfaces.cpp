#include "GameNetwork/GeneralsOnline/PluginInterfaces.h"

// GeneralsX @bugfix Android port 07/11/2026 - only the storage definition for the static member is
// needed here. Upstream's PluginInterfaces.cpp implements the Windows DLL-based anti-cheat plugin
// loader under #if defined(GENERALS_ONLINE_USE_PLUGINS_INTERFACE), which we never define (see
// PluginInterfaces.h), so that ~590 lines of loader code is deliberately not ported.
bool AnticheatPlugInterface::g_bPendingExitLobby = false;
