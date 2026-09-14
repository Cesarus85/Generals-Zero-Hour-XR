/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////
// FILE: WOLLobbyMenu.cpp
// Author: Chris Huybregts, November 2001
// Description: WOL Lobby Menu
///////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GameEngine.h"
#include "Common/GameState.h"
#include "Common/MiniLog.h"
#include "Common/MultiplayerSettings.h"
#include "Common/PlayerTemplate.h"
#include "Common/CustomMatchPreferences.h"
#include "Common/version.h"
#include "GameClient/AnimateWindowManager.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/Gadget.h"
#include "GameClient/GameClient.h"
#include "GameClient/Shell.h"
#include "GameClient/ShellHooks.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GadgetComboBox.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetSlider.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/GameText.h"
#include "GameClient/MessageBox.h"
#include "GameClient/Mouse.h"
#include "GameClient/Display.h"
#include "GameNetwork/GameSpyOverlay.h"
#include "GameClient/GameWindowTransitions.h"

#include "GameLogic/GameLogic.h"

#include "GameClient/LanguageFilter.h"
#include "GameNetwork/GameSpy/BuddyDefs.h"
#include "GameNetwork/GameSpy/GSConfig.h"
#include "GameNetwork/GameSpy/LadderDefs.h"
#include "GameNetwork/GameSpy/PeerDefs.h"
#include "GameNetwork/GameSpy/PeerThread.h"
#include "GameNetwork/GameSpy/PersistentStorageDefs.h"
#include "GameNetwork/GameSpy/PersistentStorageThread.h"
#include "GameNetwork/GameSpy/LobbyUtils.h"
#include "GameNetwork/RankPointValue.h"
#include "GameNetwork/GeneralsOnline/NGMP_interfaces.h"

void refreshGameList( Bool forceRefresh = FALSE );
void refreshPlayerList( Bool forceRefresh = FALSE );

#ifdef DEBUG_LOGGING
#define PERF_TEST
static LogClass s_perfLog("Perf.txt");
#define PERF_LOG(x) s_perfLog.log x
#else // DEBUG_LOGGING
#define PERF_LOG(x) {}
#endif // DEBUG_LOGGING

// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
static Bool isShuttingDown = false;
static Bool buttonPushed = false;
static const char *nextScreen = nullptr;
static Bool raiseMessageBoxes = false;
static time_t gameListRefreshTime = 0;
static const time_t gameListRefreshInterval = 4000;
static time_t playerListRefreshTime = 0;
static const time_t playerListRefreshInterval = 6000;
static bool isFirstRosterUpdate = false;

void setUnignoreText( WindowLayout *layout, AsciiString nick, GPProfile id);
static void doSliderTrack(GameWindow *control, Int val);
Bool DontShowMainMenu = FALSE;
enum { COLUMN_PLAYERNAME = 1 };

// window ids ------------------------------------------------------------------------------
static NameKeyType parentWOLLobbyID = NAMEKEY_INVALID;
static NameKeyType buttonBackID = NAMEKEY_INVALID;
static NameKeyType buttonHostID = NAMEKEY_INVALID;
static NameKeyType buttonRefreshID = NAMEKEY_INVALID;
static NameKeyType buttonJoinID = NAMEKEY_INVALID;
static NameKeyType buttonBuddyID = NAMEKEY_INVALID;
static NameKeyType buttonEmoteID = NAMEKEY_INVALID;
static NameKeyType textEntryChatID = NAMEKEY_INVALID;
static NameKeyType listboxLobbyPlayersID = NAMEKEY_INVALID;
static NameKeyType listboxLobbyChatID = NAMEKEY_INVALID;
static NameKeyType comboLobbyGroupRoomsID = NAMEKEY_INVALID;
//static NameKeyType // sliderChatAdjustID = NAMEKEY_INVALID;

// Window Pointers ------------------------------------------------------------------------
static GameWindow *parentWOLLobby = nullptr;
static GameWindow *buttonBack = nullptr;
static GameWindow *buttonHost = nullptr;
static GameWindow *buttonRefresh = nullptr;
static GameWindow *buttonJoin = nullptr;
static GameWindow *buttonBuddy = nullptr;
static GameWindow *buttonEmote = nullptr;
static GameWindow *textEntryChat = nullptr;
static GameWindow *listboxLobbyPlayers = nullptr;
static GameWindow *listboxLobbyChat = nullptr;
static GameWindow *comboLobbyGroupRooms = nullptr;
static GameWindow *parent = nullptr;

static Int groupRoomToJoin = 0;
static Int	initialGadgetDelay = 2;
static Bool justEntered = FALSE;

static int64_t s_lobbyLastChatTimeMs = 0;
static const int64_t S_LOBBY_CHAT_INTERVAL_MS = 3000; // how long to wait before we allow sending the next message

static bool LobbyChatSlowmodeAllowsSend()
{
	using namespace std::chrono;

	// GeneralsX @bugfix Android port 07/11/2026 - libc++ on Android NDK has no std::chrono::utc_clock yet, system_clock is fine here (no leap-second precision needed)
	int64_t nowMs =
		duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

	if (nowMs < s_lobbyLastChatTimeMs)
	{
		s_lobbyLastChatTimeMs = 0;
	}

	int64_t delta = nowMs - s_lobbyLastChatTimeMs;
	if (delta < S_LOBBY_CHAT_INTERVAL_MS)
	{
		if (listboxLobbyChat)
		{
			GadgetListBoxAddEntryText(
				listboxLobbyChat,
				UnicodeString(L"You are sending messages too quickly. Please wait a moment."),
				GameMakeColor(255, 0, 0, 255),
				-1,
				-1
			);
		}
		return false;
	}

	s_lobbyLastChatTimeMs = nowMs;
	return true;
}

#if defined(RTS_DEBUG)
Bool g_fakeCRC = FALSE;
Bool g_debugSlots = FALSE;
#endif

std::list<PeerResponse> TheLobbyQueuedUTMs;

// Slash commands -------------------------------------------------------------------------
extern "C" {
int getQR2HostingStatus();
}
extern int isThreadHosting;

Bool handleLobbySlashCommands(UnicodeString uText)
{
	AsciiString message;
	message.translate(uText);

	if (message.getCharAt(0) != '/')
	{
		return FALSE; // not a slash command
	}

	AsciiString remainder = message.str() + 1;
	AsciiString token;
	remainder.nextToken(&token);
	token.toLower();

	if (token == "host")
	{
		// TODO_NGMP
		/*
		UnicodeString s;
		s.format(L"Hosting qr2:%d thread:%d", getQR2HostingStatus(), isThreadHosting);
		TheGameSpyInfo->addText(s, GameSpyColor[GSCOLOR_DEFAULT], nullptr);
		*/
		return TRUE; // was a slash command
	}
	else if (token == "me" && uText.getLength()>4)
	{
		UnicodeString msg = UnicodeString(uText.str() + 4); // skip the /me
		NGMP_OnlineServices_RoomsInterface* pRoomsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_RoomsInterface>();
		if (pRoomsInterface != nullptr)
		{
			pRoomsInterface->SendChatMessageToCurrentRoom(msg, true);
		}
		return TRUE; // was a slash command
	}
	else if (token == "help" || token == "commands")
	{
		GadgetListBoxAddEntryText(listboxLobbyChat, UnicodeString(L"The following commands are available:"), GameSpyColor[GSCOLOR_CHAT_NORMAL], -1, -1);
		GadgetListBoxAddEntryText(listboxLobbyChat, UnicodeString(L"/name <value> - Changes your display name - Example: /name General Granger"), GameSpyColor[GSCOLOR_CHAT_NORMAL], -1, -1);
		return TRUE; // was a slash command
	}
	else if ((token == "name" && uText.getLength() > 6) || (token == "nick" && uText.getLength() > 6))
	{
		UnicodeString newName(uText.str() + 6); // skip the /name or nick

		if (newName.getLength() < 3 || newName.getLength() > 16)
		{
			GadgetListBoxAddEntryText(listboxLobbyChat, UnicodeString(L"Your new name must be between 3 and 16 characters."), GameMakeColor(255, 0, 0, 255), -1, -1);
		}
		else
		{
			std::shared_ptr<WebSocket>  pWS = NGMP_OnlineServicesManager::GetWebSocket();
			if (pWS != nullptr)
			{
				pWS->SendData_ChangeName(newName);
			}
		}
		
		return TRUE; // was a slash command
	}
	else if (token == "forcerelay")
	{
		extern bool g_bForceRelay;
		extern UnsignedInt m_exeCRCOriginal;
		g_bForceRelay = true;
		m_exeCRCOriginal = TheWritableGlobalData->m_exeCRC;
		TheWritableGlobalData->m_exeCRC = 123456;
		GadgetListBoxAddEntryText(listboxLobbyChat, UnicodeString(L"Relays are now forced on. You will only be able to join lobbies where the same option has been set. Use /allowrelay to reset this"), GameMakeColor(255, 0, 0, 255), -1, -1);
		return TRUE; // was a slash command
	}
	else if (token == "allowrelay")
	{
		extern bool g_bForceRelay;
		extern UnsignedInt m_exeCRCOriginal;
		g_bForceRelay = false;
		TheWritableGlobalData->m_exeCRC = m_exeCRCOriginal;
		m_exeCRCOriginal = 0;
		GadgetListBoxAddEntryText(listboxLobbyChat, UnicodeString(L"Relays are now optional again. You will only be able to join lobbies where the same option has been set. Use /forcerelay to reset this"), GameMakeColor(255, 0, 0, 255), -1, -1);
		return TRUE; // was a slash command
	}
	else if (token == "refresh")
	{
		// Added 2/19/03 added the game refresh
		refreshGameList(TRUE);
		refreshPlayerList(TRUE);
		return TRUE; // was a slash command
	}
	/*
	if (token == "togglegamelist")
	{
		NameKeyType buttonID = NAMEKEY("WOLCustomLobby.wnd:ButtonGameListToggle");
		GameWindow *button = TheWindowManager->winGetWindowFromId(parent, buttonID);
		if (button)
		{
			button->winHide(!button->winIsHidden());
		}
		return TRUE; // was a slash command
	}
	else if (token == "adjustchat")
	{
		NameKeyType sliderID = NAMEKEY("WOLCustomLobby.wnd:SliderChatAdjust");
		GameWindow *slider = TheWindowManager->winGetWindowFromId(parent, sliderID);
		if (slider)
		{
			slider->winHide(!slider->winIsHidden());
		}
		return TRUE; // was a slash command
	}
	*/
#if defined(RTS_DEBUG)
	else if (token == "fakecrc")
	{
		g_fakeCRC = !g_fakeCRC;
		TheGameSpyInfo->addText(L"Toggled CRC fakery", GameSpyColor[GSCOLOR_DEFAULT], nullptr);
		return TRUE; // was a slash command
	}
	else if (token == "slots")
	{
		g_debugSlots = !g_debugSlots;
		TheGameSpyInfo->addText(L"Toggled SlotList debug", GameSpyColor[GSCOLOR_DEFAULT], nullptr);
		return TRUE; // was a slash command
	}
#endif

	return FALSE; // not a slash command
}

static Bool s_tryingToHostOrJoin = FALSE;
void SetLobbyAttemptHostJoin(Bool start)
{
	// GeneralsX @bugfix Android port 11/07/2026 verbose on purpose -- this
	// flag being stuck TRUE is what makes the Back button (ExitState()) go
	// silently dead, and that's been hard to pin down from device logs
	// without seeing every place this flag actually changes.
	fprintf(stderr, "DEBUG-UI: SetLobbyAttemptHostJoin(%d) (was %d)\n", (int)start, (int)s_tryingToHostOrJoin);
	fflush(stderr);
	s_tryingToHostOrJoin = start;
}

// Tooltips -------------------------------------------------------------------------------

