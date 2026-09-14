#pragma once

#include "NGMP_include.h"
// GeneralsX @bugfix Android port 10/07/2026 NetworkMesh.h (P2P transport,
// Valve GameNetworkingSockets) deferred -- see NGMP_include.h. Only used here
// as a pointer type, covered by that header's forward declaration.
#include "GameNetwork/GameSpy/PeerDefs.h"
#include "OnlineServices_Init.h"
#include "Common/MultiplayerSettings.h"

extern NGMPGame* TheNGMPGame;

enum class EChatMessageType
{
	CHAT_MESSAGE_TYPE_NETWORK_ROOM,
	CHAT_MESSAGE_TYPE_LOBBY
};
static Color DetermineColorForChatMessage(EChatMessageType chatMessageType, Bool isPublic, bool bAction, bool bAdmin, int lobbySlot = -1)
{
	Color style = GameMakeColor(255, 255, 255, 255);

	// TODO_NGMP: Support owner chat again
	Bool isOwner = false;

	if (isPublic && bAction)
	{
		style = (isOwner) ? GameSpyColor[GSCOLOR_CHAT_OWNER_EMOTE] : GameSpyColor[GSCOLOR_CHAT_EMOTE];
	}
	else if (isPublic)
	{
		// use lobby colors
		if (chatMessageType == EChatMessageType::CHAT_MESSAGE_TYPE_LOBBY)
		{
			if (lobbySlot == -1)
			{
				return GameMakeColor(255, 255, 255, 255);
			}
			else
			{
				if (TheNGMPGame)
				{
					GameSlot* pSlot = TheNGMPGame->getSlot(lobbySlot);

					if (pSlot != nullptr)
					{
						int numColors = TheMultiplayerSettings->getNumColors();
						int color = pSlot->getColor();
						if (color > -1 && color < numColors)
						{
							MultiplayerColorDefinition* def = TheMultiplayerSettings->getColor(color);
							style = def->getColor();
						}
					}
				}
			}
		}
		else
		{
			if (bAdmin)
			{
				style = GameMakeColor(0, 162, 232, 255);
			}
			else
			{
				style = (isOwner) ? GameSpyColor[GSCOLOR_CHAT_OWNER] : GameSpyColor[GSCOLOR_CHAT_NORMAL];
			}
		}
	}
	else if (bAction)
	{
		style = (isOwner) ? GameSpyColor[GSCOLOR_CHAT_PRIVATE_OWNER_EMOTE] : GameSpyColor[GSCOLOR_CHAT_PRIVATE_EMOTE];
	}
	else
	{
		style = (isOwner) ? GameSpyColor[GSCOLOR_CHAT_PRIVATE_OWNER] : GameSpyColor[GSCOLOR_CHAT_PRIVATE];
	}

	// filters language
//  if( TheGlobalData->m_languageFilterPref )
//  {
	//TheLanguageFilter->filterLine(msg);
	//	}

	return style;
}

struct NGMP_RoomInfo
{
	int numMembers;
	int maxMembers;
};

class NetworkRoomMember : public NetworkMemberBase
{
public:
	bool IsValid() const { return user_id != -1; }
};

class NGMP_OnlineServices_RoomsInterface
{
public:
	NGMP_OnlineServices_RoomsInterface();

	void GetRoomList(std::function<void(void)> cb);

	std::function<void()> m_PendingRoomJoinCompleteCallback = nullptr;
	void JoinRoom(int roomIndex, std::function<void()> onStartCallback, std::function<void()> onCompleteCallback);

	void LeaveRoom()
	{
		std::shared_ptr<WebSocket> pWS = NGMP_OnlineServicesManager::GetWebSocket();
		if (pWS != nullptr)
		{
			pWS->SendData_LeaveNetworkRoom();
		}
	}

	std::function<void(UnicodeString strMessage, Color color)> m_OnChatCallback = nullptr;
	void RegisterForChatCallback(std::function<void(UnicodeString strMessage, Color color)> cb)
	{
		m_OnChatCallback = cb;
	}

	void DeregisterForChatCallback()
	{
		m_OnChatCallback = nullptr;
	}

	std::function<void()> m_RosterNeedsRefreshCallback = nullptr;
	void RegisterForRosterNeedsRefreshCallback(std::function<void()> cb)
	{
		m_RosterNeedsRefreshCallback = cb;
	}

	void DeregisterForRosterNeedsRefreshCallback()
	{
		m_RosterNeedsRefreshCallback = nullptr;
	}

	NetworkRoomMember* GetRoomMemberFromIndex(int index)
	{
		if (m_mapMembers.size() > index)
		{
			auto it = m_mapMembers.begin();
			std::advance(it, index);
			return &it->second;
		}

		return nullptr;
	}

	NetworkRoomMember* GetRoomMemberFromID(int64_t puid)
	{
		if (m_mapMembers.contains(puid))
		{
			return &m_mapMembers[puid];
		}

		return nullptr;
	}

	std::unordered_map<uint64_t, NetworkRoomMember>& GetMembersListForCurrentRoom();

	// Chat
	void SendChatMessageToCurrentRoom(UnicodeString& strChatMsg, bool bIsAction);

	void ResetCachedRoomData()
	{
		m_mapMembers.clear();
	
		if (m_RosterNeedsRefreshCallback != nullptr)
		{
			m_RosterNeedsRefreshCallback();
		}
	}

	void Tick()
	{

	}

	std::vector<NetworkRoom> GetGroupRooms()
	{
		return m_vecRooms;
	}

	void OnRosterUpdated(std::unordered_map<uint64_t, NetworkRoomMember> mapMembers);

	int GetCurrentRoomID() const { return m_CurrentRoomID; }


private:
	int m_CurrentRoomID = -1;

	std::vector<NetworkRoom> m_vecRooms;

	std::unordered_map<uint64_t, NetworkRoomMember> m_mapMembers = std::unordered_map<uint64_t, NetworkRoomMember>();
};
