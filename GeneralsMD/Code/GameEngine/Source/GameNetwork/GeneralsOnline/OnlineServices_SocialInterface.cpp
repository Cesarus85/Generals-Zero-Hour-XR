#include "GameNetwork/GeneralsOnline/json.hpp"
#include "GameNetwork/GeneralsOnline/NGMP_interfaces.h"
#include "GameNetwork/GameSpy/PersistentStorageThread.h"
#include "GameNetwork/RankPointValue.h"
#include "GameNetwork/GeneralsOnline/OnlineServices_Init.h"
#include "GameNetwork/GeneralsOnline/HTTP/HTTPManager.h"
#include "GameClient/GameText.h"

NGMP_OnlineServices_SocialInterface::NGMP_OnlineServices_SocialInterface()
{

}

NGMP_OnlineServices_SocialInterface::~NGMP_OnlineServices_SocialInterface()
{

}

void NGMP_OnlineServices_SocialInterface::GetFriendsList(bool bUseCache, std::function<void()> cb)
{
	if (bUseCache)
	{
		if (cb != nullptr)
		{
			cb();
		}

		return;
	}

	m_cbOnGetFriendsList = cb;

	std::string strURI = NGMP_OnlineServicesManager::GetAPIEndpoint("Social/Friends");
	std::map<std::string, std::string> mapHeaders;

	NGMP_OnlineServicesManager::GetInstance()->GetHTTPManager()->SendGETRequest(strURI.c_str(), EIPProtocolVersion::DONT_CARE, mapHeaders, [=](bool bSuccess, int statusCode, std::string strBody, HTTPRequest* pReq)
		{
			FriendsResult friendsResult;

			try
			{
				m_mapFriends.clear();
				m_mapPendingRequests.clear();

				nlohmann::json jsonObject = nlohmann::json::parse(strBody);

				// friends
				for (const auto& friendEntryIter : jsonObject["friends"])
				{
					FriendsEntry newFriend;

					friendEntryIter["user_id"].get_to(newFriend.user_id);
					friendEntryIter["display_name"].get_to(newFriend.display_name);
					friendEntryIter["online"].get_to(newFriend.online);
					friendEntryIter["presence"].get_to(newFriend.presence);


					friendsResult.vecFriends.push_back(newFriend);

					// cache
					m_mapFriends[newFriend.user_id] = newFriend;
				}

				// pending requests
				for (const auto& friendEntryIter : jsonObject["pending_requests"])
				{
					FriendsEntry newEntry;

					friendEntryIter["user_id"].get_to(newEntry.user_id);
					friendEntryIter["display_name"].get_to(newEntry.display_name);


					friendsResult.vecPendingRequests.push_back(newEntry);

					// cache
					m_mapPendingRequests[newEntry.user_id] = newEntry;
				}
			}
			catch (...)
			{

			}

			if (m_cbOnGetFriendsList != nullptr)
			{
				// TODO_SOCIAL: Clean this up on exit etc
				m_cbOnGetFriendsList();
				m_cbOnGetFriendsList = nullptr;
			}
		});
}

void NGMP_OnlineServices_SocialInterface::GetBlockList(std::function<void(BlockedResult blockResult)> cb)
{
	m_cbOnGetBlockList = cb;

	std::string strURI = NGMP_OnlineServicesManager::GetAPIEndpoint("Social/Blocked");
	std::map<std::string, std::string> mapHeaders;

	NGMP_OnlineServicesManager::GetInstance()->GetHTTPManager()->SendGETRequest(strURI.c_str(), EIPProtocolVersion::DONT_CARE, mapHeaders, [=](bool bSuccess, int statusCode, std::string strBody, HTTPRequest* pReq)
		{
			BlockedResult blockedResult;

			m_mapBlocked.clear();

			try
			{
				nlohmann::json jsonObject = nlohmann::json::parse(strBody);

				for (const auto& blockedEntryIter : jsonObject["blocked"])
				{
					FriendsEntry newEntry;

					blockedEntryIter["user_id"].get_to(newEntry.user_id);
					blockedEntryIter["display_name"].get_to(newEntry.display_name);


					blockedResult.vecBlocked.push_back(newEntry);

					// cache
					m_mapBlocked[newEntry.user_id] = newEntry;
				}
			}
			catch (...)
			{

			}

			if (m_cbOnGetBlockList != nullptr)
			{
				// TODO_SOCIAL: Clean this up on exit etc
				m_cbOnGetBlockList(blockedResult);
				m_cbOnGetBlockList = nullptr;
			}
		});
}