static void playerTooltip(GameWindow *window,
													WinInstanceData *instData,
													UnsignedInt mouse)
{
	// TODO_NGMP: Support all of this again

	Int x, y, row, col;
	x = LOLONGTOSHORT(mouse);
	y = HILONGTOSHORT(mouse);

	GadgetListBoxGetEntryBasedOnXY(window, x, y, row, col);

	if (row == -1 || col == -1)
	{
		TheMouse->setCursorTooltip(UnicodeString::TheEmptyString);//TheGameText->fetch("TOOLTIP:PlayersInLobby") );
		return;
	}

	UnicodeString uName = GadgetListBoxGetText(window, row, COLUMN_PLAYERNAME);

	// TODO_NGMP: This causes issues with duplicate names. We should have better ways of looking this up + perhaps only allow unique names
	NGMP_OnlineServices_RoomsInterface* pRoomsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_RoomsInterface>();
	NGMP_OnlineServices_AuthInterface* pAuthInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_AuthInterface>();
	NGMP_OnlineServices_StatsInterface* pStatsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_StatsInterface>();
	NGMP_OnlineServices_SocialInterface* pSocialInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_SocialInterface>();
	if (pRoomsInterface != nullptr && pAuthInterface != nullptr && pStatsInterface != nullptr && pSocialInterface != nullptr)
	{
		// GeneralsX @bugfix Android port 07/11/2026 - Cast via uintptr_t for 64-bit
		int profileID = static_cast<int>(reinterpret_cast<uintptr_t>(GadgetListBoxGetItemData(listboxLobbyPlayers, row, 0)));
		NetworkRoomMember* roomMember = pRoomsInterface->GetRoomMemberFromID(profileID);

		// TODO_NGMP: This is an async call, we should block future popups until it returns to avoid weirdness
		if (col > 0)
		{
			if (roomMember != nullptr)
			{
				// GeneralsX @bugfix Android port 11/07/2026 roomMember is a raw
				// pointer into NGMP_OnlineServices_RoomsInterface's internal room
				// roster, which gets rebuilt (e.g. refreshPlayerList() /
				// "Repopulating network room roster") whenever the room's player
				// list changes -- this async stats lookup can easily still be
				// in flight when that happens (hover-triggered, no user action
				// needed to wait it out), leaving roomMember dangling by the
                // time this lambda fires. Capture the stable int64 user_id
				// instead and use that inside the lambda; the interface
				// pointers themselves are safe to capture since they're
				// long-lived singletons, not per-roster data.
				int64_t capturedUserID = roomMember->user_id;
				pStatsInterface->findPlayerStatsByID(capturedUserID, [=](bool bSuccess, PSPlayerStats stats)
					{
						if (!bSuccess)
						{
							TheMouse->setCursorTooltip(UnicodeString(L"Error: 1"), -1, NULL, 1.5f);
						}
						else
						{
							UnicodeString tooltip = UnicodeString::TheEmptyString;
							if (capturedUserID == pAuthInterface->GetUserID())
							{
								tooltip.format(TheGameText->fetch("TOOLTIP:LocalPlayer"), uName.str());
							}
							else
							{
								// not us
								bool bIsFriend = pSocialInterface->IsUserFriend(roomMember->user_id);
								if (bIsFriend)
								{
									// buddy
									tooltip.format(TheGameText->fetch("TOOLTIP:BuddyPlayer"), uName.str());
								}
								else
								{
									// non-buddy profiled player
									tooltip.format(TheGameText->fetch("TOOLTIP:ProfiledPlayer"), uName.str());

									// NOTE: Removed non-profiled generic player, this doesn't exist on Generals Online, everyone has a profile
								}
							}

							bool bIgnored = pSocialInterface->IsUserIgnored(roomMember->user_id);
							if (bIgnored)
							{
								tooltip.concat(TheGameText->fetch("TOOLTIP:IgnoredModifier"));
							}

							// ELO data
							UnicodeString tmp;
							tmp.format(L"\n\nElo Rating: %d (in %d matches)", stats.elo_rating, stats.elo_num_matches);
							tooltip.concat(tmp);
							Int rankPoints = CalculateRank(stats);
							Int rank = 0;
							Int i = 0;
							if (TheRankPointValues != nullptr)
							{
								while (i + 1 < MAX_RANKS && rankPoints >= TheRankPointValues->m_ranks[i + 1])
									++i;
							}
							rank = i;

							// determine favorite side
							Int mostGames = 0;
							Int favorite = 0;
							for (auto it = stats.games.begin(); it != stats.games.end(); ++it)
							{
								if (it->second >= mostGames)
								{
									mostGames = it->second;
									favorite = it->first;
								}
							}

							AsciiString sideName = "GUI:RandomSide";
							if (mostGames > 0)
							{
								if (favorite > 1) // cant be civilian or observer
								{
									const PlayerTemplate* fac = ThePlayerTemplateStore->getNthPlayerTemplate(favorite);
									if (fac)
									{
										sideName.format("SIDE:%s", fac->getSide().str());
									}
								}
							}
							AsciiString rankName;
							rankName.format("GUI:GSRank%d", rank);
							
							tmp.clear();
							tmp.format(L"\n\nFavorite Side: %ls\nRank: %ls", TheGameText->fetch(sideName).str(), TheGameText->fetch(rankName).str());
							tooltip.concat(tmp);

							int totalWins = 0;
							int totalLosses = 0;
							int totalDC = 0;
							int totalWinsInRow = 0;
							int totalLossesInRow = 0;
							int totalDCInRow = 0;
							int maxWinsInRow = 0;
							int maxLossesInRow = 0;
							int maxDCInRow = 0;

							for (int i = 0; i < stats.wins.size(); ++i) { totalWins += stats.wins[i]; }
							for (int i = 0; i < stats.losses.size(); ++i) { totalLosses += stats.losses[i]; }
							for (int i = 0; i < stats.discons.size(); ++i) { totalDC += stats.discons[i]; }

                            totalWinsInRow = stats.winsInARow;
                            totalLossesInRow = stats.lossesInARow;
                            totalDCInRow = stats.disconsInARow;

                            maxWinsInRow = stats.maxWinsInARow;
                            maxLossesInRow = stats.maxLossesInARow;

							tmp.clear();
							tmp.format(L"\n\nTotal Wins: %d\nTotal Losses: %d\nTotal Disconnects: %d\n\nCurrent Win Streak: %d\nCurrent Loss Streak: %d\nCurrent Disconnect Streak: %d\n\nLongest Win Streak: %d\nLongest Loss Streak: %d\nLongest Disconnect Streak: %d",
								totalWins,
								totalLosses,
								totalDC,
								totalWinsInRow,
								totalLossesInRow,
								totalDCInRow,
								maxWinsInRow,
								maxLossesInRow,
								maxDCInRow);
							tooltip.concat(tmp);

							if (pRoomsInterface != nullptr && pAuthInterface != nullptr)
							{
								NetworkRoomMember* localMember = pRoomsInterface->GetRoomMemberFromID(pAuthInterface->GetUserID());
								if (localMember != nullptr && localMember->m_bIsAdmin)
								{
									UnicodeString idLine;
									idLine.format(L"\n\nUser ID: %lld", capturedUserID);
									tooltip.concat(idLine);
								}
							}

							TheMouse->setCursorTooltip(tooltip, -1, NULL, 1.5f); // the text and width are the only params used.  the others are the default values.
						}
					}, EStatsRequestPolicy::RESPECT_CACHE_ALLOW_REQUEST);
			}
			else
			{
				TheMouse->setCursorTooltip(UnicodeString(L"Error: 1"), -1, NULL, 1.5f); // the text and width are the only params used.  the others are the default values.
			}



		}
	}


	return;

	// TODO_NGMP:
	/*
	PlayerInfoMap::iterator it = TheGameSpyInfo->getPlayerInfoMap()->find(aName);
	PlayerInfo *info = &(it->second);
	Bool isLocalPlayer = (TheGameSpyInfo->getLocalName().compareNoCase(info->m_name) == 0);

	if (col == 0)
	{
		if (info->m_preorder)
		{
			TheMouse->setCursorTooltip( TheGameText->fetch("TOOLTIP:LobbyOfficersClub") );
		}
		else
		{
			TheMouse->setCursorTooltip( UnicodeString::TheEmptyString);
		}
		return;
	}

	AsciiString	playerLocale = info->m_locale;
	AsciiString localeIdentifier;
	localeIdentifier.format("WOL:Locale%2.2d", atoi(playerLocale.str()));
	Int					playerWins   = info->m_wins;
	Int					playerLosses = info->m_losses;
	UnicodeString	playerInfo;
	playerInfo.format(TheGameText->fetch("TOOLTIP:PlayerInfo"), TheGameText->fetch(localeIdentifier).str(), playerWins, playerLosses);

	UnicodeString tooltip = UnicodeString::TheEmptyString;//TheGameText->fetch("TOOLTIP:PlayersInLobby");
	if (isLocalPlayer)
	{
		tooltip.format(TheGameText->fetch("TOOLTIP:LocalPlayer"), uName.str());
	}
	else
	{
		// not us
		if (TheGameSpyInfo->getBuddyMap()->find(info->m_profileID) != TheGameSpyInfo->getBuddyMap()->end())
		{
			// buddy
			tooltip.format(TheGameText->fetch("TOOLTIP:BuddyPlayer"), uName.str());
		}
		else
		{
			if (info->m_profileID)
			{
				// non-buddy profiled player
				tooltip.format(TheGameText->fetch("TOOLTIP:ProfiledPlayer"), uName.str());
			}
			else
			{
				// non-profiled player
				tooltip.format(TheGameText->fetch("TOOLTIP:GenericPlayer"), uName.str());
			}
		}
	}

	if (info->isIgnored())
	{
		tooltip.concat(TheGameText->fetch("TOOLTIP:IgnoredModifier"));
	}

	if (info->m_profileID)
	{
		tooltip.concat(playerInfo);
	}

	if (!TheRankPointValues)
		return;

	Int rank = 0;
	Int i = 0;
	while (i + 1 < MAX_RANKS && info->m_rankPoints >= TheRankPointValues->m_ranks[i + 1])
		++i;
	rank = i;
	AsciiString sideName = "GUI:RandomSide";
	if (info->m_side > 0)
	{
		const PlayerTemplate *fac = ThePlayerTemplateStore->getNthPlayerTemplate(info->m_side);
		if (fac)
		{
			sideName.format("SIDE:%s", fac->getSide().str());
		}
	}
	AsciiString rankName;
	rankName.format("GUI:GSRank%d", rank);
	UnicodeString tmp;
	tmp.format(L"\n%ls %ls", TheGameText->fetch(sideName).str(), TheGameText->fetch(rankName).str());
	tooltip.concat(tmp);

	TheMouse->setCursorTooltip( tooltip, -1, nullptr, 1.5f ); // the text and width are the only params used.  the others are the default values.
	*/
}

static void PopulateLobbyFilterComboBox(GameWindow* comboBox)
{
	if (comboBox == nullptr)
		return;

	extern LobbyGameModeFilter theLobbyFilter;
	GadgetComboBoxReset(comboBox);

	Int idx;
	idx = GadgetComboBoxAddEntry(comboBox, UnicodeString(L"Filter: All"),
		GameSpyColor[GSCOLOR_DEFAULT]); GadgetComboBoxSetItemData(comboBox, idx, (void*)LOBBY_FILTER_ALL);
	idx = GadgetComboBoxAddEntry(comboBox, UnicodeString(L"Filter: 1v1"),
		GameSpyColor[GSCOLOR_DEFAULT]); GadgetComboBoxSetItemData(comboBox, idx, (void*)LOBBY_FILTER_1V1);
	idx = GadgetComboBoxAddEntry(comboBox, UnicodeString(L"Filter: Team Games"),
		GameSpyColor[GSCOLOR_DEFAULT]); GadgetComboBoxSetItemData(comboBox, idx, (void*)LOBBY_FILTER_TEAM);
	idx = GadgetComboBoxAddEntry(comboBox, UnicodeString(L"Filter: FFA"),
		GameSpyColor[GSCOLOR_DEFAULT]); GadgetComboBoxSetItemData(comboBox, idx, (void*)LOBBY_FILTER_FFA);
	idx = GadgetComboBoxAddEntry(comboBox, UnicodeString(L"Filter: AOD"),
		GameSpyColor[GSCOLOR_DEFAULT]); GadgetComboBoxSetItemData(comboBox, idx, (void*)LOBBY_FILTER_AOD);

	GadgetComboBoxSetSelectedPos(comboBox, (Int)theLobbyFilter);
}

static const char *const rankNames[] = {
	"Private",
	"Corporal",
	"Sergeant",
	"Lieutenant",
	"Captain",
	"Major",
	"Colonel",
	"General",
	"Brigadier",
	"Commander",
};
static_assert(ARRAY_SIZE(rankNames) == MAX_RANKS, "Incorrect array size");


const Image* LookupSmallRankImage(Int side, Int rankPoints)
{
	if (rankPoints == 0 || !TheRankPointValues)
		return nullptr;

	Int rank = 0;
	Int i = 0;
	while (i + 1 < MAX_RANKS && rankPoints >= TheRankPointValues->m_ranks[i + 1])
		++i;
	rank = i;

	if (rank < 0 || rank >= 10)
		return nullptr;

	AsciiString sideStr = "N";
	switch(side)
	{
		case 2:  //USA
		case 5:  //Super Weapon
		case 6:  //Laser
		case 7:  //Air Force
			sideStr = "USA";
			break;

		case 3:  //China
		case 8:  //Tank
		case 9:  //Infantry
		case 10: //Nuke
			sideStr = "CHA";
			break;

		case 4:  //GLA
		case 11: //Toxin
		case 12: //Demolition
		case 13: //Stealth
			sideStr = "GLA";
			break;
	}

	AsciiString fullImageName;
	fullImageName.format("%s-%s", rankNames[rank], sideStr.str());
	const Image *img = TheMappedImageCollection->findImageByName(fullImageName);
	DEBUG_ASSERTLOG(img, ("*** Could not load small rank image '%s' from TheMappedImageCollection!", fullImageName.str()));
	return img;
}

static Int insertPlayerInListbox(const PlayerInfo& info, Color color)
{
#if defined(GENERALS_ONLINE)
	UnicodeString uStr = info.m_nameUni;
#else
	UnicodeString uStr;
	uStr.translate(info.m_name);
#endif

	Int currentRank = info.m_rankPoints;
	Int currentSide = info.m_side;
	/* since PersistentStorage updates now update PlayerInfo, we don't need this.
	if (info.m_profileID)
	{
		PSPlayerStats psStats = TheGameSpyPSMessageQueue->findPlayerStatsByID(info.m_profileID);
		if (psStats.id)
		{
			currentRank = CalculateRank(psStats);

			PerGeneralMap::iterator it;
			Int numGames = 0;
			for(it = psStats.games.begin(); it != psStats.games.end(); ++it)
			{
				if(it->second >= numGames)
				{
					numGames = it->second;
					currentSide = it->first;
				}
			}
			if(numGames == 0 || psStats.gamesAsRandom >= numGames )
			{
				currentSide = 0;
			}
		}
	}
	*/

	// TODO_NGMP: Reimplement this, what were the pre-order bonuses?
	Bool isPreorder = true;
	//Bool isPreorder = TheGameSpyInfo->didPlayerPreorder(info.m_profileID);

	const Image *preorderImg = TheMappedImageCollection->findImageByName("OfficersClubsmall");
	Int w = (preorderImg)?preorderImg->getImageWidth():10;
	//Int h = (preorderImg)?preorderImg->getImageHeight():10;
	w = min(GadgetListBoxGetColumnWidth(listboxLobbyPlayers, 0), w);
	Int h = w;
	if (!isPreorder)
		preorderImg = nullptr;

	const Image *rankImg = LookupSmallRankImage(currentSide, currentRank);

#if 0  //Officer's Club (preorder image) no longer used in Zero Hour
	Int index = GadgetListBoxAddEntryImage(listboxLobbyPlayers, preorderImg, -1, 0, w, h);
	GadgetListBoxAddEntryImage(listboxLobbyPlayers, rankImg, index, 1, w, h);
	GadgetListBoxAddEntryText(listboxLobbyPlayers, uStr, color, index, 2);
#else
	Int index = GadgetListBoxAddEntryImage(listboxLobbyPlayers, rankImg, -1, 0, w, h);
	GadgetListBoxAddEntryText(listboxLobbyPlayers, uStr, color, index, 1);
#endif

	// attach data
	GadgetListBoxSetItemData(listboxLobbyPlayers, (void*)info.m_profileID, index);
	return index;
}

std::vector<int64_t> m_vecUsersProcessed;

// GeneralsX @bugfix Android port 11/07/2026 PopulateLobbyPlayerListbox()'s
// findPlayerStatsByBatch completion callback below is re-entrant: it can be
// triggered again (e.g. by a roster-changed event) while a PREVIOUS call's
// completion callback is still running its GadgetListBoxReset()+rebuild --
// GadgetListBoxReset() frees every existing row's DisplayString, so a second
// invocation clearing/rebuilding listboxLobbyPlayers while the first one is
// still constructing captions for it (heavy Cyrillic-name font/glyph work,
// per a real-device crash log) is a plausible use-after-free. The analogous
// game-list refresh (LobbyUtils.cpp's RefreshGameListBox / SearchForLobbies)
// already guards against this with m_bSearchInProgress; this path had no
// equivalent. Mirror that pattern here.
static Bool s_playerListboxRefreshInProgress = FALSE;

