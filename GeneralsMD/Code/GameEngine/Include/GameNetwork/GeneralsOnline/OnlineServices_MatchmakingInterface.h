#pragma once

#include "NGMP_include.h"

struct PlaylistMapEntry
{
	std::string Name = std::string();
	std::string Path = std::string();
	bool Custom = false;
};

struct PlaylistEntry
{
	uint16_t PlaylistID = -1;
	std::string Name = std::string();
	int MinPlayers = -1;
	int DesiredPlayers = -1;
	// GeneralsX @bugfix Android port 07/11/2026 - ported from upstream GeneralsOnline, used by WOLQuickMatchMenu.cpp
	int MinSelectedMaps = 0;
	bool AllowTeams = false;
	int TeamSize = -1;
	bool AllowArmySelection = false;
	uint16_t GracePeriodAtMinPlayersMSec = 0;
	std::vector<PlaylistMapEntry> Maps = std::vector<PlaylistMapEntry>();
};

class NGMP_OnlineServices_MatchmakingInterface
{
public:
	NGMP_OnlineServices_MatchmakingInterface();

	void RetrievePlaylists(std::function<void(std::vector<PlaylistEntry>)> callbackOnComplete);

	void WidenSearch();

	void StartMatchmaking(uint16_t playlistID, std::vector<int> vecSelectedMapIndexes, std::function<void(bool)> fnCallback);

	void CancelMatchmaking();

	std::vector<PlaylistEntry> GetCachedPlaylists() const { return m_vecCachedPlaylists; }
	PlaylistEntry GetCachedPlaylistFromID(uint16_t id) const
	{
		for (PlaylistEntry plEntry : m_vecCachedPlaylists)
		{
			if (plEntry.PlaylistID == id)
			{
				return plEntry;
			}
		}

		return PlaylistEntry();
	}

	PlaylistEntry GetCachedPlaylistFromIndex(int index) const
	{
		if (index < m_vecCachedPlaylists.size())
		{
			return m_vecCachedPlaylists.at(index);
		}

		return PlaylistEntry();
	}

private:
	std::vector<PlaylistEntry> m_vecCachedPlaylists;
};