void NGMP_OnlineServices_SocialInterface::AddFriend(int64_t target_user_id)
{
	std::string strURI = std::format("{}/{}", NGMP_OnlineServicesManager::GetAPIEndpoint("Social/Friends/Requests"), target_user_id);
	std::map<std::string, std::string> mapHeaders;

	NGMP_OnlineServicesManager::GetInstance()->GetHTTPManager()->SendPUTRequest(strURI.c_str(), EIPProtocolVersion::DONT_CARE, mapHeaders, "", [=](bool bSuccess, int statusCode, std::string strBody, HTTPRequest* pReq)
		{

		});
}

void NGMP_OnlineServices_SocialInterface::RemoveFriend(int64_t target_user_id)
{
	std::string strURI = std::format("{}/{}", NGMP_OnlineServicesManager::GetAPIEndpoint("Social/Friends"), target_user_id);
	std::map<std::string, std::string> mapHeaders;

	NGMP_OnlineServicesManager::GetInstance()->GetHTTPManager()->SendDELETERequest(strURI.c_str(), EIPProtocolVersion::DONT_CARE, mapHeaders, "", [=](bool bSuccess, int statusCode, std::string strBody, HTTPRequest* pReq)
		{

		});
}

void NGMP_OnlineServices_SocialInterface::IgnoreUser(int64_t target_user_id)
{
	std::string strURI = std::format("{}/{}", NGMP_OnlineServicesManager::GetAPIEndpoint("Social/Blocked"), target_user_id);
	std::map<std::string, std::string> mapHeaders;

	NGMP_OnlineServicesManager::GetInstance()->GetHTTPManager()->SendPUTRequest(strURI.c_str(), EIPProtocolVersion::DONT_CARE, mapHeaders, "", [=](bool bSuccess, int statusCode, std::string strBody, HTTPRequest* pReq)
		{

		});
}

void NGMP_OnlineServices_SocialInterface::UnignoreUser(int64_t target_user_id)
{
	std::string strURI = std::format("{}/{}", NGMP_OnlineServicesManager::GetAPIEndpoint("Social/Blocked"), target_user_id);
	std::map<std::string, std::string> mapHeaders;

	NGMP_OnlineServicesManager::GetInstance()->GetHTTPManager()->SendDELETERequest(strURI.c_str(), EIPProtocolVersion::DONT_CARE, mapHeaders, "", [=](bool bSuccess, int statusCode, std::string strBody, HTTPRequest* pReq)
		{

		});
}

void NGMP_OnlineServices_SocialInterface::AcceptPendingRequest(int64_t target_user_id)
{
	std::string strURI = std::format("{}/{}", NGMP_OnlineServicesManager::GetAPIEndpoint("Social/Friends/Requests"), target_user_id);
	std::map<std::string, std::string> mapHeaders;

	NGMP_OnlineServicesManager::GetInstance()->GetHTTPManager()->SendPOSTRequest(strURI.c_str(), EIPProtocolVersion::DONT_CARE, mapHeaders, "", [=](bool bSuccess, int statusCode, std::string strBody, HTTPRequest* pReq)
		{

		});

	// update notifications
	--m_numTotalNotifications;
	TriggerCallback_OnNumberGlobalNotificationsChanged();
}

void NGMP_OnlineServices_SocialInterface::RejectPendingRequest(int64_t target_user_id)
{
	std::string strURI = std::format("{}/{}", NGMP_OnlineServicesManager::GetAPIEndpoint("Social/Friends/Requests"), target_user_id);
	std::map<std::string, std::string> mapHeaders;

	NGMP_OnlineServicesManager::GetInstance()->GetHTTPManager()->SendDELETERequest(strURI.c_str(), EIPProtocolVersion::DONT_CARE, mapHeaders, "", [=](bool bSuccess, int statusCode, std::string strBody, HTTPRequest* pReq)
		{

		});

    // update notifications
    --m_numTotalNotifications;
    TriggerCallback_OnNumberGlobalNotificationsChanged();
}