void PopulateLobbyPlayerListbox()
{
	if (s_playerListboxRefreshInProgress)
		return;

	NGMP_OnlineServices_RoomsInterface* pRoomsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_RoomsInterface>();
	NGMP_OnlineServices_StatsInterface* pStatsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_StatsInterface>();
	NGMP_OnlineServices_AuthInterface* pAuthInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_AuthInterface>();
	NGMP_OnlineServices_SocialInterface* pSocialInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_SocialInterface>();
	if (pRoomsInterface != nullptr && pStatsInterface != nullptr && pAuthInterface != nullptr && pSocialInterface != nullptr)
	{
		int64_t localUserID = pAuthInterface->GetUserID();

		// work out which stats we have, and which we need to bulk request
		std::vector<int64_t> vecUserStatsToRequest;
        for (auto kvPair : pRoomsInterface->GetMembersListForCurrentRoom())
        {
            NetworkRoomMember& netRoomMember = kvPair.second;

			if (!pStatsInterface->ArePlayerStatsCached(netRoomMember.user_id))
			{
				vecUserStatsToRequest.push_back(netRoomMember.user_id);
			}
		}

		// now batch request stats
		s_playerListboxRefreshInProgress = TRUE;
		pStatsInterface->findPlayerStatsByBatch(vecUserStatsToRequest, [=](bool bSuccess)
			{
				// GeneralsX @bugfix Android port 11/07/2026 clears
				// s_playerListboxRefreshInProgress when this lambda invocation
				// ends, on EVERY exit path (including the early `return` a bit
				// further down for duplicate entries) -- see comment above
				// PopulateLobbyPlayerListbox().
				std::shared_ptr<void> refreshInProgressGuard(nullptr, [](void*) { s_playerListboxRefreshInProgress = FALSE; });

				// NOTE: We dont clear until we get a response, so there's no period where the box is empty
                // save off old selection
                // GeneralsX @bugfix Android port 11/07/2026 selectedIndices used
                // to be an uninitialized Int* whose own ADDRESS was passed to
                // GadgetListBoxGetSelected() -- for a multi-select listbox that
                // function copies an array of up to GadgetListBoxGetMaxSelectedLength()
                // indices into the caller's buffer, so passing a single
                // uninitialized pointer variable (not a real buffer) let the
                // copy write past it and left `selectedIndices` holding
                // whatever garbage that overwrite produced. Confirmed via a
                // real-device crash right after this call, reading through the
                // resulting wild pointer at selectedIndices[i] below.
                Int maxSelectedItems = GadgetListBoxGetNumEntries(listboxLobbyPlayers);
                std::vector<Int> selectedIndicesBuf(GadgetListBoxGetMaxSelectedLength(listboxLobbyPlayers));
                GadgetListBoxGetSelected(listboxLobbyPlayers, selectedIndicesBuf.data());
                Int* selectedIndices = selectedIndicesBuf.data();
                std::set<int> selectedUserIDs;
                Int numSelected = 0;
                for (Int i = 0; i < maxSelectedItems; ++i)
                {
                    if (selectedIndices[i] < 0)
                    {
                        break;
                    }
                    ++numSelected;

                    // GeneralsX @bugfix Android port 07/11/2026 - Cast via uintptr_t for 64-bit
                    int profileID = static_cast<int>(reinterpret_cast<uintptr_t>(GadgetListBoxGetItemData(listboxLobbyPlayers, selectedIndices[i], 0)));
                    selectedUserIDs.insert(profileID);
                }

                // save off old top entry
                Int previousTopIndex = GadgetListBoxGetTopVisibleEntry(listboxLobbyPlayers);

                // reset UI
                m_vecUsersProcessed.clear();
                GadgetListBoxReset(listboxLobbyPlayers);

				std::set<Int> indicesToSelect;

				// by this point, all stats should be cached - they were either already cached, or we just got them back from the service
				// sort
                std::vector<NetworkRoomMember> sorted;

                {
                    auto membersMAp = pRoomsInterface->GetMembersListForCurrentRoom();
                    sorted.reserve(membersMAp.size());

                    for (auto& [id, member] : membersMAp) {
                        NetworkRoomMember copy = member;

                        // Precompute lowercase sort key
                        copy.sort_key.resize(copy.display_name.size());
                        std::transform(
                            copy.display_name.begin(),
                            copy.display_name.end(),
                            copy.sort_key.begin(),
                            [](unsigned char c) { return std::tolower(c); }
                        );

                        sorted.emplace_back(std::move(copy));
                    }

                    // Case-insensitive alphabetical sort
                    std::sort(sorted.begin(), sorted.end(),
                        [](const auto& a, const auto& b) {
                            return a.sort_key < b.sort_key;
                        });

                    // Admin/staff first
					auto adminSorted = std::stable_partition(sorted.begin(), sorted.end(),
                        [](const auto& x) {
                            return x.m_bIsAdmin;
                        });

					// friends next, after admin and if not admin
					std::stable_partition(adminSorted, sorted.end(),
						[=](const auto& x)
						{
							return pSocialInterface->IsUserFriend(x.user_id);
						});
                }


				for (const NetworkRoomMember& netRoomMember : sorted)
				{
					// safety, this is async so we could in theory get delayed callbacks resulting in dupes
					if (std::find(m_vecUsersProcessed.begin(), m_vecUsersProcessed.end(), netRoomMember.user_id) != m_vecUsersProcessed.end())
					{
						return;
					}

                    PSPlayerStats stats = PSPlayerStats();
					// we dont care about result here - always add them, with empty stats if we dont get stats
					pStatsInterface->getPlayerStatsFromCache(netRoomMember.user_id, &stats);

					m_vecUsersProcessed.push_back(netRoomMember.user_id);
					PlayerInfo pi;

					pi.m_name = AsciiString(netRoomMember.display_name.c_str());
					pi.m_nameUni = UnicodeString(from_utf8(netRoomMember.display_name).c_str());

					// if we don't have the stats from the server, just add us without any stats
					//if (bSuccess)
					{
						Int currentRank = 0;
						if (!TheRankPointValues)
							continue;

						Int rankPoints = CalculateRank(stats);
						Int i = 0;
						while (i + 1 < MAX_RANKS && rankPoints >= TheRankPointValues->m_ranks[i + 1])
							++i;
						currentRank = i;

						PerGeneralMap::iterator it;
						Int numWins = 0;
						Int numLosses = 0;
						Int numDiscons = 0;
						Int numGamesTotal = 0;
						for (it = stats.wins.begin(); it != stats.wins.end(); ++it)
						{
							numWins += it->second;
						}
						for (it = stats.losses.begin(); it != stats.losses.end(); ++it)
						{
							numLosses += it->second;
						}
						for (it = stats.discons.begin(); it != stats.discons.end(); ++it)
						{
							numDiscons += it->second;
						}
						for (it = stats.desyncs.begin(); it != stats.desyncs.end(); ++it)
						{
							numDiscons += it->second;
						}

						numDiscons += GetAdditionalDisconnectsFromUserFile(netRoomMember.user_id);

						numGamesTotal = numWins + numLosses + numDiscons;

						// determine favorite army
						Int numGamesThisArmy = 0;
						Int favorite = 0;
						for (it = stats.games.begin(); it != stats.games.end(); ++it)
						{
							if (it->second >= numGamesThisArmy)
							{
								numGamesThisArmy = it->second;
								favorite = it->first;
							}
						}

						int favoriteSide = PLAYERTEMPLATE_RANDOM;
						if (numGamesThisArmy == 0)
						{
							favoriteSide = 0; // this isnt a real army, but they also havent played any games so they cant possibly have a rank
						}
						else if (stats.gamesAsRandom >= numGamesThisArmy)
						{
							favoriteSide = PLAYERTEMPLATE_RANDOM;
						}
						else
						{
							favoriteSide = favorite;

							/*
							const PlayerTemplate* fac = ThePlayerTemplateStore->getNthPlayerTemplate(favorite);
							if (fac)
							{
								AsciiString side;
								side.format("SIDE:%s", fac->getSide().str());

								favoriteSide = TheGameText->fetch(side);
							}
							*/
						}

						// store on playerinfo object
						pi.m_wins = numWins;
						pi.m_losses = numLosses;
						pi.m_profileID = netRoomMember.user_id; // TODO_NGMP: Downcast... we need to use int64_t everywhere really
						pi.m_flags = 0;
						pi.m_rankPoints = rankPoints;
						pi.m_side = favorite;
						pi.m_preorder = 0;
					}

					// restore top visible entry
					GadgetListBoxSetTopVisibleEntry(listboxLobbyPlayers, previousTopIndex);

					NGMP_OnlineServices_SocialInterface* pSocialInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_SocialInterface>();

					bool bFriend = pSocialInterface != nullptr ? pSocialInterface->IsUserFriend(netRoomMember.user_id) : false;
					bool bIgnored = pSocialInterface != nullptr ? pSocialInterface->IsUserIgnored(netRoomMember.user_id) : false;
					bool bLocal = localUserID == netRoomMember.user_id;

					Color colorToUse = GameSpyColor[GSCOLOR_PLAYER_NORMAL];
					if (netRoomMember.m_bIsAdmin)
					{
						colorToUse = GameSpyColor[GSCOLOR_PLAYER_OWNER];;// GameMakeColor(0, 162, 232, 255);
					}
					else if (bFriend)
					{
						colorToUse = GameSpyColor[GSCOLOR_PLAYER_BUDDY];
					}
					else if (bIgnored)
					{
						colorToUse = GameSpyColor[GSCOLOR_PLAYER_IGNORED];
					}
					else if (bLocal)
					{
						colorToUse = GameSpyColor[GSCOLOR_PLAYER_SELF];
					}

					Int index = insertPlayerInListbox(pi, colorToUse);

					// TODO_NGMP: Use int for user ID like gamespy did, or move everything to uint64
					std::set<int>::const_iterator selIt = selectedUserIDs.find(netRoomMember.user_id);
					if (selIt != selectedUserIDs.end())
					{
						indicesToSelect.insert(index);
					}
				}

                // restore selection
                if (indicesToSelect.size())
                {
                    std::set<Int>::const_iterator indexIt = indicesToSelect.begin();
                    const size_t count = indicesToSelect.size();
                    size_t index = 0;
                    Int* newIndices = NEW Int[count];
                    while (index < count)
                    {
                        newIndices[index] = *indexIt;
                        DEBUG_LOG(("Queueing up index %d to re-select", *indexIt));
                        ++index;
                        ++indexIt;
                    }
                    GadgetListBoxSetSelected(listboxLobbyPlayers, newIndices, count);
                    delete[] newIndices;
                }

                if (indicesToSelect.size() != numSelected)
                {
                    TheWindowManager->winSetLoneWindow(NULL);
                }
			});

		/*
		for (auto kvPair :pRoomsInterface->GetMembersListForCurrentRoom())
		{
			NetworkRoomMember& netRoomMember = kvPair.second;

			// TODO_NGMP: Add a batched request
			// TODO_NGMP: Add a timeout to this where we just add the person with no stats
			pStatsInterface->findPlayerStatsByID(netRoomMember.user_id, [=](bool bSuccess, PSPlayerStats stats)
				{
					

					// TODO_NGMP: We should wait until the entire fresh / stats retrieval is done before restoring selections etc
				}, EStatsRequestPolicy::RESPECT_CACHE_ALLOW_REQUEST);
		}
		*/
	}

	return;

	if (!listboxLobbyPlayers)
		return;

	// Display players
	PlayerInfoMap *players = TheGameSpyInfo->getPlayerInfoMap();
	PlayerInfoMap::iterator it;
	BuddyInfoMap *buddies = TheGameSpyInfo->getBuddyMap();
	BuddyInfoMap::iterator bIt;
	if (listboxLobbyPlayers)
	{
		// save off old selection
		Int maxSelectedItems = GadgetListBoxGetNumEntries(listboxLobbyPlayers);
		Int *selectedIndices;
		GadgetListBoxGetSelected(listboxLobbyPlayers, (Int *)(&selectedIndices));
		std::set<AsciiString> selectedNames;
		std::set<AsciiString>::const_iterator selIt;
		std::set<Int> indicesToSelect;
		UnicodeString uStr;
		Int numSelected = 0;
		for (Int i=0; i<maxSelectedItems; ++i)
		{
			if (selectedIndices[i] < 0)
			{
				break;
			}
			++numSelected;
			AsciiString selectedName;
			uStr = GadgetListBoxGetText(listboxLobbyPlayers, selectedIndices[i], 2);
			selectedName.translate(uStr);
			selectedNames.insert(selectedName);
			DEBUG_LOG(("Saving off old selection %d (%s)", selectedIndices[i], selectedName.str()));
		}

		// save off old top entry
		Int previousTopIndex = GadgetListBoxGetTopVisibleEntry(listboxLobbyPlayers);

		GadgetListBoxReset(listboxLobbyPlayers);

		// Ops
		for (it = players->begin(); it != players->end(); ++it)
		{
			PlayerInfo info = it->second;
			if (info.m_flags & PEER_FLAG_OP || TheGameSpyConfig->isPlayerVIP(info.m_profileID))
			{
				Int index = insertPlayerInListbox(info, info.isIgnored()?GameSpyColor[GSCOLOR_PLAYER_IGNORED]:GameSpyColor[GSCOLOR_PLAYER_OWNER]);

				selIt = selectedNames.find(info.m_name);
				if (selIt != selectedNames.end())
				{
					DEBUG_LOG(("Marking index %d (%s) to re-select", index, info.m_name.str()));
					indicesToSelect.insert(index);
				}
			}
		}

		// Buddies
		for (it = players->begin(); it != players->end(); ++it)
		{
			PlayerInfo info = it->second;
			bIt = buddies->find(info.m_profileID);
			if ( !(info.m_flags & PEER_FLAG_OP || TheGameSpyConfig->isPlayerVIP(info.m_profileID)) && bIt != buddies->end() )
			{
				Int index = insertPlayerInListbox(info, info.isIgnored()?GameSpyColor[GSCOLOR_PLAYER_IGNORED]:GameSpyColor[GSCOLOR_PLAYER_BUDDY]);

				selIt = selectedNames.find(info.m_name);
				if (selIt != selectedNames.end())
				{
					DEBUG_LOG(("Marking index %d (%s) to re-select", index, info.m_name.str()));
					indicesToSelect.insert(index);
				}
			}
		}

		// Everyone else
		for (it = players->begin(); it != players->end(); ++it)
		{
			PlayerInfo info = it->second;
			bIt = buddies->find(info.m_profileID);
			if ( !(info.m_flags & PEER_FLAG_OP || TheGameSpyConfig->isPlayerVIP(info.m_profileID)) && bIt == buddies->end() )
			{
				Int index = insertPlayerInListbox(info, info.isIgnored()?GameSpyColor[GSCOLOR_PLAYER_IGNORED]:GameSpyColor[GSCOLOR_PLAYER_NORMAL]);

				selIt = selectedNames.find(info.m_name);
				if (selIt != selectedNames.end())
				{
					DEBUG_LOG(("Marking index %d (%s) to re-select", index, info.m_name.str()));
					indicesToSelect.insert(index);
				}
			}
		}

		// restore selection
		if (!indicesToSelect.empty())
		{
			std::set<Int>::const_iterator indexIt = indicesToSelect.begin();
			const size_t count = indicesToSelect.size();
			size_t index = 0;
			Int *newIndices = NEW Int[count];
			while (index < count)
			{
				newIndices[index] = *indexIt;
				DEBUG_LOG(("Queueing up index %d to re-select", *indexIt));
				++index;
				++indexIt;
			}
			GadgetListBoxSetSelected(listboxLobbyPlayers, newIndices, count);
			delete[] newIndices;
		}

		if (indicesToSelect.size() != numSelected)
		{
			TheWindowManager->winSetLoneWindow(nullptr);
		}

		// restore top visible entry
		GadgetListBoxSetTopVisibleEntry(listboxLobbyPlayers, previousTopIndex);
	}

}

void NGMP_WOLLobbyMenu_CreateLobbyCallback(bool bSuccess)
{
	// GeneralsX @bugfix Android port 11/07/2026 unlike
	// NGMP_WOLLobbyMenu_JoinLobbyCallback (which resets this at its very top,
	// success or fail), this callback never reset s_tryingToHostOrJoin --
	// left it stuck TRUE for the rest of the process after ANY host attempt
	// (this callback fires for both success and failure). ExitState() (the
	// Back button handler) bails immediately whenever this flag is true, so
	// hosting a single game -- even successfully -- permanently broke Back
	// for the rest of the session, on every future visit to this screen.
	// Confirmed via read-through; matches the long-reported "Back does
	// nothing" behavior. Also handle the failure case properly instead of
	// always navigating away as if it succeeded (the old // TODO_NGMP
	// comment here).
	fprintf(stderr, "DEBUG-UI: NGMP_WOLLobbyMenu_CreateLobbyCallback bSuccess=%d (clearing s_tryingToHostOrJoin)\n", (int)bSuccess);
	fflush(stderr);
	SetLobbyAttemptHostJoin(FALSE);

	if (!bSuccess)
	{
		GSMessageBoxOk(TheGameText->fetch("GUI:Error"), TheGameText->fetch("GUI:JoinFailedDefault"));
		return;
	}

	buttonPushed = true;
	nextScreen = "Menus/GameSpyGameOptionsMenu.wnd";
	TheShell->pop();
	//TheGameSpyInfo->markAsStagingRoomHost();
	//TheGameSpyInfo->setGameOptions();
}

