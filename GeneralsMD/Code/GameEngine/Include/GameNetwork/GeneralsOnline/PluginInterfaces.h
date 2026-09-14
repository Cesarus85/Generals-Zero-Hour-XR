#pragma once

// GeneralsX @bugfix Android port 07/11/2026 - ported verbatim from upstream GeneralsOnline. The
// GENERALS_ONLINE_USE_PLUGINS_INTERFACE branch below (Windows DLL-based anti-cheat plugin loader)
// is never compiled since we don't define that macro; only the portable no-op stub class is used.
#include <cstdint>
#include <string>

enum class EConnectionState : uint8_t
{
    NOT_CONNECTED,
    CONNECTING_DIRECT,
    FINDING_ROUTE,
    CONNECTED_DIRECT,
    CONNECTION_FAILED,
    CONNECTION_DISCONNECTED
};

enum class ENetworkChannels : uint8_t
{
    Game = 0,
    Anticheat,
    Signalling
};

enum class EPacketReliability : int32_t
{
    PACKET_RELIABILITY_UNRELIABLE_UNORDERED = 0,
    PACKET_RELIABILITY_RELIABLE_UNORDERED = 1,
    PACKET_RELIABILITY_RELIABLE_ORDERED = 2
};



enum class EAnticheatActionType : int32_t
{
    NONE = 0,
    KICK = 1
};

enum class EAnticheatActionReason : int32_t
{
    Unknown = 0,
    InternalError = 1,
    InvalidMessage = 2,
    AuthFailure = 3,
    ACNotRunning = 4,
    HeartbeatTimedOut = 5,
    ClientViolation = 6,
    BackendViolation = 7,
    TempCooldown = 8,
    TempBanned = 9,
    PermaBanned = 10
};

#if defined(GENERALS_ONLINE_USE_PLUGINS_INTERFACE)
#define AC_ENABLED 1

class AnticheatPlugInterface
{
public:
    static bool g_bPendingExitLobby;

    static void AC_NetworkMessageArrived(uint32_t goUserID, void* pData, uint32_t dataLen);

    static bool DidPluginFailToLoad() { return m_bPluginLoadFailed; }

    static bool IsPluginLoaded()
    {
        return g_hACPluginModule != nullptr && !m_bPluginLoadFailed;
    }

    static bool IsExternalProcessRunning();

    static int GetAnticheatIdentifier();

    static int GetConnectionLatencyForUser(std::string mwUserID, uint32_t goUserID);

    static void LoadPlugin(const char* szPluginName);
    static void Authenticate();
    static void UnloadPlugin();
    static void Tick();

    static void RefreshToken();

    static bool RegisterPlayer(std::string mwUserID, uint32_t goUserID);
    static bool DeregisterPlayer(std::string mwUserID, uint32_t goUserID);

    static void BeginSession();
    static void EndSession();

    // transport related
    static bool DoesACPluginProvideSecureGameTransport();
    static void SendPacket(const char* szMiddlewareUserID, uint64_t targetGoUserID, void* pData, int numBytes, ENetworkChannels channel, EPacketReliability reliability);
    static void StartSignalling(const char* szMiddlewareUserID, uint64_t goUserID);
    static int GetNextRecvPacketSize(uint8_t channelToReceiveOn);
    static bool RecvPacket(uint8_t** pOutData, uint8_t channelToReceiveOn);

    static void DisconnectPlayer(const char* szMiddlewareUserID, uint64_t goUserID);
    static void DisconnectAll();

#if defined(AC_ENABLED)
    typedef void (*FuncDefStartSignalling)(const char* szMiddlewareUserID, uint64_t goUserID);
    typedef void (*FuncDefSendPacket)(const char* szMiddlewareUserID, uint64_t targetGoUserID, void* pData, int numBytes, ENetworkChannels channel, EPacketReliability reliability);
    typedef bool (*FuncDefDoesACPluginProvideSecureGameTransport)(void);
    typedef int (*FuncDefGetNextRecvPacketSize)(uint8_t channelToReceiveOn);
    typedef bool (*FuncDefRecvPacket)(uint8_t** pOutData, uint8_t channelToReceiveOn);
    typedef void (*FuncDefFreePacket)(void* pPacketData);
    typedef void (*FuncDefDisconnectPlayer)(const char* szMiddlewareUserID, uint64_t goUserID);
    typedef void (*FuncDefDisconnectAll)();