void NGMP_OnlineServices_SocialInterface::OnChatMessage(int64_t source_user_id, int64_t target_user_id, UnicodeString unicodeStr)
{
	// also cache it incase UI isnt visible
	NGMP_OnlineServices_AuthInterface* pAuthInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_AuthInterface>();
	if (pAuthInterface != nullptr)
	{
		int64_t user_id_to_store = -1;

		// was it me chatting?
		if (pAuthInterface->GetUserID() == source_user_id)
		{
			// cache under target user
			user_id_to_store = target_user_id;
		}
		else
		{
			// cache under source user
			user_id_to_store = source_user_id;
		}

		// does it exist yet?
		if (!m_mapCachedMessages.contains(user_id_to_store))
		{
			m_mapCachedMessages[user_id_to_store] = std::vector<UnicodeString>();
		}

		// only if I am not the sender and overlay isnt active
		if (pAuthInterface->GetUserID() != source_user_id && !m_bOverlayActive)
		{
            if (!m_mapUnreadMessagesForUser.contains(user_id_to_store))
            {
                m_mapUnreadMessagesForUser[user_id_to_store] = 1;

                ++m_numTotalNotifications; // only increase this if we dont already have unread messages from the person
                TriggerCallback_OnNumberGlobalNotificationsChanged();
            }
            else
            {
                ++m_mapUnreadMessagesForUser[user_id_to_store];
            }
            // show popup for incoming message
            if (TheNGMPGame != nullptr && TheNGMPGame->isGameInProgress())
            {
                if (NGMP_OnlineServicesManager::Settings.Social_Notifications_FriendComesOnline_Gameplay())
                {
#if defined(GENERALS_ONLINE)
                    showNotificationBox(AsciiString::TheEmptyString, unicodeStr, true /*bPlaySound*/);
#else
                    showNotificationBox(AsciiString::TheEmptyString, unicodeStr);
#endif
                }
            }
            else
            {
                if (NGMP_OnlineServicesManager::Settings.Social_Notifications_FriendComesOnline_Menus())
                {
#if defined(GENERALS_ONLINE)
                    showNotificationBox(AsciiString::TheEmptyString, unicodeStr, true /*bPlaySound*/);
#else
                    showNotificationBox(AsciiString::TheEmptyString, unicodeStr);
#endif
                }
            }
        }
        m_mapCachedMessages[user_id_to_store].push_back(unicodeStr);
        
        if (m_cbOnChatMessage != nullptr)
        {
            m_cbOnChatMessage(source_user_id, target_user_id, unicodeStr);
        }
	}
}

void NGMP_OnlineServices_SocialInterface::OnOnlineStatusChanged(std::string strDisplayName, bool bOnline)
{
	bool bShowNotification = false;
	if (bOnline)
	{
		if (TheNGMPGame != nullptr && TheNGMPGame->isGameInProgress())
		{
			bShowNotification = NGMP_OnlineServicesManager::Settings.Social_Notifications_FriendComesOnline_Gameplay();
		}
		else
		{
			bShowNotification = NGMP_OnlineServicesManager::Settings.Social_Notifications_FriendComesOnline_Menus();
		}
	}
	else
	{
        if (TheNGMPGame != nullptr && TheNGMPGame->isGameInProgress())
        {
            bShowNotification = NGMP_OnlineServicesManager::Settings.Social_Notifications_FriendGoesOffline_Gameplay();
        }
        else
        {
            bShowNotification = NGMP_OnlineServicesManager::Settings.Social_Notifications_FriendGoesOffline_Menus();
        }
	}

	// TODO_SOCIAL: Update communicator if its active
	if (bShowNotification)
	{
		showNotificationBox(AsciiString(strDisplayName.c_str()), bOnline ? TheGameText->fetch("Buddy:OnlineNotification") : UnicodeString(L"%hs went offline"));
	}
}