void NGMP_WOLLobbyMenu_JoinLobbyCallback(EJoinLobbyResult result)
{
	// TODO_NGMP: Show accurate errors again

	SetLobbyAttemptHostJoin(FALSE);
	if (result == EJoinLobbyResult::JoinLobbyResult_Success)
	{
		// Woohoo!  On to our next screen!
		buttonPushed = true;
		nextScreen = "Menus/GameSpyGameOptionsMenu.wnd";
		TheShell->pop();
	}
	else
	{
		UnicodeString s;

		switch (result)
		{
		case EJoinLobbyResult::JoinLobbyResult_FullRoom:        // The room is full.
			s = TheGameText->fetch("GUI:JoinFailedRoomFull");
			break;

        case EJoinLobbyResult::JoinLobbyResult_AnticheatMismatch:
            s = TheGameText->fetchOrSubstitute("GUI:JoinFailedAnticheatMismatch", L"You are running a different anticheat from this lobby host.");
            break;

		// NOTE: Commented out ones are no longer supported. Seems like these we GS concepts but not part of the game
		/*
		case PEERInviteOnlyRoom:  // The room is invite only.
			s = TheGameText->fetch("GUI:JoinFailedInviteOnly");
			break;
		case PEERBannedFromRoom:  // The local user is banned from the room.
			s = TheGameText->fetch("GUI:JoinFailedBannedFromRoom");
			break;
			*/
		case EJoinLobbyResult::JoinLobbyResult_BadPassword:     // An incorrect password (or none) was given for a passworded room.
			s = TheGameText->fetch("GUI:JoinFailedBadPassword");
			break;
		/*
		case PEERAlreadyInRoom:   // The local user is already in or entering a room of the same type.
			s = TheGameText->fetch("GUI:JoinFailedAlreadyInRoom");
			break;
		case PEERNoConnection:    // Can't join a room if there's no chat connection.
			s = TheGameText->fetch("GUI:JoinFailedNoConnection");
			break;
			*/
		default:
			s = TheGameText->fetch("GUI:JoinFailedDefault");
			break;
		}

		GSMessageBoxOk(TheGameText->fetch("GUI:JoinFailedDefault"), s);

		// NGMP: We don't need to do this anymore, the service does it for us
		/*
		if (groupRoomToJoin)
		{
			DEBUG_LOG(("WOLLobbyMenuUpdate() - rejoining group room %d\n", groupRoomToJoin));
			TheGameSpyInfo->joinGroupRoom(groupRoomToJoin);
			groupRoomToJoin = 0;
		}
		else
		{
			DEBUG_LOG(("WOLLobbyMenuUpdate() - joining best group room\n"));
			TheGameSpyInfo->joinBestGroupRoom();
		}
		*/
	}
}

//-------------------------------------------------------------------------------------------------
/** Initialize the WOL Lobby Menu */
//-------------------------------------------------------------------------------------------------
void WOLLobbyMenuInit( WindowLayout *layout, void *userData )
{
	// for safety (and sanity)
	NGMP_OnlineServices_LobbyInterface* pLobbyInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_LobbyInterface>();
	if (pLobbyInterface != nullptr)
	{
		pLobbyInterface->LeaveCurrentLobby();
	}
	
	nextScreen = nullptr;
	buttonPushed = false;
	isShuttingDown = false;
	isFirstRosterUpdate = true;

	SetLobbyAttemptHostJoin(FALSE); // not trying to host or join

	gameListRefreshTime = 0;
	playerListRefreshTime = 0;

	parentWOLLobbyID = TheNameKeyGenerator->nameToKey( "WOLCustomLobby.wnd:WOLLobbyMenuParent" );
	parent = TheWindowManager->winGetWindowFromId(nullptr, parentWOLLobbyID);

	buttonBackID = TheNameKeyGenerator->nameToKey("WOLCustomLobby.wnd:ButtonBack");
	buttonBack = TheWindowManager->winGetWindowFromId(parent, buttonBackID);

	buttonHostID = TheNameKeyGenerator->nameToKey("WOLCustomLobby.wnd:ButtonHost");
	buttonHost = TheWindowManager->winGetWindowFromId(parent, buttonHostID);

	buttonRefreshID = TheNameKeyGenerator->nameToKey("WOLCustomLobby.wnd:ButtonRefresh");
	buttonRefresh = TheWindowManager->winGetWindowFromId(parent, buttonRefreshID);

	buttonJoinID = TheNameKeyGenerator->nameToKey("WOLCustomLobby.wnd:ButtonJoin");
	buttonJoin = TheWindowManager->winGetWindowFromId(parent, buttonJoinID);
	buttonJoin->winEnable(FALSE);

	buttonBuddyID = TheNameKeyGenerator->nameToKey("WOLCustomLobby.wnd:ButtonBuddy");
	buttonBuddy = TheWindowManager->winGetWindowFromId(parent, buttonBuddyID);

	buttonEmoteID = TheNameKeyGenerator->nameToKey("WOLCustomLobby.wnd:ButtonEmote");
	buttonEmote = TheWindowManager->winGetWindowFromId(parent, buttonEmoteID);

	textEntryChatID = TheNameKeyGenerator->nameToKey("WOLCustomLobby.wnd:TextEntryChat");
	textEntryChat = TheWindowManager->winGetWindowFromId(parent, textEntryChatID);

	listboxLobbyPlayersID = TheNameKeyGenerator->nameToKey("WOLCustomLobby.wnd:ListboxPlayers");
	listboxLobbyPlayers = TheWindowManager->winGetWindowFromId(parent, listboxLobbyPlayersID);
	listboxLobbyPlayers->winSetTooltipFunc(playerTooltip);

	listboxLobbyChatID = TheNameKeyGenerator->nameToKey("WOLCustomLobby.wnd:ListboxChat");
	listboxLobbyChat = TheWindowManager->winGetWindowFromId(parent, listboxLobbyChatID);

	comboLobbyGroupRoomsID = TheNameKeyGenerator->nameToKey("WOLCustomLobby.wnd:ComboBoxGroupRooms");
	comboLobbyGroupRooms = TheWindowManager->winGetWindowFromId(parent, comboLobbyGroupRoomsID);

	//GadgetListBoxAddEntryText(listboxLobbyChat, UnicodeString(L"Welcome to Generals Online for Zero Hour!"), GameMakeColor(255, 194, 15, 255), -1, -1);

	GadgetTextEntrySetText(textEntryChat, UnicodeString::TheEmptyString);

	PopulateLobbyFilterComboBox(comboLobbyGroupRooms);

	// Show Menu
	layout->hide( FALSE );

	// if we're not in a room, this will join the best available one
	// TODO_NGMP
	/*
	if (!TheGameSpyInfo->getCurrentGroupRoom())
	{
		if (groupRoomToJoin)
		{
			DEBUG_LOG(("WOLLobbyMenuInit() - rejoining group room %d", groupRoomToJoin));
			TheGameSpyInfo->joinGroupRoom(groupRoomToJoin);
			groupRoomToJoin = 0;
		}
		else
		{
			DEBUG_LOG(("WOLLobbyMenuInit() - joining best group room"));
			TheGameSpyInfo->joinBestGroupRoom();
		}
	}
	else
	{
		DEBUG_LOG(("WOLLobbyMenuInit() - not joining group room because we're already in one"));
	}
	*/

	// NGMP: Register for create lobby callback
	NGMP_OnlineServices_RoomsInterface* pRoomsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_RoomsInterface>();
	if (pLobbyInterface != nullptr && pRoomsInterface != nullptr)
	{
		pLobbyInterface->RegisterForCreateLobbyCallback(NGMP_WOLLobbyMenu_CreateLobbyCallback);

		// NGMP: Join lobby callback
		pLobbyInterface->RegisterForJoinLobbyCallback(NGMP_WOLLobbyMenu_JoinLobbyCallback);

		// NGMP: Request lobbies

		//GadgetListBoxSetItemData(listboxLobbyChat, (void*)-1, index);

		// TODO_NGMP: player list change callbacks

		// register for chat events
		pRoomsInterface->RegisterForChatCallback([](UnicodeString strMessage, Color color)
			{
				GadgetListBoxAddEntryText(listboxLobbyChat, strMessage, color, -1, -1);
			});

		// register for roster events
		pRoomsInterface->RegisterForRosterNeedsRefreshCallback([]()
			{
				if (isFirstRosterUpdate)
				{
					refreshPlayerList(true);
					isFirstRosterUpdate = false;
				}
				else
				{
					refreshPlayerList(false);
				}
			});
	}

	GrabWindowInfo();

	// TODO_NGMP
	//TheGameSpyInfo->clearStagingRoomList();

	// TODO_NGMP
	/*
	PeerRequest req;
	req.peerRequestType = PeerRequest::PEERREQUEST_STARTGAMELIST;
	req.gameList.restrictGameList = TheGameSpyConfig->restrictGamesToLobby();
	TheGameSpyPeerMessageQueue->addRequest(req);
	*/

	// animate controls
//	TheShell->registerWithAnimateManager(parent, WIN_ANIMATION_SLIDE_TOP, TRUE);
	TheShell->showShellMap(TRUE);
#if !defined(GENERALS_ONLINE)
	TheGameSpyGame->reset();
	
#else
	if (TheNGMPGame != nullptr)
	{
		TheNGMPGame->reset();
	}
#endif

	// TODO_NGMP
	//CustomMatchPreferences pref;
//	GameWindow *slider = TheWindowManager->winGetWindowFromId(parent, sliderChatAdjustID);
//	if (slider)
//	{
//		GadgetSliderSetPosition(slider, pref.getChatSizeSlider());
//		doSliderTrack(slider, pref.getChatSizeSlider());
//	}
//

	// TODO_NGMP
	/*
	if (pref.usesLongGameList())
	{
		ToggleGameListType();
	}
	*/

	raiseMessageBoxes = true;

	TheLobbyQueuedUTMs.clear();
	justEntered = TRUE;
	initialGadgetDelay = 2;
	GameWindow *win = TheWindowManager->winGetWindowFromId(nullptr, TheNameKeyGenerator->nameToKey("WOLCustomLobby.wnd:GadgetParent"));
	if(win)
		win->winHide(TRUE);
	DontShowMainMenu = TRUE;


#if defined(GENERALS_ONLINE)
// upon entry, retrieve room list

	NGMP_OnlineServices_RoomsInterface* pRoomsInterfaceOuter = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_RoomsInterface>();
	fprintf(stderr, "DEBUG-JOIN: WOLLobbyMenuInit pRoomsInterfaceOuter=%p, calling GetRoomList\n", (void*)pRoomsInterfaceOuter);
	fflush(stderr);
	if (pRoomsInterfaceOuter != nullptr)
	{
		pRoomsInterfaceOuter->GetRoomList([=]()
			{
				fprintf(stderr, "DEBUG-JOIN: GetRoomList complete, calling JoinRoom(0)\n");
				fflush(stderr);
				// attempt to join the first room
				pRoomsInterfaceOuter->JoinRoom(0, []()
					{
						fprintf(stderr, "DEBUG-JOIN: JoinRoom onStart\n");
						fflush(stderr);
						//GadgetListBoxAddEntryText(listboxLobbyChat, UnicodeString(L"Attempting to join room"), GameMakeColor(255, 194, 15, 255), -1, -1);
					},
					[]()
					{
						fprintf(stderr, "DEBUG-JOIN: JoinRoom onComplete entry\n");
						fflush(stderr);
						GadgetListBoxReset(listboxLobbyChat);

						NGMP_OnlineServices_RoomsInterface* pRoomsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_RoomsInterface>();
						if (pRoomsInterface != nullptr)
						{
							fprintf(stderr, "DEBUG-JOIN: GetGroupRooms().size()=%zu\n", pRoomsInterface->GetGroupRooms().size());
							fflush(stderr);
							// TODO_NGMP: What can we do if empty? kick them out back to the front end?
							if (!pRoomsInterface->GetGroupRooms().empty())
							{
								UnicodeString msg;
								msg.format(TheGameText->fetch("GUI:LobbyJoined"), pRoomsInterface->GetGroupRooms().at(0).GetRoomDisplayName().str());
								GadgetListBoxAddEntryText(listboxLobbyChat, msg, GameSpyColor[GSCOLOR_DEFAULT], -1, -1);

								// process flag related info messages
								ERoomFlags flags = pRoomsInterface->GetGroupRooms().at(0).GetRoomFlags();
								if (flags == ERoomFlags::ROOM_FLAGS_SHOW_ALL_MATCHES)
								{
									//GadgetListBoxAddEntryText(listboxLobbyChat, UnicodeString(L"\t INFO: This is a special room where the lobby list shows lobbies from ALL rooms, not just the current room, to make it easier to find a match without room hopping."), GameMakeColor(255, 194, 15, 255), -1, -1);
									//GadgetListBoxAddEntryText(listboxLobbyChat, UnicodeString(L"\t INFO: Lobbies created here will only show in here. The members list & chat will also only show players in this room."), GameMakeColor(255, 194, 15, 255), -1, -1);
								}
							}
							else
							{
								GadgetListBoxAddEntryText(listboxLobbyChat, UnicodeString(L"\t ERROR: No rooms are available. Try logging in again."), GameMakeColor(255, 0, 0, 255), -1, -1);
							}
						}

						fprintf(stderr, "DEBUG-JOIN: calling refreshPlayerList(TRUE)\n");
						fflush(stderr);
						// refresh on join
						refreshPlayerList(TRUE);
						fprintf(stderr, "DEBUG-JOIN: refreshPlayerList returned, calling refreshGameList(TRUE)\n");
						fflush(stderr);

						refreshGameList(TRUE);
						fprintf(stderr, "DEBUG-JOIN: refreshGameList returned, calling RefreshGameListBoxes()\n");
						fflush(stderr);
						RefreshGameListBoxes();
						fprintf(stderr, "DEBUG-JOIN: RefreshGameListBoxes returned, calling PopulateLobbyFilterComboBox\n");
						fflush(stderr);

						PopulateLobbyFilterComboBox(comboLobbyGroupRooms);
						fprintf(stderr, "DEBUG-JOIN: JoinRoom onComplete exit\n");
						fflush(stderr);
					});
			});
	}

	// Update the communicator button anytime we get notifications
    NGMP_OnlineServices_SocialInterface* pSocialInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_SocialInterface>();
	if (pSocialInterface != nullptr)
	{
		// notifiactions callback
		pSocialInterface->RegisterForCallback_OnNumberGlobalNotificationsChanged([](int numNotifications)
			{
				// update communicator button
				if (buttonBuddy != nullptr)
				{
					UnicodeString buttonText;
                    if (numNotifications > 0)
                    {
                        buttonText.format(L"%s [%d]", TheGameText->fetch("GUI:Buddies").str(), numNotifications);
                    }
                    else
                    {
                        buttonText.format(L"%s", TheGameText->fetch("GUI:Buddies").str());
                    }
					buttonBuddy->winSetText(buttonText);
				}
			});
	}

	// And also initialize it
    if (buttonBuddy != nullptr && pSocialInterface->GetNumTotalNotifications() > 0)
    {
        UnicodeString buttonText;
        buttonText.format(L"%s [%d]", TheGameText->fetch("GUI:Buddies").str(), pSocialInterface->GetNumTotalNotifications());
        buttonBuddy->winSetText(buttonText);
    }
#endif
}