    // Callbacks from plugin
    typedef void (*LoginCallback)(bool bSuccess);
    typedef void (*LoggingFunc)(const char*);
    typedef void (*FuncDefACPlayerActionRequiredCallbackFunc)(uint32_t, const char*, EAnticheatActionType, EAnticheatActionReason);
    typedef void (*FuncDefSetACActionRequiredCallback)(FuncDefACPlayerActionRequiredCallbackFunc);
    typedef void (*SendMessageViaTransportCallbackFunc)(uint32_t, const void*, uint32_t);

    typedef void (*FuncDefCIntegrityViolationOccurredCallbackFunc)(const char*, int);
    typedef void (*FuncDefSetACIntegrityViolationOccurredCallback)(FuncDefCIntegrityViolationOccurredCallbackFunc);

    // Func defs
    typedef void (*FuncDefSetLoggingFunction)(LoggingFunc);

    typedef void (*OnConnectionStateChangedCallbackFunc)(const char*, uint64_t, EConnectionState);
    typedef int (*FuncDefInitialize)(OnConnectionStateChangedCallbackFunc connectionStateChangedCB);
    typedef bool (*FuncDefIsExternalProcessRunning)(void);

    typedef int (*FuncDefGetAnticheatIdentifier)(void);
    typedef int (*FuncDefGetConnectionLatencyForUser)(const char* szMiddlewareUserID, uint32_t goUserID);
    
    typedef void (*FuncDefSetSendMessageViaTransportCallback)(SendMessageViaTransportCallbackFunc);
    typedef void (*FuncDefACMessageArrivedViaTransport)(uint32_t, void*, uint32_t);
    typedef void (*FuncDefLogin)(const char* szGameToken, LoginCallback cb);
    typedef void (*FuncDefRefreshToken)(const char* szGameToken, LoginCallback cb);
    typedef bool (*FuncDefGetMiddlewareAuthToken)(char* buffer, size_t bufferSize);
    typedef bool (*FuncDefIsLoggedIn)(void);
    typedef void (*FuncDefBeginSession)(void);
    typedef void (*FuncDefEndSession)(void);
    typedef bool (*FuncDefRegisterPlayer)(const char* szMiddlewareUserID, uint32_t goUserID);
    typedef bool (*FuncDefDeregisterPlayer)(const char* szMiddlewareUserID, uint32_t goUserID);
    typedef void (*FuncDefTick)(void);

    typedef void (*FuncDefShutdown)(void);

    struct AnticheatPluginFunctionPtrs
    {
        FuncDefSetLoggingFunction fnSetLoggingFunction = nullptr;
        FuncDefInitialize fnInitialize = nullptr;
        FuncDefIsExternalProcessRunning fnIsExternalProcessRunning = nullptr;
        FuncDefGetAnticheatIdentifier fnGetAnticheatIdentifier = nullptr;
        FuncDefSetACActionRequiredCallback fnSetACActionRequiredCallback = nullptr;
        FuncDefSetACIntegrityViolationOccurredCallback fnSetACIntegrityViolationOccurredCallback = nullptr;
        FuncDefSetSendMessageViaTransportCallback fnSetSendMessageViaTransportCallback = nullptr;
        FuncDefACMessageArrivedViaTransport fnACMessageArrivedViaTransport = nullptr;
        FuncDefLogin fnLogin = nullptr;
        FuncDefRefreshToken fnRefreshToken = nullptr;
        FuncDefGetMiddlewareAuthToken fnGetMiddlewareAuthToken = nullptr;
        FuncDefIsLoggedIn fnIsLoggedIn = nullptr;
        FuncDefBeginSession fnBeginSession = nullptr;
        FuncDefEndSession fnEndSession = nullptr;
        FuncDefRegisterPlayer fnRegisterPlayer = nullptr;
        FuncDefDeregisterPlayer fnDeregisterPlayer = nullptr;
        FuncDefTick fnTick = nullptr;
        FuncDefShutdown fnShutdown = nullptr;