void NGMP_OnlineServices_SocialInterface::OnFriendRequestAccepted(std::string strDisplayName)
{
	bool bShowNotification = true;
    if (TheNGMPGame != nullptr && TheNGMPGame->isGameInProgress())
    {
        bShowNotification = NGMP_OnlineServicesManager::Settings.Social_Notifications_PlayerAcceptsRequest_Gameplay();
    }
    else
    {
        bShowNotification = NGMP_OnlineServicesManager::Settings.Social_Notifications_PlayerAcceptsRequest_Menus();
    }

	if (bShowNotification)
	{
		showNotificationBox(AsciiString(strDisplayName.c_str()), UnicodeString(L"%hs accepted your friend request."));
	}
}

bool NGMP_OnlineServices_SocialInterface::IsUserIgnored(int64_t target_user_id)
{
	return m_mapBlocked.contains(target_user_id);
}

bool NGMP_OnlineServices_SocialInterface::IsUserFriend(int64_t target_user_id)
{
	return m_mapFriends.contains(target_user_id);
}

bool NGMP_OnlineServices_SocialInterface::IsUserPendingRequest(int64_t target_user_id)
{
    return m_mapPendingRequests.contains(target_user_id);
}

void NGMP_OnlineServices_SocialInterface::RegisterForRealtimeServiceUpdates()
{
	std::shared_ptr<WebSocket> pWS = NGMP_OnlineServicesManager::GetWebSocket();
	if (pWS != nullptr)
	{
		pWS->SendData_SubscribeRealtimeUpdates();
	}

	m_bOverlayActive = true;
}

void NGMP_OnlineServices_SocialInterface::DeregisterForRealtimeServiceUpdates()
{
	std::shared_ptr<WebSocket> pWS = NGMP_OnlineServicesManager::GetWebSocket();
	if (pWS != nullptr)
	{
		pWS->SendData_UnsubscribeRealtimeUpdates();
	}

	m_bOverlayActive = false;
}

void NGMP_OnlineServices_SocialInterface::InvokeCallback_NewFriendRequest(std::string strDisplayName)
{
	// only if overlay isnt active
	if (!m_bOverlayActive)
	{
        ++m_numTotalNotifications;
        TriggerCallback_OnNumberGlobalNotificationsChanged();
	}

	if (m_cbOnNewFriendRequest != nullptr)
	{
		m_cbOnNewFriendRequest(strDisplayName);
	}

    bool bShowNotification = true;
    if (TheNGMPGame != nullptr && TheNGMPGame->isGameInProgress())
    {
        bShowNotification = NGMP_OnlineServicesManager::Settings.Social_Notifications_PlayerSendsRequest_Gameplay();
    }
    else
    {
        bShowNotification = NGMP_OnlineServicesManager::Settings.Social_Notifications_PlayerSendsRequest_Menus();
    }

	if (bShowNotification)
	{
		showNotificationBox(AsciiString(strDisplayName.c_str()), TheGameText->fetch("Buddy:AddNotification"));
	}
}

void NGMP_OnlineServices_SocialInterface::CommitLobbyPlayerListToRecentlyPlayedWithList()
{
    NGMP_OnlineServices_AuthInterface* pAuthInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_AuthInterface>();
    int64_t user_id = pAuthInterface != nullptr ? pAuthInterface->GetUserID() : -1;

    m_mapRecentlyPlayedWith.clear();

    if (TheNGMPGame != nullptr)
    {
        if (TheNGMPGame != nullptr)
        {
            for (Int i = 0; i < MAX_SLOTS; ++i)
            {
                NGMPGameSlot* slot = TheNGMPGame->getGameSpySlot(i);
                if (slot && slot->isHuman())
                {
                    int64_t profileID = slot->m_userID;

                    // dont allow self
                    if (profileID != user_id)
                    {
                        // dont show if already friends
                        if (!IsUserFriend(profileID))
                        {
                            FriendsEntry newEntry;
                            newEntry.user_id = profileID;
                            newEntry.display_name = to_utf8(slot->getName().str());
                            m_mapRecentlyPlayedWith.emplace(profileID, newEntry);
                        }
                    }
                }
            }
        }
    }

	m_RecentlyPlayedWithTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