//-------------------------------------------------------------------------------------------------
/** This is called when a shutdown is complete for this menu */
//-------------------------------------------------------------------------------------------------
static void shutdownComplete( WindowLayout *layout )
{

	isShuttingDown = false;

	// hide the layout
	layout->hide( TRUE );

	// our shutdown is complete
	TheShell->shutdownComplete( layout, (nextScreen != nullptr) );

	if (nextScreen != nullptr)
	{
		TheShell->push(nextScreen);
	}

	nextScreen = nullptr;

}

//-------------------------------------------------------------------------------------------------
/** WOL Lobby Menu shutdown method */
//-------------------------------------------------------------------------------------------------
void WOLLobbyMenuShutdown( WindowLayout *layout, void *userData )
{
	NGMP_OnlineServices_LobbyInterface* pLobbyInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_LobbyInterface>();
	if (pLobbyInterface != nullptr)
	{
		pLobbyInterface->DeregisterForCreateLobbyCallback();
		pLobbyInterface->DeregisterForJoinLobbyCallback();
		pLobbyInterface->DeregisterForChatCallback();
		pLobbyInterface->DeregisterForRosterNeedsRefreshCallback();
		pLobbyInterface->DeregisterForSearchForLobbiesCallback();
	}

	CustomMatchPreferences pref;
//	GameWindow *slider = TheWindowManager->winGetWindowFromId(parent, sliderChatAdjustID);
//	if (slider)
//	{
//		pref.setChatSizeSlider(GadgetSliderGetPosition(slider));
//	}
	if (GetGameInfoListBox())
	{
		pref.setUsesLongGameList(FALSE);
	}
	else
	{
		pref.setUsesLongGameList(TRUE);
	}
	pref.write();

	ReleaseWindowInfo();

	// TODO_NGMP
	//TheGameSpyInfo->unregisterTextWindow(listboxLobbyChat);

	//TheGameSpyChat->stopListingGames();
	// TODO_NGMP
	//PeerRequest req;
	//req.peerRequestType = PeerRequest::PEERREQUEST_STOPGAMELIST;
	//TheGameSpyPeerMessageQueue->addRequest(req);

	listboxLobbyChat = nullptr;
	listboxLobbyPlayers = nullptr;

	isShuttingDown = true;

	// if we are shutting down for an immediate pop, skip the animations
	Bool popImmediate = *(Bool *)userData;
	if( popImmediate )
	{

		shutdownComplete( layout );
		return;

	}

	TheShell->reverseAnimatewindow();
	DontShowMainMenu = FALSE;

	RaiseGSMessageBox();
	TheTransitionHandler->reverse("WOLCustomLobbyFade");

}

static void fillPlayerInfo(const PeerResponse *resp, PlayerInfo *info)
{
	info->m_name			= resp->nick.c_str();
	info->m_profileID	= resp->player.profileID;
	info->m_flags			= resp->player.flags;
	info->m_wins			= resp->player.wins;
	info->m_losses		= resp->player.losses;
	info->m_locale		= resp->locale.c_str();
	info->m_rankPoints= resp->player.rankPoints;
	info->m_side			= resp->player.side;
	info->m_preorder	= resp->player.preorder;
}

#ifdef PERF_TEST
static const char* getMessageString(Int t)
{
	switch(t)
	{
		case PeerResponse::PEERRESPONSE_LOGIN:
			return "login";
		case PeerResponse::PEERRESPONSE_DISCONNECT:
			return "disconnect";
		case PeerResponse::PEERRESPONSE_MESSAGE:
			return "message";
		case PeerResponse::PEERRESPONSE_GROUPROOM:
			return "group room";
		case PeerResponse::PEERRESPONSE_STAGINGROOM:
			return "staging room";
		case PeerResponse::PEERRESPONSE_STAGINGROOMPLAYERINFO:
			return "staging room player info";
		case PeerResponse::PEERRESPONSE_JOINGROUPROOM:
			return "group room join";
		case PeerResponse::PEERRESPONSE_CREATESTAGINGROOM:
			return "staging room create";
		case PeerResponse::PEERRESPONSE_JOINSTAGINGROOM:
			return "staging room join";
		case PeerResponse::PEERRESPONSE_PLAYERJOIN:
			return "player join";
		case PeerResponse::PEERRESPONSE_PLAYERLEFT:
			return "player part";
		case PeerResponse::PEERRESPONSE_PLAYERCHANGEDNICK:
			return "player nick";
		case PeerResponse::PEERRESPONSE_PLAYERINFO:
			return "player info";
		case PeerResponse::PEERRESPONSE_PLAYERCHANGEDFLAGS:
			return "player flags";
		case PeerResponse::PEERRESPONSE_ROOMUTM:
			return "room UTM";
		case PeerResponse::PEERRESPONSE_PLAYERUTM:
			return "player UTM";
		case PeerResponse::PEERRESPONSE_QUICKMATCHSTATUS:
			return "QM status";
		case PeerResponse::PEERRESPONSE_GAMESTART:
			return "game start";
		case PeerResponse::PEERRESPONSE_FAILEDTOHOST:
			return "host failure";
	}
	return "unknown";
}
#endif // PERF_TEST

//-------------------------------------------------------------------------------------------------
/** refreshGameList
		The Bool is used to force refresh if the refresh button was hit.*/
//-------------------------------------------------------------------------------------------------
void refreshGameList( Bool forceRefresh )
{
	// TODO_NGMP: rate limit this like before
	//RefreshGameListBoxes();

	Int refreshInterval = gameListRefreshInterval;

	if (forceRefresh || ((gameListRefreshTime == 0) || ((gameListRefreshTime + refreshInterval) <= timeGetTime())))
	{
#if defined(GENERALS_ONLINE)
		RefreshGameListBoxes();
		gameListRefreshTime = timeGetTime();
#else
		if (TheGameSpyInfo->hasStagingRoomListChanged())
		{
			//DEBUG_LOG(("################### refreshing game list"));
			//DEBUG_LOG(("gameRefreshTime=%d, refreshInterval=%d, now=%d", gameListRefreshTime, refreshInterval, timeGetTime()));
			RefreshGameListBoxes();
			gameListRefreshTime = timeGetTime();
		} else {
			//DEBUG_LOG(("-"));
		}
#endif
	} else {
		//DEBUG_LOG(("gameListRefreshTime: %d refreshInterval: %d", gameListRefreshTime, refreshInterval));
	}
}
//-------------------------------------------------------------------------------------------------
/** refreshPlayerList
		The Bool is used to force refresh if the refresh button was hit.*/
//-------------------------------------------------------------------------------------------------
void refreshPlayerList( Bool forceRefresh )
{
		Int refreshInterval = playerListRefreshInterval;

		if (forceRefresh ||((playerListRefreshTime == 0) || ((playerListRefreshTime + refreshInterval) <= timeGetTime())))
		{
				PopulateLobbyPlayerListbox();
				playerListRefreshTime = timeGetTime();
		}
}

void ExitState()
{
	fprintf(stderr, "DEBUG-UI: ExitState() enter -- s_tryingToHostOrJoin=%d buttonPushed=%d\n", (int)s_tryingToHostOrJoin, (int)buttonPushed);
	fflush(stderr);
	if (s_tryingToHostOrJoin)
	{
		fprintf(stderr, "DEBUG-UI: ExitState() bail -- s_tryingToHostOrJoin is TRUE, Back does nothing this press\n");
		fflush(stderr);
		return;
	}

	// Leave any group room, then pop off the screen
	auto pOnlineServicesManager = NGMP_OnlineServicesManager::GetInstance();
	if (pOnlineServicesManager != nullptr)
	{
		NGMP_OnlineServices_RoomsInterface* pRoomsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_RoomsInterface>();
		if (pRoomsInterface != nullptr)
		{
			pRoomsInterface->LeaveRoom();
		}
	}

	SetLobbyAttemptHostJoin(TRUE); // pretend, since we don't want to queue up another action
	buttonPushed = true;

	if (pOnlineServicesManager == nullptr || pOnlineServicesManager->IsPendingFullTeardown()) // go back to the front end
	{
		nextScreen = nullptr;
	}
	else // user backed out, go back to welcome menu
	{
		nextScreen = "Menus/WOLWelcomeMenu.wnd";
	}

	fprintf(stderr, "DEBUG-UI: ExitState() popping shell, nextScreen=%s\n", nextScreen ? nextScreen : "(null)");
	fflush(stderr);
	TheShell->pop();
}