        // transport related
        FuncDefDoesACPluginProvideSecureGameTransport fnDoesACPluginProvideSecureGameTransport = nullptr;
        FuncDefStartSignalling fnStartSignalling = nullptr;
        FuncDefSendPacket fnSendPacket = nullptr;
        FuncDefGetNextRecvPacketSize fnGetNextRecvPacketSize = nullptr;
        FuncDefRecvPacket fnRecvPacket = nullptr;

        FuncDefGetConnectionLatencyForUser fnGetConnectionLatencyForUser = nullptr;

        FuncDefDisconnectPlayer fnDisconnectPlayer = nullptr;
        FuncDefDisconnectAll fnDisconnectAll = nullptr;
    };
    static AnticheatPluginFunctionPtrs Functions;
#else
    typedef bool (*FuncDefIsExternalProcessRunning)(void);
    typedef int (*FuncDefGetAnticheatIdentifier)(void);
    typedef int (*FuncDefInitialize)();

    struct AnticheatPluginFunctionPtrs
    {
        FuncDefIsExternalProcessRunning fnIsExternalProcessRunning = nullptr;
        FuncDefGetAnticheatIdentifier fnGetAnticheatIdentifier = nullptr;
        FuncDefInitialize fnInitialize = nullptr;
    };
    static AnticheatPluginFunctionPtrs Functions;
#endif

    // Module
    static HMODULE g_hACPluginModule;
    static bool m_bPluginLoadFailed;

    static int64_t m_tokenCreationTime;
};

extern HWND ApplicationHWnd;
#else
class AnticheatPlugInterface
{
public:
    static bool g_bPendingExitLobby;

    static void AC_NetworkMessageArrived(uint32_t goUserID, void* pData, uint32_t dataLen)
    {

    }

    static bool DidPluginFailToLoad() { return false; }

    static bool IsPluginLoaded()
    {
        return true;
    }

    static bool IsExternalProcessRunning()
    {
        return true;
    }

    static int GetAnticheatIdentifier()
    {
        return 0;
    }

    static int GetConnectionLatencyForUser(std::string mwUserID, uint32_t goUserID)
    {
        return 0;
    }

    static void LoadPlugin(const char* szPluginName)
    {

    }

    static void Authenticate()
    {

    }

    static void UnloadPlugin()
    {

    }

    static void Tick()
    {

    }

    static void RefreshToken()
    {

    }

    static bool RegisterPlayer(std::string mwUserID, uint32_t goUserID)
    {
        return true;
    }

    static bool DeregisterPlayer(std::string mwUserID, uint32_t goUserID)
    {
        return true;
    }

    static void BeginSession()
    {

    }

    static void EndSession()
    {

    }

    static bool DoesACPluginProvideSecureGameTransport()
    {
        return false;
    }

    static void SendPacket(const char* szMiddlewareUserID, uint64_t targetGoUserID, void* pData, int numBytes, ENetworkChannels channel, EPacketReliability reliability)
    {

    }

    static void StartSignalling(const char* szMiddlewareUserID, uint64_t goUserID)
    {

    }

    static int GetNextRecvPacketSize(uint8_t channelToReceiveOn)
    {
        return 0;
    }

    static bool RecvPacket(uint8_t** pOutData, uint8_t channelToReceiveOn)
    {
        *pOutData = nullptr;
        return false;
    }

    static void DisconnectPlayer(const char* szMiddlewareUserID, uint64_t goUserID)
    {

    }

    static void DisconnectAll()
    {

    }
};

#endif
