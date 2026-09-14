#pragma once

#include "NGMP_include.h"
#if defined(_WIN32)
#include <ws2ipdef.h>
#endif
#include <steam/steamnetworkingsockets.h>
// GeneralsX @bugfix Android port 12/07/2026 - EConnectionState used to be
// defined locally right here (this file is from the older go_int 32ae5135
// snapshot); it's now canonically declared in PluginInterfaces.h (ported
// later from go_client/main for the anticheat plugin interface, which
// AnticheatPlugInterface::StartSignalling/DisconnectPlayer/etc. -- called
// from NetworkMesh.cpp -- already take as a parameter type). Two same-named
// enum classes in one translation unit is a hard redefinition error, so this
// now just includes the one true definition instead of shadowing it.
#include "GameNetwork/GeneralsOnline/PluginInterfaces.h"

class NetRoom_ChatMessagePacket;

// trivial signalling client interface
class ISignalingClient
{
public:
	virtual ISteamNetworkingConnectionSignaling* CreateSignalingForConnection(const SteamNetworkingIdentity& identityPeer, SteamNetworkingErrMsg& errMsg) = 0;

	virtual void Poll() = 0;

	/// Disconnect from the server and close down our polling thread.
	virtual void Release() = 0;
};

class NetworkMesh;
class PlayerConnection
{
public:
	PlayerConnection()
	{
		
	}

	PlayerConnection(int64_t userID, HSteamNetConnection hSteamConnection);

	EConnectionState GetState() const { return m_State; }

	int SendGamePacket(void* pBuffer, uint32_t totalDataSize);

	void UpdateLatencyHistogram();

	bool IsIPV4();
	bool IsDirect()
	{
		std::string strConnectionType = GetConnectionType();
		return strConnectionType.find("Relayed") == std::string::npos;
	}

	int Recv(SteamNetworkingMessage_t** pMsg);

	int GetHighestHistoricalLatency()
	{
		int highestLatency = 0;
		for (int latencyHistory : m_vecLatencyHistory)
		{
			if (latencyHistory > highestLatency)
			{
				highestLatency = latencyHistory;
			}
		}

		return highestLatency;
	}

	std::vector<int> m_vecLatencyHistory;
	std::string GetStats();

	std::string GetConnectionType();

	void UpdateState(EConnectionState newState, NetworkMesh* pOwningMesh);
	void SetDisconnected(bool bWasError, NetworkMesh* pOwningMesh, bool bIsRetrying);
	
	int64_t m_userID = -1;

	EConnectionState m_State = EConnectionState::NOT_CONNECTED;
	
	int64_t pingSent = -1;

	int m_SignallingAttempts = 0;
	
	int GetLatency();
	int GetJitter();
	float GetConnectionQuality();
	int ComputeConnectionScore();

	HSteamNetConnection m_hSteamConnection = k_HSteamNetConnection_Invalid;
};

struct LobbyMemberEntry;

struct QueuedGamePacket
{
	CBitStream* m_bs = nullptr;
	int64_t m_userID = -1;
};

class NetworkMesh
{
public:
	NetworkMesh();

	~NetworkMesh()
	{
		if (m_pSignaling != nullptr)
		{
			delete m_pSignaling;
			m_pSignaling = nullptr;
		}
	}

	void Flush();

	void RegisterConnectivity(int64_t userID);
	void UpdateConnectivity(PlayerConnection* connection);

	std::function<void(int64_t, std::wstring, PlayerConnection*)> m_cbOnConnected = nullptr;
	void RegisterForConnectionEvents(std::function<void(int64_t, std::wstring, PlayerConnection*)> cb)
	{
		m_cbOnConnected = cb;
	}

	void DeregisterForConnectionEvents()
	{
		m_cbOnConnected = nullptr;
	}

	int getMaximumLatency()
	{
		int highestLatency = 0;

		for (auto& kvPair : m_mapConnections)
		{
			PlayerConnection& conn = kvPair.second;
			if (conn.GetLatency() > highestLatency)
			{
				highestLatency = conn.GetLatency();
			}
		}

		return highestLatency;
	}

	Real getMaximumHistoricalLatency()
	{
		int highestLatency = 0;

		for (auto& kvPair : m_mapConnections)
		{
			PlayerConnection& conn = kvPair.second;
			if (conn.GetHighestHistoricalLatency() > highestLatency)
			{
				highestLatency = conn.GetHighestHistoricalLatency();
			}
		}

		return Real(highestLatency);
	}


	std::queue<QueuedGamePacket> m_queueQueuedGamePackets;

	bool HasGamePacket();
	QueuedGamePacket RecvGamePacket();
	int SendGamePacket(void* pBuffer, uint32_t totalDataSize, int64_t userID);

	void StartConnectionSignalling(int64_t remoteUserID, uint16_t preferredPort);
	void DisconnectUser(int64_t remoteUserID);
	void Disconnect();

	void Tick();

	HSteamListenSocket GetListenSocketHandle() const { return m_hListenSock; }

	std::map<int64_t, PlayerConnection>& GetAllConnections()
	{
		return m_mapConnections;
	}

	PlayerConnection* GetConnectionForUser(int64_t user_id)
	{
		if (m_mapConnections.contains(user_id))
		{
			return &m_mapConnections[user_id];
		}

		return nullptr;
	}


private:
	std::map<int64_t, PlayerConnection> m_mapConnections;

	ISignalingClient* m_pSignaling = nullptr;

	HSteamListenSocket m_hListenSock = k_HSteamListenSocket_Invalid;

	std::string m_strTurnUsername;
	std::string m_strTurnToken;
	std::string m_strTurnUsernameString;
	std::string m_strTurnTokenString;
};