//-------------------------------------------------------------------------------------------------
/** WOL Lobby Menu update method */
//-------------------------------------------------------------------------------------------------
void WOLLobbyMenuUpdate( WindowLayout * layout, void *userData)
{
	// need to exit?
	if (NGMP_OnlineServicesManager::GetInstance() != nullptr && NGMP_OnlineServicesManager::GetInstance()->IsPendingFullTeardown())
	{
		if (!s_tryingToHostOrJoin)
		{
			s_tryingToHostOrJoin = false;
			ExitState();
			TearDownGeneralsOnline();
		}		

		return;
	}

	if(justEntered)
	{
		if(initialGadgetDelay == 1)
		{
			TheTransitionHandler->remove("MainMenuDefaultMenuLogoFade");
			TheTransitionHandler->setGroup("WOLCustomLobbyFade");
			TheWindowManager->winSetFocus(textEntryChat);
			initialGadgetDelay = 2;
			justEntered = FALSE;
		}
		else
			initialGadgetDelay--;
	}
	if (TheGameLogic->isInShellGame() && TheGameLogic->getFrame() == 1)
	{
		SignalUIInteraction(SHELL_SCRIPT_HOOK_GENERALS_ONLINE_ENTERED_FROM_GAME);
	}


	// We'll only be successful if we've requested to
	if(isShuttingDown && TheShell->isAnimFinished() && TheTransitionHandler->isFinished())
		shutdownComplete(layout);

	if (raiseMessageBoxes)
	{
		RaiseGSMessageBox();
		raiseMessageBoxes = false;
	}
	
	// do we need to update?
	NGMP_OnlineServices_LobbyInterface* pLobbyInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_LobbyInterface>();
	if (pLobbyInterface != nullptr && pLobbyInterface->IsLobbyListDirty() && !isShuttingDown && !buttonPushed && !pLobbyInterface->IsInLobby() && pLobbyInterface->GetLobbyTryingToJoin().lobbyID == -1)
	{
		const bool bShouldAutoRefresh = true;

		pLobbyInterface->ConsumeLobbyListDirtyFlag();

		if (bShouldAutoRefresh)
		{
			refreshGameList(false);
		}
		else
		{
			GadgetListBoxAddEntryText(listboxLobbyChat, UnicodeString(L"Your lobby list is outdated. Hit refresh to see the latest servers."), GameMakeColor(255, 194, 15, 255), -1, -1);
		}

	}

#if defined(GENERALS_ONLINE) // GO needs to tick this, so notifications disappear etc
	HandleBuddyResponses();
#endif

	if (TheShell->isAnimFinished() && TheTransitionHandler->isFinished() && !buttonPushed && TheGameSpyPeerMessageQueue)
	{
		HandleBuddyResponses();
		HandlePersistentStorageResponses();

#ifdef PERF_TEST
		UnsignedInt start = timeGetTime();
		UnsignedInt end = timeGetTime();
		std::list<Int> responses;
		Int numMessages = 0;
#endif // PERF_TEST

		Int allowedMessages = TheGameSpyInfo->getMaxMessagesPerUpdate();
		Bool sawImportantMessage = FALSE;
		Bool shouldRepopulatePlayers = FALSE;
		PeerResponse resp;
		while (allowedMessages-- && !sawImportantMessage && TheGameSpyPeerMessageQueue->getResponse( resp ))
		{
#ifdef PERF_TEST
			++numMessages;
			responses.push_back(resp.peerResponseType);
#endif // PERF_TEST
			switch (resp.peerResponseType)
			{
			case PeerResponse::PEERRESPONSE_JOINGROUPROOM:
				sawImportantMessage = TRUE;
				if (resp.joinGroupRoom.ok)
				{
					//buttonPushed = true;
					TheGameSpyInfo->setCurrentGroupRoom(resp.joinGroupRoom.id);
					TheGameSpyInfo->getPlayerInfoMap()->clear();
					GroupRoomMap::iterator iter = TheGameSpyInfo->getGroupRoomList()->find(resp.joinGroupRoom.id);
					if (iter != TheGameSpyInfo->getGroupRoomList()->end())
					{
						GameSpyGroupRoom room = iter->second;
						UnicodeString msg;
						msg.format(TheGameText->fetch("GUI:LobbyJoined"), room.m_translatedName.str());
						TheGameSpyInfo->addText(msg, GameSpyColor[GSCOLOR_DEFAULT], nullptr);
					}
				}
				else
				{
					DEBUG_LOG(("WOLLobbyMenuUpdate() - joining best group room"));
					TheGameSpyInfo->joinBestGroupRoom();
				}
				PopulateLobbyFilterComboBox(comboLobbyGroupRooms);
				shouldRepopulatePlayers = TRUE;
				break;
			case PeerResponse::PEERRESPONSE_PLAYERCHANGEDFLAGS:
				{
					PlayerInfo p;
					fillPlayerInfo(&resp, &p);
					TheGameSpyInfo->updatePlayerInfo(p);
					shouldRepopulatePlayers = TRUE;
				}
				break;
			case PeerResponse::PEERRESPONSE_PLAYERCHANGEDNICK:
				{
					PlayerInfo p;
					fillPlayerInfo(&resp, &p);
					TheGameSpyInfo->updatePlayerInfo(p);
					shouldRepopulatePlayers = TRUE;
				}
				break;
			case PeerResponse::PEERRESPONSE_PLAYERINFO:
				{
					PlayerInfo p;
					fillPlayerInfo(&resp, &p);
					TheGameSpyInfo->updatePlayerInfo(p);
					shouldRepopulatePlayers = TRUE;
				}
				break;
			case PeerResponse::PEERRESPONSE_PLAYERJOIN:
				{
					if (resp.player.roomType == GroupRoom)
					{
						PlayerInfo p;
						fillPlayerInfo(&resp, &p);
						TheGameSpyInfo->updatePlayerInfo(p);
						shouldRepopulatePlayers = TRUE;
					}
				}
				break;
			case PeerResponse::PEERRESPONSE_PLAYERUTM:
			case PeerResponse::PEERRESPONSE_ROOMUTM:
				{
					DEBUG_LOG(("Putting off a UTM in the lobby"));
					TheLobbyQueuedUTMs.push_back(resp);
				}
				break;
			case PeerResponse::PEERRESPONSE_PLAYERLEFT:
				{
					PlayerInfo p;
					fillPlayerInfo(&resp, &p);
					TheGameSpyInfo->playerLeftGroupRoom(resp.nick.c_str());
					shouldRepopulatePlayers = TRUE;
				}
				break;
			case PeerResponse::PEERRESPONSE_MESSAGE:
				{
					TheGameSpyInfo->addChat(resp.nick.c_str(), resp.message.profileID,
						UnicodeString(resp.text.c_str()), !resp.message.isPrivate, resp.message.isAction, listboxLobbyChat);
				}
				break;
			case PeerResponse::PEERRESPONSE_DISCONNECT:
				{
					sawImportantMessage = TRUE;
					UnicodeString title, body;
					AsciiString disconMunkee;
					disconMunkee.format("GUI:GSDisconReason%d", resp.discon.reason);
					title = TheGameText->fetch( "GUI:GSErrorTitle" );
					body = TheGameText->fetch( disconMunkee );
					GameSpyCloseAllOverlays();
					GSMessageBoxOk( title, body );
					TheGameSpyInfo->reset();
					TheShell->pop();
				}
				break;
			case PeerResponse::PEERRESPONSE_CREATESTAGINGROOM:
				{
					sawImportantMessage = TRUE;
					SetLobbyAttemptHostJoin(FALSE);
					if (resp.createStagingRoom.result == PEERJoinSuccess)
					{
						// Woohoo!  On to our next screen!
						buttonPushed = true;
						nextScreen = "Menus/GameSpyGameOptionsMenu.wnd";
						TheShell->pop();
						TheGameSpyInfo->markAsStagingRoomHost();
						TheGameSpyInfo->setGameOptions();
					}
				}
				break;
			case PeerResponse::PEERRESPONSE_JOINSTAGINGROOM:
				{
					sawImportantMessage = TRUE;
					SetLobbyAttemptHostJoin(FALSE);
					Bool isHostPresent = TRUE;
					if (resp.joinStagingRoom.ok == PEERTrue)
					{
						GameSpyStagingRoom *room = TheGameSpyInfo->getCurrentStagingRoom();
						if (!room)
						{
							isHostPresent = FALSE;
						}
						else
						{
							isHostPresent = FALSE;
							for (Int i=0; i<MAX_SLOTS; ++i)
							{
								AsciiString hostName;
								hostName.translate(room->getConstSlot(0)->getName());
								const char *firstPlayer = resp.stagingRoomPlayerNames[i].c_str();
								if (strcmp(hostName.str(), firstPlayer) == 0)
								{
									DEBUG_LOG(("Saw host %s == %s in slot %d", hostName.str(), firstPlayer, i));
									isHostPresent = TRUE;
								}
							}
						}
					}
					if (resp.joinStagingRoom.ok == PEERTrue && isHostPresent)
					{
						// Woohoo!  On to our next screen!
						buttonPushed = true;
						nextScreen = "Menus/GameSpyGameOptionsMenu.wnd";
						TheShell->pop();
					}
					else
					{
						UnicodeString s;

						switch(resp.joinStagingRoom.result)
						{
						case PEERFullRoom:        // The room is full.
							s = TheGameText->fetch("GUI:JoinFailedRoomFull");
							break;
						case PEERInviteOnlyRoom:  // The room is invite only.
							s = TheGameText->fetch("GUI:JoinFailedInviteOnly");
							break;
						case PEERBannedFromRoom:  // The local user is banned from the room.
							s = TheGameText->fetch("GUI:JoinFailedBannedFromRoom");
							break;
						case PEERBadPassword:     // An incorrect password (or none) was given for a passworded room.
							s = TheGameText->fetch("GUI:JoinFailedBadPassword");
							break;
						case PEERAlreadyInRoom:   // The local user is already in or entering a room of the same type.
							s = TheGameText->fetch("GUI:JoinFailedAlreadyInRoom");
							break;
						case PEERNoConnection:    // Can't join a room if there's no chat connection.
							s = TheGameText->fetch("GUI:JoinFailedNoConnection");
							break;
						default:
							s = TheGameText->fetch("GUI:JoinFailedDefault");
							break;
						}
						GSMessageBoxOk(TheGameText->fetch("GUI:JoinFailedDefault"), s);
						if (groupRoomToJoin)
						{
							DEBUG_LOG(("WOLLobbyMenuUpdate() - rejoining group room %d", groupRoomToJoin));
							TheGameSpyInfo->joinGroupRoom(groupRoomToJoin);
							groupRoomToJoin = 0;
						}
						else
						{
							DEBUG_LOG(("WOLLobbyMenuUpdate() - joining best group room"));
							TheGameSpyInfo->joinBestGroupRoom();
						}
					}
				}
				break;
			case PeerResponse::PEERRESPONSE_STAGINGROOMLISTCOMPLETE:
				TheGameSpyInfo->sawFullGameList();
				break;
			case PeerResponse::PEERRESPONSE_STAGINGROOM:
				{
					GameSpyStagingRoom room;
					switch(resp.stagingRoom.action)
					{
					case PEER_CLEAR:
						TheGameSpyInfo->clearStagingRoomList();
						//TheGameSpyInfo->addText( L"gameList: PEER_CLEAR", GameSpyColor[GSCOLOR_DEFAULT], listboxLobbyChat );
						break;
					case PEER_ADD:
					case PEER_UPDATE:
					{
						if (resp.stagingRoom.percentComplete == 100)
						{
							TheGameSpyInfo->sawFullGameList();
						}

						//if (ParseAsciiStringToGameInfo(&room, resp.stagingRoomMapName.c_str()))
						//if (ParseAsciiStringToGameInfo(&room, resp.stagingServerGameOptions.c_str()))
						Bool serverOk = TRUE;
						if (resp.stagingRoomMapName.empty())
						{
							serverOk = FALSE;
						}
						// fix for ghost game problem - need to iterate over all resp.stagingRoomPlayerNames[i]
						Bool sawSelf = FALSE;
						//for (Int i=0; i<MAX_SLOTS; ++i)
						//{
							if (TheGameSpyInfo->getLocalName() == resp.stagingRoomPlayerNames[0].c_str())
							{
								sawSelf = TRUE; // don't show ghost games for myself
							}
						//}
						if (sawSelf)
							serverOk = FALSE;

						if (serverOk)
						{
							room.setGameName(UnicodeString(resp.stagingServerName.c_str()));
							room.setID(resp.stagingRoom.id);
							room.setHasPassword(resp.stagingRoom.requiresPassword);
							room.setVersion(resp.stagingRoom.version);
							room.setExeCRC(resp.stagingRoom.exeCRC);
							room.setIniCRC(resp.stagingRoom.iniCRC);
							room.setAllowObservers(resp.stagingRoom.allowObservers);
              room.setUseStats(resp.stagingRoom.useStats);
							room.setPingString(resp.stagingServerPingString.c_str());
							room.setLadderIP(resp.stagingServerLadderIP.c_str());
							room.setLadderPort(resp.stagingRoom.ladderPort);
							room.setReportedNumPlayers(resp.stagingRoom.numPlayers);
							room.setReportedMaxPlayers(resp.stagingRoom.maxPlayers);
							room.setReportedNumObservers(resp.stagingRoom.numObservers);

							Int i;
							AsciiString gsMapName = resp.stagingRoomMapName.c_str();
							AsciiString mapName = "";
							for (i=0; i<gsMapName.getLength(); ++i)
							{
								char c = gsMapName.getCharAt(i);
								if (c != '/')
									mapName.concat(c);
								else
									mapName.concat('\\');
							}
							room.setMap(TheGameState->portableMapPathToRealMapPath(mapName));

							Int numPlayers = 0;
							for (i=0; i<MAX_SLOTS; ++i)
							{
								GameSpyGameSlot *slot = room.getGameSpySlot(i);
								if (slot)
								{
									slot->setWins( resp.stagingRoom.wins[i] );
									slot->setLosses( resp.stagingRoom.losses[i] );
									slot->setProfileID( resp.stagingRoom.profileID[i] );
									slot->setPlayerTemplate( resp.stagingRoom.faction[i] );
									slot->setColor( resp.stagingRoom.color[i] );
									if (resp.stagingRoom.profileID[i] == SLOT_EASY_AI)
									{
										slot->setState(SLOT_EASY_AI);
										++numPlayers;
									}
									else if (resp.stagingRoom.profileID[i] == SLOT_MED_AI)
									{
										slot->setState(SLOT_MED_AI);
										++numPlayers;
									}
									else if (resp.stagingRoom.profileID[i] == SLOT_BRUTAL_AI)
									{
										slot->setState(SLOT_BRUTAL_AI);
										++numPlayers;
									}
									else if (!resp.stagingRoomPlayerNames[i].empty())
									{
										UnicodeString nameUStr;
										nameUStr.translate(resp.stagingRoomPlayerNames[i].c_str());
										slot->setState(SLOT_PLAYER, nameUStr);
										++numPlayers;
									}
									else
									{
										slot->setState(SLOT_OPEN);
									}
								}
							}
							DEBUG_ASSERTCRASH(numPlayers, ("Game had no players!"));
							//DEBUG_LOG(("Saw room: hasPass=%d, allowsObservers=%d", room.getHasPassword(), room.getAllowObservers()));
							if (resp.stagingRoom.action == PEER_ADD)
							{
								TheGameSpyInfo->addStagingRoom(room);
								//TheGameSpyInfo->addText( L"gameList: PEER_ADD", GameSpyColor[GSCOLOR_DEFAULT], listboxLobbyChat );
							}
							else
							{
								TheGameSpyInfo->updateStagingRoom(room);
								//TheGameSpyInfo->addText( L"gameList: PEER_UPDATE", GameSpyColor[GSCOLOR_DEFAULT], listboxLobbyChat );
							}
						}
						else
						{
							room.setID(resp.stagingRoom.id);
							TheGameSpyInfo->removeStagingRoom(room);
							//TheGameSpyInfo->addText( L"gameList: PEER_UPDATE FAILED", GameSpyColor[GSCOLOR_DEFAULT], listboxLobbyChat );
						}
						break;
					}
					case PEER_REMOVE:
						room.setID(resp.stagingRoom.id);
						TheGameSpyInfo->removeStagingRoom(room);
						//TheGameSpyInfo->addText( L"gameList: PEER_REMOVE", GameSpyColor[GSCOLOR_DEFAULT], listboxLobbyChat );
						break;
					default:
						//TheGameSpyInfo->addText( L"gameList: Unknown", GameSpyColor[GSCOLOR_DEFAULT], listboxLobbyChat );
						break;
					}
				}
				break;
			}
		}
#if 0
		if (shouldRepopulatePlayers)
		{
			PopulateLobbyPlayerListbox();
		}
#else
		refreshPlayerList();
#endif

#ifdef PERF_TEST
		// check performance
		end = timeGetTime();
		PERF_LOG(("Frame time was %d ms", end-start));
		std::list<Int>::const_iterator it;
		for (it = responses.begin(); it != responses.end(); ++it)
		{
			PERF_LOG(("  %s", getMessageString(*it)));
		}
		PERF_LOG((""));
#endif // PERF_TEST

#if 0
// Removed 2-17-03 to pull out into a function so we can do the same checks
		Int refreshInterval = gameListRefreshInterval;

		if ((gameListRefreshTime == 0) || ((gameListRefreshTime + refreshInterval) <= timeGetTime()))
		{
			if (TheGameSpyInfo->hasStagingRoomListChanged())
			{
				//DEBUG_LOG(("################### refreshing game list"));
				//DEBUG_LOG(("gameRefreshTime=%d, refreshInterval=%d, now=%d", gameListRefreshTime, refreshInterval, timeGetTime()));
				RefreshGameListBoxes();
				gameListRefreshTime = timeGetTime();
			} else {
				//DEBUG_LOG(("-"));
			}
		} else {
			//DEBUG_LOG(("gameListRefreshTime: %d refreshInterval: %d", gameListRefreshTime, refreshInterval));
		}
#else
	refreshGameList();
#endif
	}
}

