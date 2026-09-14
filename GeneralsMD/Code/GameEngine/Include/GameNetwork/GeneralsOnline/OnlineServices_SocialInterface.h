#pragma once

#include "NGMP_include.h"
#include "GameNetwork/RankPointValue.h"
#include "GameNetwork/GameSpy/PersistentStorageThread.h"

struct FriendsEntry
{
	int64_t user_id = -1;
	std::string display_name;
	bool online;
	std::string presence;
};

struct FriendsResult
{
	std::vector<FriendsEntry> vecFriends;
	std::vector<FriendsEntry> vecPendingRequests;
};

struct BlockedResult
{
	std::vector<FriendsEntry> vecBlocked;
};

class NGMP_OnlineServices_SocialInterface
{
public:
	NGMP_OnlineServices_SocialInterface();
	~NGMP_OnlineServices_SocialInterface();

	void GetFriendsList(bool bUseCache, std::function<void()> cb);

	void GetBlockList(std::function<void(BlockedResult blockResult)> cb);

	void AddFriend(int64_t target_user_id);
	void RemoveFriend(int64_t target_user_id);

	void IgnoreUser(int64_t target_user_id);
	void UnignoreUser(int64_t target_user_id);

	void AcceptPendingRequest(int64_t target_user_id);
	void RejectPendingRequest(int64_t target_user_id);

	void OnChatMessage(int64_t source_user_id, int64_t target_user_id, UnicodeString unicodeStr);
	void OnOnlineStatusChanged(std::string strDisplayName, bool bOnline);

	void OnFriendRequestAccepted(std::string strDisplayName);

	bool IsUserIgnored(int64_t target_user_id);
	bool IsUserFriend(int64_t target_user_id);
	bool IsUserPendingRequest(int64_t target_user_id);

	// NOTE: If we aren't registered for indepth notifications (only when UI is visible), we will just get online/offline status changes.
	//	     This cuts down on server traffic, and we don't need the additional info like presence etc unless the UI is visible
	void RegisterForRealtimeServiceUpdates();
	void DeregisterForRealtimeServiceUpdates();

	// TODO_SOCIAL: Store unread messages in DB so data isnt lost if user logs out or crashes etc without reading them?
	std::vector<UnicodeString> GetChatMessagesForUser(int64_t target_user_id)
	{
		if (m_mapCachedMessages.contains(target_user_id))
		{
			return m_mapCachedMessages[target_user_id];
		}

		return std::vector<UnicodeString>();
	}

	void ClearUnreadChatMessagesForUser(int64_t target_user_id)
	{
		m_mapUnreadMessagesForUser.erase(target_user_id);
	}

	int GetNumberUnreadChatMessagesForUser(int64_t target_user_id)
	{
        if (m_mapUnreadMessagesForUser.contains(target_user_id))
        {
            return m_mapUnreadMessagesForUser[target_user_id];
        }

        return 0;
	}

	// Callbacks
	void InvokeCallback_NewFriendRequest(std::string strDisplayName);
	void RegisterForCallback_NewFriendRequest(std::function<void(std::string strDisplayName)> cbOnNewFriendRequest)
	{
		m_cbOnNewFriendRequest = cbOnNewFriendRequest;
	}

	void RegisterForCallback_OnChatMessage(std::function<void(int64_t source_user_id, int64_t target_user_id, UnicodeString unicodeStr)> cbOnChatMessage)
	{
		m_cbOnChatMessage = cbOnChatMessage;
	}

	std::unordered_map<int64_t, FriendsEntry> GetRecentlyPlayedWithList()
    {
		// is it stale? clear it out
		const int64_t recentPlayersListLifespan = 600000; // 10 minutes
		int64_t currTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		if (currTime - m_RecentlyPlayedWithTimestamp >= recentPlayersListLifespan)
		{
			m_mapRecentlyPlayedWith.clear();
		}

        return m_mapRecentlyPlayedWith;
    }

	std::unordered_map<int64_t, FriendsEntry> GetCachedFriendsList()
	{
		return m_mapFriends;
	}

    std::unordered_map<int64_t, FriendsEntry> GetCachedRequestsList()
    {
        return m_mapPendingRequests;
    }

	void RegisterForCallback_OnNumberGlobalNotificationsChanged(std::function<void(int newNumNotifications)> cb)
	{
		m_cbOnNumberGlobalNotificationsChanged = cb;
	}

	void ClearGlobalNotificatations()
	{
		m_numTotalNotifications = 0;

		TriggerCallback_OnNumberGlobalNotificationsChanged();
	}

	void RegisterInitialPendingRequestsUponLogin(int num)
	{
		m_numTotalNotifications = num;

		if (num > 0)
		{
			TriggerCallback_OnNumberGlobalNotificationsChanged();
		}
	}

	int GetNumTotalNotifications() const
	{
		return std::max<int>(0, m_numTotalNotifications);
	}

	void CommitLobbyPlayerListToRecentlyPlayedWithList();

private:
	void TriggerCallback_OnNumberGlobalNotificationsChanged()
	{
		if (m_cbOnNumberGlobalNotificationsChanged)
		{
			m_cbOnNumberGlobalNotificationsChanged(m_numTotalNotifications);
		}
	}

    // This includes:
    // - one count per player who has sent us unread messages (we dont show number of total messages)
    // - number of pending friend requests
	int m_numTotalNotifications = 0;

	// NOTE: We cache messages here, because the UI isn't always present, but we dont want to miss messages
	// TODO_SOCIAL: Limit this
	std::map<int64_t, std::vector<UnicodeString>> m_mapCachedMessages; // user id here is who we are talking to / target user
	std::map<int64_t, int> m_mapUnreadMessagesForUser; // user id here is who we are talking to / target user

	std::function<void(int newNumNotifications)> m_cbOnNumberGlobalNotificationsChanged = nullptr;

	std::function<void()> m_cbOnGetFriendsList = nullptr;
	std::function<void(BlockedResult blockResult)> m_cbOnGetBlockList = nullptr;

	std::function<void(std::string strDisplayName)> m_cbOnNewFriendRequest = nullptr;

	std::function<void(int64_t source_user_id, int64_t target_user_id, UnicodeString unicodeStr)> m_cbOnChatMessage = nullptr;

	// Cached, may be out of date if friends UI isnt active, optimized for lookup
	std::unordered_map<int64_t, FriendsEntry> m_mapFriends;
	std::unordered_map<int64_t, FriendsEntry> m_mapPendingRequests;
	std::unordered_map<int64_t, FriendsEntry> m_mapBlocked;

	// managed on client / locally
	std::unordered_map<int64_t, FriendsEntry> m_mapRecentlyPlayedWith;
	int64_t m_RecentlyPlayedWithTimestamp = -1;

	bool m_bOverlayActive = false;
};