//-------------------------------------------------------------------------------------------------
/** WOL Lobby Menu input callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType WOLLobbyMenuInput( GameWindow *window, UnsignedInt msg,
																			 WindowMsgData mData1, WindowMsgData mData2 )
{
	switch( msg )
	{

		// --------------------------------------------------------------------------------------------
		case GWM_CHAR:
		{
			UnsignedByte key = mData1;
			UnsignedByte state = mData2;
			if (buttonPushed)
				break;

			switch( key )
			{

				// ----------------------------------------------------------------------------------------
				case KEY_ESC:
				{
					// GeneralsX @bugfix Android port 11/07/2026 - The physical/gesture Back key on
					// Android routes here as KEY_ESC. Each GameSpyOpenOverlay() popup (PopupPlayerInfo,
					// Communicator/Buddy, Options, ...) has its own KEY_ESC handler that requests input
					// focus and closes itself. If one of those overlays is open on top of this screen,
					// don't ALSO simulate this screen's own Back click here -- popping/tearing down the
					// whole lobby screen while a modal overlay is still alive on top of it is a plausible
					// crash source (observed: real-device crash with an unresolvable PC right after
					// pressing Back while a PopupPlayerInfo overlay was open). This is defense-in-depth:
					// if overlay input focus is working correctly this branch is simply never taken.
					Bool anyOverlayOpen = FALSE;
					for (Int i = 0; i < GSOVERLAY_MAX; ++i)
					{
						if (GameSpyIsOverlayOpen((GSOverlayType)i))
						{
							anyOverlayOpen = TRUE;
							break;
						}
					}

					fprintf(stderr, "DEBUG-UI: WOLLobbyMenuInput KEY_ESC state=%d anyOverlayOpen=%d\n",
						(int)state, (int)anyOverlayOpen);
					fflush(stderr);

					if (anyOverlayOpen)
					{
						// let the overlay's own input handler deal with it instead
						return MSG_HANDLED;
					}

					//
					// send a simulated selected event to the parent window of the
					// back/exit button
					//
					if( BitIsSet( state, KEY_STATE_UP ) )
					{
						TheWindowManager->winSendSystemMsg( window, GBM_SELECTED,
																							(WindowMsgData)buttonBack, buttonBackID );

					}

					// don't let key fall through anywhere else
					return MSG_HANDLED;

				}

			}

		}

	}

	return MSG_IGNORED;
}

//static void doSliderTrack(GameWindow *control, Int val)
//{
//	Int sliderW, sliderH, sliderX, sliderY;
//	control->winGetPosition(&sliderX, &sliderY);
//	control->winGetSize(&sliderW, &sliderH);
//	Real cursorY = sliderY + (100-val)*0.01f*sliderH;
//
//	extern GameWindow *listboxLobbyGamesSmall;
//	extern GameWindow *listboxLobbyGamesLarge;
//	extern GameWindow *listboxLobbyGameInfo;
//
//	static Int gwsX = 0, gwsY = 0, gwsW = 0, gwsH = 0;
//	static Int gwlX = 0, gwlY = 0, gwlW = 0, gwlH = 0;
//	static Int gwiX = 0, gwiY = 0, gwiW = 0, gwiH = 0;
//	static Int pwX = 0, pwY = 0, pwW = 0, pwH = 0;
//	static Int chatPosX = 0, chatPosY = 0, chatW = 0, chatH = 0;
//	static Int spacing = 0;
//	if (chatPosX == 0)
//	{
//		listboxLobbyChat->winGetPosition(&chatPosX, &chatPosY);
//		listboxLobbyChat->winGetSize(&chatW, &chatH);
//
////		listboxLobbyGamesSmall->winGetPosition(&gwsX, &gwsY);
////		listboxLobbyGamesSmall->winGetSize(&gwsW, &gwsH);
//
//		listboxLobbyGamesLarge->winGetPosition(&gwlX, &gwlY);
//		listboxLobbyGamesLarge->winGetSize(&gwlW, &gwlH);
//
////		listboxLobbyGameInfo->winGetPosition(&gwiX, &gwiY);
////		listboxLobbyGameInfo->winGetSize(&gwiW, &gwiH);
////
//		listboxLobbyPlayers->winGetPosition(&pwX, &pwY);
//		listboxLobbyPlayers->winGetSize(&pwW, &pwH);
//
//		spacing = chatPosY - pwY - pwH;
//	}
//
//	Int newChatY = cursorY;
//	Int newChatH = chatH + chatPosY - newChatY;
//	listboxLobbyChat->winSetPosition(chatPosX, newChatY);
//	listboxLobbyChat->winSetSize(chatW, newChatH);
//
//	Int newH = cursorY - pwY - spacing;
//	listboxLobbyPlayers->winSetSize(pwW, newH);
////	listboxLobbyGamesSmall->winSetSize(gwsW, newH);
//	listboxLobbyGamesLarge->winSetSize(gwlW, newH);
////	listboxLobbyGameInfo->winSetSize(gwiW, newH);


//-------------------------------------------------------------------------------------------------
/** WOL Lobby Menu window system callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType WOLLobbyMenuSystem( GameWindow *window, UnsignedInt msg,
														 WindowMsgData mData1, WindowMsgData mData2 )
{
	UnicodeString txtInput;
	static NameKeyType buttonGameListTypeToggleID = NAMEKEY_INVALID;

	switch( msg )
	{


		//---------------------------------------------------------------------------------------------
		case GWM_CREATE:
			{
				fprintf(stderr, "DEBUG-UI: WOLLobbyMenuSystem GWM_CREATE\n");
				fflush(stderr);
				buttonGameListTypeToggleID = NAMEKEY("WOLCustomLobby.wnd:ButtonGameListToggle");
//				sliderChatAdjustID = NAMEKEY("WOLCustomLobby.wnd:SliderChatAdjust");

				break;
			}

		//---------------------------------------------------------------------------------------------
		case GWM_DESTROY:
			{
				fprintf(stderr, "DEBUG-UI: WOLLobbyMenuSystem GWM_DESTROY\n");
				fflush(stderr);
				break;
			}

		//---------------------------------------------------------------------------------------------
		case GWM_INPUT_FOCUS:
			{
				// if we're givin the opportunity to take the keyboard focus we must say we want it
				if( mData1 == TRUE )
					*(Bool *)mData2 = TRUE;

				return MSG_HANDLED;
			}

		//---------------------------------------------------------------------------------------------
		case GLM_SELECTED:
			{
				GameWindow *control = (GameWindow *)mData1;
				Int controlID = control->winGetWindowId();
				fprintf(stderr, "DEBUG-UI: WOLLobbyMenuSystem GLM_SELECTED control='%s'\n", KEYNAME((NameKeyType)controlID).str());
				fflush(stderr);
				if ( controlID == GetGameListBoxID() )
				{
					int rowSelected = mData2;
					if( rowSelected >= 0 )
					{
						buttonJoin->winEnable(TRUE);
						static UnsignedInt lastFrame = 0;
						static Int lastID = -1;
						UnsignedInt now = TheGameClient->getFrame();

						PeerRequest req;
						req.peerRequestType = PeerRequest::PEERREQUEST_GETEXTENDEDSTAGINGROOMINFO;
						// GeneralsX @bugfix Android port 07/11/2026 - Cast via uintptr_t for 64-bit
						req.stagingRoom.id = static_cast<Int>(reinterpret_cast<uintptr_t>(GadgetListBoxGetItemData(control, rowSelected, 0)));

						if (lastID != req.stagingRoom.id || now > lastFrame + 60)
						{
							// TODO_NGMP: Impl this again
							/*
							TheGameSpyPeerMessageQueue->addRequest(req);
							*/
						}
						
						lastID = req.stagingRoom.id;
						lastFrame = now;
					}
					else
					{
						buttonJoin->winEnable(FALSE);
					}
					if (GetGameInfoListBox())
					{
						RefreshGameInfoListBox(GetGameListBox(), GetGameInfoListBox());
					}
				}

				break;
			}

		//---------------------------------------------------------------------------------------------
		case GBM_SELECTED:
			{
				// GeneralsX @bugfix Android port 11/07/2026 log every button
				// press in this screen (Custom Match) BEFORE the buttonPushed
				// early-out, specifically so a stuck buttonPushed flag (which
				// would silently eat every click, same failure shape as the
				// s_tryingToHostOrJoin bug fixed nearby) shows up in the log
				// as "press seen, buttonPushed=1, ignored" instead of nothing
				// at all.
				{
					GameWindow *controlForLog = (GameWindow *)mData1;
					fprintf(stderr, "DEBUG-UI: WOLLobbyMenuSystem GBM_SELECTED control='%s' buttonPushed=%d\n",
						KEYNAME((NameKeyType)controlForLog->winGetWindowId()).str(), (int)buttonPushed);
					fflush(stderr);
				}

				if (buttonPushed)
					break;

				GameWindow *control = (GameWindow *)mData1;
				Int controlID = control->winGetWindowId();

				if (HandleSortButton((NameKeyType)controlID))
					break;

				// If we back out, just bail - we haven't gotten far enough to need to log out
				if ( controlID == buttonBackID )
				{
					ExitState();

				}
				else if ( controlID == buttonRefreshID )
				{
					// Added 2/17/03 added the game refresh button
					refreshGameList(TRUE);
					refreshPlayerList(TRUE);
				}
				else if ( controlID == buttonHostID )
				{
					if (s_tryingToHostOrJoin)
						break;

					SetLobbyAttemptHostJoin( TRUE );
					TheLobbyQueuedUTMs.clear();
					// TODO_NGMP
					//groupRoomToJoin = TheGameSpyInfo->getCurrentGroupRoom();
					GameSpyOpenOverlay(GSOVERLAY_GAMEOPTIONS);
				}
				else if ( controlID == buttonJoinID )
				{
					NGMP_OnlineServices_LobbyInterface* pLobbyInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_LobbyInterface>();
					if (pLobbyInterface == nullptr)
					{
						break;
					}

					// TODO_NGMP: Support re-ordering again
					Int selected;
					GadgetListBoxGetSelected(GetGameListBox(), &selected);
					if (selected >= 0)
					{
						// GeneralsX @bugfix Android port 07/11/2026 - Cast via uintptr_t for 64-bit
						Int selectedID = static_cast<Int>(reinterpret_cast<uintptr_t>(GadgetListBoxGetItemData(GetGameListBox(), selected)));
						if (selectedID >= 0)
						{
							auto Lobby = pLobbyInterface->GetLobbyFromID(selectedID);

							if (Lobby.lobbyID == -1) // -1 is invalid
							{
								break;
							}

							// GeneralsX @bugfix Android port 13/07/2026 - Log this synchronously, before
							// any network round-trip, so the comparison is captured even if the user
							// backs out or the app closes before an async JoinLobby response would land.
							fprintf(stderr, "DEBUG-JOIN: CRC check for lobby %lld '%s': host exe_crc=%u ini_crc=%u, local exe_crc=%u ini_crc=%u (VANILLA_INI_CRC=%u)\n",
								(long long)Lobby.lobbyID, Lobby.name.c_str(), Lobby.exe_crc, Lobby.ini_crc, TheGlobalData->m_exeCRC, TheGlobalData->m_iniCRC, (unsigned)VANILLA_INI_CRC);
							fflush(stderr);

							// CRC Check
							if (Lobby.exe_crc != TheGlobalData->m_exeCRC || Lobby.ini_crc != TheGlobalData->m_iniCRC)
							{
								if (TheGlobalData->m_iniCRC != VANILLA_INI_CRC)
								{
									GSMessageBoxOk(TheGameText->fetch("GUI:JoinFailedDefault"), UnicodeString(L"You have modified INI files or a modification."));
								}
								else if (Lobby.ini_crc != VANILLA_INI_CRC)
								{
									GSMessageBoxOk(TheGameText->fetch("GUI:JoinFailedDefault"), UnicodeString(L"The host has modified INI files or a modification."));
								}
								else
								{
									GSMessageBoxOk(TheGameText->fetch("GUI:JoinFailedDefault"), TheGameText->fetch("GUI:JoinFailedCRCMismatch"));
								}
								break;
							}

							// TODO_NGMP: Enforce this on the host too, vanilla game did not...

							
							pLobbyInterface->SetLobbyTryingToJoin(Lobby);

							if (Lobby.passworded)
							{
								GameSpyOpenOverlay(GSOVERLAY_GAMEPASSWORD);
							}
							else
							{
								pLobbyInterface->JoinLobby(Lobby, std::string());

								SetLobbyAttemptHostJoin(TRUE);
							}
						}
					}
					else
					{
						// GeneralsX @bugfix Android port 13/07/2026 - Distinguish "tapped Join with
						// no row selected" from a real CRC/network rejection in the log, since on
						// touch the listbox selection may not register the same way it does on PC.
						fprintf(stderr, "DEBUG-JOIN: ButtonJoin pressed but no row selected (GadgetListBoxGetSelected returned <0)\n");
						fflush(stderr);
						GSMessageBoxOk(TheGameText->fetch("GUI:Error"), TheGameText->fetch("GUI:NoGameSelected"), NULL);
					}
					// TODO_NGMP: Start using StagingRoomInfo again, it'll make this easier and cleaner
					/*
					if (s_tryingToHostOrJoin)
						break;

					TheLobbyQueuedUTMs.clear();
					// Look for a game to join
					groupRoomToJoin = TheGameSpyInfo->getCurrentGroupRoom();
					Int selected;
					GadgetListBoxGetSelected(GetGameListBox(), &selected);
					if (selected >= 0)
					{
						// GeneralsX @bugfix Android port 07/11/2026 - Cast via uintptr_t for 64-bit
						Int selectedID = static_cast<Int>(reinterpret_cast<uintptr_t>(GadgetListBoxGetItemData(GetGameListBox(), selected)));
						if (selectedID > 0)
						{
							StagingRoomMap *srm = TheGameSpyInfo->getStagingRoomList();
							StagingRoomMap::iterator srmIt = srm->find(selectedID);
							if (srmIt != srm->end())
							{
								GameSpyStagingRoom *roomToJoin = srmIt->second;
								if (!roomToJoin || roomToJoin->getExeCRC() != TheGlobalData->m_exeCRC || roomToJoin->getIniCRC() != TheGlobalData->m_iniCRC)
								{
									// bad crc.  don't go.
									DEBUG_LOG(("WOLLobbyMenuSystem - CRC mismatch with the game I'm trying to join. My CRC's - EXE:0x%08X INI:0x%08X  Their CRC's - EXE:0x%08x INI:0x%08x", TheGlobalData->m_exeCRC, TheGlobalData->m_iniCRC, roomToJoin->getExeCRC(), roomToJoin->getIniCRC()));
#if defined(RTS_DEBUG)
									if (TheGlobalData->m_netMinPlayers)
									{
										GSMessageBoxOk(TheGameText->fetch("GUI:JoinFailedDefault"), TheGameText->fetch("GUI:JoinFailedCRCMismatch"));
										break;
									}
									else if (g_fakeCRC)
									{
										TheWritableGlobalData->m_exeCRC = roomToJoin->getExeCRC();
										TheWritableGlobalData->m_iniCRC = roomToJoin->getIniCRC();
									}
#else
									GSMessageBoxOk(TheGameText->fetch("GUI:JoinFailedDefault"), TheGameText->fetch("GUI:JoinFailedCRCMismatch"));
									break;
#endif
								}
								Bool unknownLadder = (roomToJoin->getLadderPort() && TheLadderList->findLadder(roomToJoin->getLadderIP(), roomToJoin->getLadderPort()) == nullptr);
								if (unknownLadder)
								{
									GSMessageBoxOk(TheGameText->fetch("GUI:JoinFailedDefault"), TheGameText->fetch("GUI:JoinFailedUnknownLadder"));
									break;
								}
								if (roomToJoin->getNumPlayers() == MAX_SLOTS)
								{
									GSMessageBoxOk(TheGameText->fetch("GUI:JoinFailedDefault"), TheGameText->fetch("GUI:JoinFailedRoomFull"));
									break;
								}
								TheGameSpyInfo->markAsStagingRoomJoiner(selectedID);
								TheGameSpyGame->setGameName(roomToJoin->getGameName());
								TheGameSpyGame->setLadderIP(roomToJoin->getLadderIP());
								TheGameSpyGame->setLadderPort(roomToJoin->getLadderPort());
								SetLobbyAttemptHostJoin( TRUE );
								if (roomToJoin->getHasPassword())
								{
									GameSpyOpenOverlay(GSOVERLAY_GAMEPASSWORD);
								}
								else
								{
									// no password - just join it
									PeerRequest req;
									req.peerRequestType = PeerRequest::PEERREQUEST_JOINSTAGINGROOM;
									req.text = srmIt->second->getGameName().str();
									req.stagingRoom.id = selectedID;
									req.password = "";
									TheGameSpyPeerMessageQueue->addRequest(req);
								}
							}
						}
						else
						{
							GSMessageBoxOk(TheGameText->fetch("GUI:Error"), TheGameText->fetch("GUI:NoGameInfo"), nullptr);
						}
					}
					else
					{
						GSMessageBoxOk(TheGameText->fetch("GUI:Error"), TheGameText->fetch("GUI:NoGameSelected"), nullptr);
					}
					*/
				}
				else if ( controlID == buttonBuddyID )
				{
					GameSpyToggleOverlay( GSOVERLAY_BUDDY );
				}
				else if ( controlID == buttonGameListTypeToggleID )
				{
					ToggleGameListType();
				}
				else if ( controlID == buttonEmoteID )
				{
				// read the user's input and clear the entry box
					UnicodeString txtInput;
					txtInput.set(GadgetTextEntryGetText( textEntryChat ));
					GadgetTextEntrySetText(textEntryChat, UnicodeString::TheEmptyString);
					txtInput.trim();
					if (!txtInput.isEmpty())
					{
                        if (!LobbyChatSlowmodeAllowsSend())
						{
							break;
						}
						// Send the message
						NGMP_OnlineServices_RoomsInterface* pRoomsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_RoomsInterface>();
						if (pRoomsInterface != nullptr)
						{
							pRoomsInterface->SendChatMessageToCurrentRoom(txtInput, false);
						}
					}
				}

				break;
			}

		//---------------------------------------------------------------------------------------------
		case GCM_SELECTED:
			{
				if (s_tryingToHostOrJoin)
					break;

				//NGMP_OnlineServices_RoomsInterface* pRoomsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_RoomsInterface>();
				//if (pRoomsInterface == nullptr)
				//{
				//	break;
				//}

				extern LobbyGameModeFilter theLobbyFilter;
				GameWindow *control = (GameWindow *)mData1;
				Int controlID = control->winGetWindowId();
				if( controlID == comboLobbyGroupRoomsID )
				{
					/*
					int rowSelected = -1;
					GadgetComboBoxGetSelectedPos(control, &rowSelected);

					DEBUG_LOG(("Row selected = %d", rowSelected));
					if (rowSelected >= 0)
					{
						Int groupID;
						// GeneralsX @bugfix Android port 07/11/2026 - Cast via uintptr_t for 64-bit
						groupID = static_cast<Int>(reinterpret_cast<uintptr_t>(GadgetComboBoxGetItemData(comboLobbyGroupRooms, rowSelected)));
						//DEBUG_LOG(("ItemData was %d, current Group Room is %d", groupID, TheGameSpyInfo->getCurrentGroupRoom()));
// did it change?
						if (groupID != pRoomsInterface->GetCurrentRoomID())
						{
							// join
							pRoomsInterface->JoinRoom(groupID, [=]()
								{
									//GadgetListBoxAddEntryText(listboxLobbyChat, UnicodeString(L"Attempting to join room"), GameMakeColor(255, 194, 15, 255), -1, -1);
								},
								[=]()
								{
									// TODO_NGMP: There are two cb lamdas for this, flatten them
									GadgetListBoxReset(listboxLobbyChat);

									UnicodeString msg;
									msg.format(TheGameText->fetch("GUI:LobbyJoined"), pRoomsInterface->GetGroupRooms().at(groupID).GetRoomDisplayName().str());
									GadgetListBoxAddEntryText(listboxLobbyChat, msg, GameSpyColor[GSCOLOR_DEFAULT], -1, -1);


									// process flag related info messages
									ERoomFlags flags = pRoomsInterface->GetGroupRooms().at(groupID).GetRoomFlags();
									if (flags == ERoomFlags::ROOM_FLAGS_SHOW_ALL_MATCHES)
									{
										//GadgetListBoxAddEntryText(listboxLobbyChat, UnicodeString(L"\t INFO: This is a special room where the lobby list shows lobbies from ALL rooms, not just the current room, to make it easier to find a match without room hopping."), GameMakeColor(255, 194, 15, 255), -1, -1);
										//GadgetListBoxAddEntryText(listboxLobbyChat, UnicodeString(L"\t INFO: Lobbies created here will only show in here. The members list & chat will also only show players in this room."), GameMakeColor(255, 194, 15, 255), -1, -1);
									}

									// refresh on join
									refreshPlayerList(TRUE);

									RefreshGameListBoxes();

									populateGroupRoomListbox(comboLobbyGroupRooms);
								});
						}

						// TODO_NGMP: What does TheGameSpyConfig->restrictGamesToLobby() do?
						/*						if (groupID && groupID != TheGameSpyInfo->getCurrentGroupRoom())
						{
							TheGameSpyInfo->leaveGroupRoom();
							TheGameSpyInfo->joinGroupRoom(groupID);
							if (TheGameSpyConfig->restrictGamesToLobby())
							{
								TheGameSpyInfo->clearStagingRoomList();
								RefreshGameListBoxes();
								PeerRequest req;
								req.peerRequestType = PeerRequest::PEERREQUEST_STARTGAMELIST;
								req.gameList.restrictGameList = TRUE;
								TheGameSpyPeerMessageQueue->addRequest(req);
							}
						}

					}
				*/
					Int pos = -1;
					GadgetComboBoxGetSelectedPos(comboLobbyGroupRooms, &pos);
					if (pos >= 0)
						// GeneralsX @bugfix Android port 07/11/2026 - Cast via uintptr_t for 64-bit
						theLobbyFilter = static_cast<LobbyGameModeFilter>(static_cast<Int>(reinterpret_cast<uintptr_t>(GadgetComboBoxGetItemData(comboLobbyGroupRooms, pos))));
					RefreshGameListBoxes();
				}
			}
			break;

		//---------------------------------------------------------------------------------------------
		case GLM_DOUBLE_CLICKED:
			{
				if (buttonPushed)
					break;

				GameWindow *control = (GameWindow *)mData1;
				Int controlID = control->winGetWindowId();
				if (controlID == GetGameListBoxID())
				{
					int rowSelected = mData2;

					if (rowSelected >= 0)
					{
						GadgetListBoxSetSelected( control, rowSelected );
						GameWindow *button = TheWindowManager->winGetWindowFromId( window, buttonJoinID );

						TheWindowManager->winSendSystemMsg( window, GBM_SELECTED,
																								(WindowMsgData)button, buttonJoinID );
					}
				}
				break;
			}

		//---------------------------------------------------------------------------------------------
		case GLM_RIGHT_CLICKED:
			{
				GameWindow *control = (GameWindow *)mData1;
				Int controlID = control->winGetWindowId();

				if (controlID == listboxLobbyPlayersID)
				{
#if defined(GENERALS_ONLINE)
					RightClickStruct* rc = (RightClickStruct*)mData2;
					WindowLayout* rcLayout = NULL;
					GameWindow* rcMenu;
					if (rc->pos < 0)
					{
						GadgetListBoxSetSelected(control, -1);
						break;
					}

					// TODO_NGMP: This causes issues with duplicate names. We should have better ways of looking this up + perhaps only allow unique names
					NGMP_OnlineServices_RoomsInterface* pRoomsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_RoomsInterface>();
					NGMP_OnlineServices_AuthInterface* pAuthInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_AuthInterface>();
					NGMP_OnlineServices_StatsInterface* pStatsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_StatsInterface>();
					NGMP_OnlineServices_SocialInterface* pSocialInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_SocialInterface>();
					if (pRoomsInterface != nullptr && pAuthInterface != nullptr && pStatsInterface != nullptr && pSocialInterface != nullptr)
					{

						// GeneralsX @bugfix Android port 07/11/2026 - Cast via uintptr_t for 64-bit
						int profileID = static_cast<int>(reinterpret_cast<uintptr_t>(GadgetListBoxGetItemData(listboxLobbyPlayers, rc->pos, 0)));
						NetworkRoomMember* roomMember = pRoomsInterface->GetRoomMemberFromID(profileID);

						if (rc->pos >= 0)
						{
							if (roomMember != nullptr)
							{
								AsciiString aName = AsciiString(roomMember->display_name.c_str());
								int64_t localuserID = pAuthInterface->GetUserID();

								Bool isBuddy = pSocialInterface->IsUserFriend(profileID);
								if (profileID <= 0)
									rcLayout = TheWindowManager->winCreateLayout(AsciiString("Menus/RCNoProfileMenu.wnd"));
								else
								{
									if (profileID == localuserID)
									{
										rcLayout = TheWindowManager->winCreateLayout(AsciiString("Menus/RCLocalPlayerMenu.wnd"));
									}
									else if (isBuddy)
									{
										rcLayout = TheWindowManager->winCreateLayout(AsciiString("Menus/RCBuddiesMenu.wnd"));
									}
									else
										rcLayout = TheWindowManager->winCreateLayout(AsciiString("Menus/RCNonBuddiesMenu.wnd"));
								}
								if (!rcLayout)
									break;

								GadgetListBoxSetSelected(control, rc->pos);

								rcMenu = rcLayout->getFirstWindow();
								rcMenu->winGetLayout()->runInit();
								rcMenu->winBringToTop();
								rcMenu->winHide(FALSE);
								setUnignoreText(rcLayout, aName, profileID);
								ICoord2D rcSize, rcPos;
								rcMenu->winGetSize(&rcSize.x, &rcSize.y);
								rcPos.x = rc->mouseX;
								rcPos.y = rc->mouseY;
								if (rc->mouseX + rcSize.x > TheDisplay->getWidth())
									rcPos.x = TheDisplay->getWidth() - rcSize.x;
								if (rc->mouseY + rcSize.y > TheDisplay->getHeight())
									rcPos.y = TheDisplay->getHeight() - rcSize.y;
								rcMenu->winSetPosition(rcPos.x, rcPos.y);

								GameSpyRCMenuData* rcData = NEW GameSpyRCMenuData;
								rcData->m_id = profileID;
								rcData->m_nick = aName;
								rcData->m_itemType = (isBuddy) ? ITEM_BUDDY : ITEM_NONBUDDY;
								rcMenu->winSetUserData((void*)rcData);
								TheWindowManager->winSetLoneWindow(rcMenu);
							}
							else if (controlID == GetGameListBoxID())
							{
								// TODO_NGMP: enable right click for ladders again
								break;

								RightClickStruct* rc = (RightClickStruct*)mData2;
								WindowLayout* rcLayout = NULL;
								GameWindow* rcMenu;
								if (rc->pos < 0)
								{
									GadgetListBoxSetSelected(control, -1);
									break;
								}

								// GeneralsX @bugfix Android port 07/11/2026 - Cast via uintptr_t for 64-bit
								Int selectedID = static_cast<Int>(reinterpret_cast<uintptr_t>(GadgetListBoxGetItemData(control, rc->pos)));
								if (selectedID > 0)
								{
									StagingRoomMap* srm = TheGameSpyInfo->getStagingRoomList();
									StagingRoomMap::iterator srmIt = srm->find(selectedID);
									if (srmIt != srm->end())
									{
										GameSpyStagingRoom* theRoom = srmIt->second;
										if (!theRoom)
											break;
										const LadderInfo* linfo = TheLadderList->findLadder(theRoom->getLadderIP(), theRoom->getLadderPort());
										if (linfo)
										{
											rcLayout = TheWindowManager->winCreateLayout(AsciiString("Menus/RCGameDetailsMenu.wnd"));
											if (!rcLayout)
												break;

											GadgetListBoxSetSelected(control, rc->pos);

											rcMenu = rcLayout->getFirstWindow();
											rcMenu->winGetLayout()->runInit();
											rcMenu->winBringToTop();
											rcMenu->winHide(FALSE);
											rcMenu->winSetPosition(rc->mouseX, rc->mouseY);

											rcMenu->winSetUserData((void*)selectedID);
											TheWindowManager->winSetLoneWindow(rcMenu);
										}
									}
								}
							}
						}
					}
				}
				break;
		}
#else
					RightClickStruct *rc = (RightClickStruct *)mData2;
					WindowLayout *rcLayout = nullptr;
					GameWindow *rcMenu;
					if(rc->pos < 0)
					{
						GadgetListBoxSetSelected(control, -1);
						break;
					}

					GPProfile profileID = 0;
					AsciiString aName;
					aName.translate(GadgetListBoxGetText(control, rc->pos, COLUMN_PLAYERNAME));
					PlayerInfoMap::iterator it = TheGameSpyInfo->getPlayerInfoMap()->find(aName);
					if (it != TheGameSpyInfo->getPlayerInfoMap()->end())
						profileID = it->second.m_profileID;

					Bool isBuddy = FALSE;
					if (profileID <= 0)
						rcLayout = TheWindowManager->winCreateLayout("Menus/RCNoProfileMenu.wnd");
					else
					{
						if (profileID == TheGameSpyInfo->getLocalProfileID())
						{
							rcLayout = TheWindowManager->winCreateLayout("Menus/RCLocalPlayerMenu.wnd");
						}
						else if(TheGameSpyInfo->isBuddy(profileID))
						{
							rcLayout = TheWindowManager->winCreateLayout("Menus/RCBuddiesMenu.wnd");
							isBuddy = TRUE;
						}
						else
							rcLayout = TheWindowManager->winCreateLayout("Menus/RCNonBuddiesMenu.wnd");
					}
					if(!rcLayout)
						break;

					GadgetListBoxSetSelected(control, rc->pos);

					rcMenu = rcLayout->getFirstWindow();
					rcMenu->winGetLayout()->runInit();
					rcMenu->winBringToTop();
					rcMenu->winHide(FALSE);
					setUnignoreText( rcLayout, aName, profileID);
					ICoord2D rcSize, rcPos;
					rcMenu->winGetSize(&rcSize.x, &rcSize.y);
					rcPos.x = rc->mouseX;
					rcPos.y = rc->mouseY;
					if(rc->mouseX + rcSize.x > TheDisplay->getWidth())
						rcPos.x = TheDisplay->getWidth() - rcSize.x;
					if(rc->mouseY + rcSize.y > TheDisplay->getHeight())
						rcPos.y = TheDisplay->getHeight() - rcSize.y;
					rcMenu->winSetPosition(rcPos.x, rcPos.y);

					GameSpyRCMenuData *rcData = NEW GameSpyRCMenuData;
					rcData->m_id = profileID;
					rcData->m_nick = aName;
					rcData->m_itemType = (isBuddy)?ITEM_BUDDY:ITEM_NONBUDDY;
					rcMenu->winSetUserData((void *)rcData);
					TheWindowManager->winSetLoneWindow(rcMenu);
				}
				else if( controlID == GetGameListBoxID() )
				{
					// TODO_NGMP: enable right click for ladders again
					break;

					RightClickStruct *rc = (RightClickStruct *)mData2;
					WindowLayout *rcLayout = nullptr;
					GameWindow *rcMenu;
					if(rc->pos < 0)
					{
						GadgetListBoxSetSelected(control, -1);
						break;
					}

					// GeneralsX @bugfix Android port 07/11/2026 - Cast via uintptr_t for 64-bit
					Int selectedID = static_cast<Int>(reinterpret_cast<uintptr_t>(GadgetListBoxGetItemData(control, rc->pos)));
					if (selectedID > 0)
					{
						StagingRoomMap *srm = TheGameSpyInfo->getStagingRoomList();
						StagingRoomMap::iterator srmIt = srm->find(selectedID);
						if (srmIt != srm->end())
						{
							GameSpyStagingRoom *theRoom = srmIt->second;
							if (!theRoom)
								break;
							const LadderInfo *linfo = TheLadderList->findLadder(theRoom->getLadderIP(), theRoom->getLadderPort());
							if (linfo)
							{
								rcLayout = TheWindowManager->winCreateLayout("Menus/RCGameDetailsMenu.wnd");
								if (!rcLayout)
									break;

								GadgetListBoxSetSelected(control, rc->pos);

								rcMenu = rcLayout->getFirstWindow();
								rcMenu->winGetLayout()->runInit();
								rcMenu->winBringToTop();
								rcMenu->winHide(FALSE);
								rcMenu->winSetPosition(rc->mouseX, rc->mouseY);

								rcMenu->winSetUserData((void *)selectedID);
								TheWindowManager->winSetLoneWindow(rcMenu);
							}
						}
					}
				}
				break;
			}
#endif

//		//---------------------------------------------------------------------------------------------
//		case GSM_SLIDER_TRACK:
//		{
//				if (buttonPushed)
//					break;
//
//			GameWindow *control = (GameWindow *)mData1;
//			Int val = (Int)mData2;
//			Int controlID = control->winGetWindowId();
//			if (controlID == sliderChatAdjustID)
//			{
//				doSliderTrack(control, val);
//			}
//			break;
//		}

		//---------------------------------------------------------------------------------------------
		case GEM_EDIT_DONE:
			{
				if (buttonPushed)
					break;

				// read the user's input and clear the entry box
				UnicodeString txtInput;
				txtInput.set(GadgetTextEntryGetText( textEntryChat ));
				GadgetTextEntrySetText(textEntryChat, UnicodeString::TheEmptyString);
				txtInput.trim();
				if (!txtInput.isEmpty())
				{
					// Send the message
					if (!handleLobbySlashCommands(txtInput))
					{
                        if (!LobbyChatSlowmodeAllowsSend())
						{
							break;
						}
						std::shared_ptr<WebSocket>  pWS = NGMP_OnlineServicesManager::GetWebSocket();
						if (pWS != nullptr)
						{
							pWS->SendData_RoomChatMessage(txtInput, false);
						}
						// TODO_NGMP: Support private message again
						//TheGameSpyInfo->sendChat( txtInput, false, listboxLobbyPlayers );
					}
				}
				break;
			}

		//---------------------------------------------------------------------------------------------
		default:
			return MSG_IGNORED;

	}

	return MSG_HANDLED;
}
