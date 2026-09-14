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

// GameEngine.cpp /////////////////////////////////////////////////////////////////////////////////
// Implementation of the Game Engine singleton
// Author: Michael S. Booth, April 2001

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ActionManager.h"
#include "Common/AudioAffect.h"
#include "Common/BuildAssistant.h"
#include "Common/CRCDebug.h"
#include "Common/FramePacer.h"
#include "Common/Radar.h"
#include "Common/PlayerTemplate.h"
#include "Common/Team.h"
#include "Common/PlayerList.h"
#include "Common/GameAudio.h"
#include "Common/GameEngine.h"
#include "Common/INI.h"
#include "Common/INIException.h"
#include "Common/MessageStream.h"
#include "Common/ThingFactory.h"
#include "Common/file.h"
#include "Common/FileSystem.h"
#include "Common/ArchiveFileSystem.h"
#include "Common/LocalFileSystem.h"
#include "Common/GlobalData.h"
#include "Common/PerfTimer.h"
#include "Common/RandomValue.h"
#include "Common/NameKeyGenerator.h"
#include "Common/ModuleFactory.h"
#include "Common/Debug.h"
#include "Common/GameState.h"
#include "Common/GameStateMap.h"
#include "Common/Science.h"
#include "Common/FunctionLexicon.h"
#include "Common/CommandLine.h"
#include "Common/DamageFX.h"
#include "Common/MultiplayerSettings.h"
#include "Common/Recorder.h"
#include "Common/SpecialPower.h"
#include "Common/TerrainTypes.h"
#include "Common/Upgrade.h"
#include "Common/OptionPreferences.h"
#include "Common/Xfer.h"
#include "Common/XferCRC.h"
#include "Common/GameLOD.h"
#include "Common/Registry.h"
#include "Common/GameCommon.h"	// FOR THE ALLOW_DEBUG_CHEATS_IN_RELEASE #define
#include "GXTrace.h"

#include <chrono>

// GeneralsX @bugfix Android port 16/07/2026 __cxa_current_exception_type() /
// __cxa_demangle() are Itanium C++ ABI runtime entry points (libc++abi on
// Android/bionic, libstdc++/libsupc++ on Linux) -- not available under MSVC's
// ABI, which has no equivalent public API. Only used inside the update()
// catch(...) diagnostic below, gated the same way (issue #2).
#if !defined(_MSC_VER)
#include <cxxabi.h>
#endif

#include "GameLogic/Armor.h"
#include "GameLogic/AI.h"
#include "GameLogic/CaveSystem.h"
#include "GameLogic/CrateSystem.h"
#include "GameLogic/Damage.h"
#include "GameLogic/VictoryConditions.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/RankInfo.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/SidesList.h"

#include "GameClient/ClientInstance.h"
#include "GameClient/FXList.h"
#include "GameClient/GameClient.h"
#include "GameClient/Keyboard.h"
#include "GameClient/Shell.h"
#include "GameClient/GameText.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/Water.h"
#include "GameClient/TerrainRoads.h"
#include "GameClient/MetaEvent.h"
#include "GameClient/MapUtil.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GlobalLanguage.h"
#include "GameClient/Drawable.h"
#include "GameClient/GUICallbacks.h"

#include "GameNetwork/NetworkInterface.h"
#include "GameNetwork/WOLBrowser/WebBrowser.h"
#include "GameNetwork/LANAPI.h"
#include "GameNetwork/GameSpy/GameResultsThread.h"
// GeneralsX @feature Android port 10/07/2026 GeneralsOnline's HTTPManager/
// WebSocket need pumping once a frame, same as TheNetwork below, or nothing
// they kick off (auth polling, WS connect, HTTP requests) ever progresses.
#include "GameNetwork/GeneralsOnline/NGMP_interfaces.h"
// GeneralsX @bugfix Android port 07/11/2026 - needed for GSMessageBoxOk/GameSpyCloseAllOverlays used by TearDownGeneralsOnline below
#include "GameNetwork/GameSpyOverlay.h"

#include "Common/version.h"

// GeneralsX @bugfix Android port 07/11/2026 - ported from upstream GeneralsOnline: request a delayed teardown of NGMP
// online services, deferred to the next GameEngine::update() so it doesn't destroy the manager mid-callback.
static bool g_bTearDownGeneralsOnlineRequested = false;
void TearDownGeneralsOnline()
{
	g_bTearDownGeneralsOnlineRequested = true;

	if (NGMP_OnlineServicesManager::GetInstance() == nullptr)
		return;

	EGOTearDownReason teardownReason = NGMP_OnlineServicesManager::GetInstance()->GetTeardownReason();

	if (teardownReason != EGOTearDownReason::USER_REQUESTED_SILENT)
	{
		UnicodeString title, body;

		if (teardownReason == EGOTearDownReason::USER_LOGOUT)
		{
			title = L"Logged Out";
			body = L"You are now logged out of GeneralsOnline.";
		}
		else if (teardownReason == EGOTearDownReason::LOST_CONNECTION)
		{
			title = TheGameText->fetch("GUI:GSErrorTitle");
			body = L"Your connection to the Generals Online servers was lost.";
		}
		else
		{
			title = TheGameText->fetch("GUI:GSErrorTitle");
			body = L"An unknown error occurred.";
		}

		NGMP_OnlineServicesManager::GetInstance()->ResetPendingFullTeardownReason();

		GameSpyCloseAllOverlays();
		GSMessageBoxOk(title, body);
	}
}

//-------------------------------------------------------------------------------------------------

#ifdef DEBUG_CRC
class DeepCRCSanityCheck : public SubsystemInterface
{
public:
	DeepCRCSanityCheck() {}
	virtual ~DeepCRCSanityCheck() {}

	virtual void init() {}
	virtual void reset();
	virtual void update() {}

protected:
};

DeepCRCSanityCheck *TheDeepCRCSanityCheck = nullptr;

void DeepCRCSanityCheck::reset()
{
	static Int timesThrough = 0;
	static UnsignedInt lastCRC = 0;

	AsciiString fname;
	fname.format("%sCRCAfter%dMaps.dat", TheGlobalData->getPath_UserData().str(), timesThrough);
	UnsignedInt thisCRC = TheGameLogic->getCRC( CRC_RECALC, fname );

	DEBUG_LOG(("DeepCRCSanityCheck: CRC is %X", thisCRC));
	DEBUG_ASSERTCRASH(timesThrough == 0 || thisCRC == lastCRC,
		("CRC after reset did not match beginning CRC!\nNetwork games won't work after this.\nOld: 0x%8.8X, New: 0x%8.8X",
		lastCRC, thisCRC));
	lastCRC = thisCRC;

	timesThrough++;
}
#endif // DEBUG_CRC

//-------------------------------------------------------------------------------------------------
/// The GameEngine singleton instance
GameEngine *TheGameEngine = nullptr;

//-------------------------------------------------------------------------------------------------
SubsystemInterfaceList* TheSubsystemList = nullptr;

//-------------------------------------------------------------------------------------------------
template<class SUBSYSTEM>
void initSubsystem(
	SUBSYSTEM*& sysref,
	AsciiString name,
	SUBSYSTEM* sys,
	Xfer *pXfer,
	const char* path1 = nullptr,
	const char* path2 = nullptr)
{
	sysref = sys;
	TheSubsystemList->initSubsystem(sys, path1, path2, pXfer, name);
}

//-------------------------------------------------------------------------------------------------
extern HINSTANCE ApplicationHInstance;  ///< our application instance
// TheSuperHackers @build fighter19 11/02/2026 COM module (Windows-only)
#ifdef _WIN32
extern CComModule _Module;
#endif

//-------------------------------------------------------------------------------------------------
static void updateTGAtoDDS();

//-------------------------------------------------------------------------------------------------
static void updateWindowTitle()
{
	// TheSuperHackers @tweak Now prints product and version information in the Window title.

	DEBUG_ASSERTCRASH(TheVersion != nullptr, ("TheVersion is null"));
	DEBUG_ASSERTCRASH(TheGameText != nullptr, ("TheGameText is null"));

	UnicodeString title;

	if (rts::ClientInstance::getInstanceId() > 1u)
	{
		UnicodeString str;
		str.format(L"Instance:%.2u", rts::ClientInstance::getInstanceId());
		title.concat(str);
	}

	UnicodeString productString = TheVersion->getUnicodeProductString();

	if (!productString.isEmpty())
	{
		if (!title.isEmpty())
			title.concat(L" ");
		title.concat(productString);
	}

#if RTS_GENERALS
	const WideChar* defaultGameTitle = L"Command and Conquer Generals";
#elif RTS_ZEROHOUR
	const WideChar* defaultGameTitle = L"Command and Conquer Generals Zero Hour";
#endif
	UnicodeString gameTitle = TheGameText->FETCH_OR_SUBSTITUTE("GUI:Command&ConquerGenerals", defaultGameTitle);

	if (!gameTitle.isEmpty())
	{
		UnicodeString gameTitleFinal;
		UnicodeString gameVersion = TheVersion->getUnicodeVersion();

		if (productString.isEmpty())
		{
			gameTitleFinal = gameTitle;
		}
		else
		{
			UnicodeString gameTitleFormat = TheGameText->FETCH_OR_SUBSTITUTE("Version:GameTitle", L"for %ls");
			gameTitleFinal.format(gameTitleFormat.str(), gameTitle.str());
		}

		if (!title.isEmpty())
			title.concat(L" ");
		title.concat(gameTitleFinal.str());
		title.concat(L" ");
		title.concat(gameVersion.str());
	}

	if (!title.isEmpty())
	{
		AsciiString titleA;
		titleA.translate(title);	//get ASCII version for Win 9x

		extern HWND ApplicationHWnd;  ///< our application window handle
		if (ApplicationHWnd) {
// TheSuperHackers @build fighter19 11/02/2026 SetWindowText is Windows-only
#ifdef _WIN32
			//Set it twice because Win 9x does not support SetWindowTextW.
			::SetWindowText(ApplicationHWnd, titleA.str());
			::SetWindowTextW(ApplicationHWnd, title.str());
#else
			// Linux: SDL3 handles window title (set via SDL_SetWindowTitle if needed)
#endif
		}
	}
}

//-------------------------------------------------------------------------------------------------
GameEngine::GameEngine()
{
	// initialize to non garbage values
	m_logicTimeAccumulator = 0.0f;
	m_quitting = FALSE;
	m_isActive = FALSE;

// TheSuperHackers @build fighter19 11/02/2026 COM initialization (Windows-only)
#ifdef _WIN32
	_Module.Init(nullptr, ApplicationHInstance, nullptr);
#endif
}

//-------------------------------------------------------------------------------------------------
GameEngine::~GameEngine()
{
	//extern std::vector<std::string>	preloadTextureNamesGlobalHack;
	//preloadTextureNamesGlobalHack.clear();

	delete TheMapCache;
	TheMapCache = nullptr;

//	delete TheShell;
//	TheShell = nullptr;

	TheGameResultsQueue->endThreads();

	// TheSuperHackers @fix helmutbuhler 03/06/2025
	// Reset all subsystems before deletion to prevent crashing due to cross dependencies.
	reset();

	TheSubsystemList->shutdownAll();
	delete TheSubsystemList;
	TheSubsystemList = nullptr;

	delete TheSkirmishGameInfo;
	TheSkirmishGameInfo = nullptr;

	delete TheChallengeGameInfo;
	TheChallengeGameInfo = nullptr;

	delete TheNetwork;
	TheNetwork = nullptr;

	delete TheCommandList;
	TheCommandList = nullptr;

	delete TheNameKeyGenerator;
	TheNameKeyGenerator = nullptr;

	delete TheFileSystem;
	TheFileSystem = nullptr;

	delete TheGameLODManager;
	TheGameLODManager = nullptr;

	Drawable::killStaticImages();

// TheSuperHackers @build fighter19 11/02/2026 COM termination (Windows-only)
#ifdef _WIN32
	_Module.Term();
#endif

#ifdef PERF_TIMERS
	PerfGather::termPerfDump();
#endif
}

//-------------------------------------------------------------------------------------------------
Bool GameEngine::isTimeFrozen()
{
	// TheSuperHackers @fix The time can no longer be frozen in Network games. It would disconnect the player.
	if (TheNetwork != nullptr)
		return false;

	if (TheTacticalView != nullptr)
	{
		if (TheTacticalView->isTimeFrozen() && !TheTacticalView->isCameraMovementFinished())
			return true;
	}

	if (TheScriptEngine != nullptr)
	{
		if (TheScriptEngine->isTimeFrozenDebug() || TheScriptEngine->isTimeFrozenScript())
			return true;
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
Bool GameEngine::isGameHalted()
{
	if (TheNetwork != nullptr)
	{
		if (TheNetwork->isStalling())
			return true;
	}
	else
	{
		if (TheGameLogic != nullptr && TheGameLogic->isGamePaused())
			return true;
	}

	return false;
}

/** -----------------------------------------------------------------------------------------------
 * Initialize the game engine by initializing the GameLogic and GameClient.
 */
void GameEngine::init()
{
	try {
		//create an INI object to use for loading stuff
		INI ini;

#ifdef DEBUG_LOGGING
		if (TheVersion)
		{
			DEBUG_LOG(("================================================================================"));
			DEBUG_LOG(("Generals version %s", TheVersion->getAsciiVersion().str()));
			DEBUG_LOG(("Build date: %s", TheVersion->getAsciiBuildTime().str()));
			DEBUG_LOG(("Build location: %s", TheVersion->getAsciiBuildLocation().str()));
			DEBUG_LOG(("Build user: %s", TheVersion->getAsciiBuildUser().str()));
			DEBUG_LOG(("Build git revision: %s", TheVersion->getAsciiGitCommitCount().str()));
			DEBUG_LOG(("Build git version: %s", TheVersion->getAsciiGitTagOrHash().str()));
			DEBUG_LOG(("Build git commit time: %s", TheVersion->getAsciiGitCommitTime().str()));
			DEBUG_LOG(("Build git commit author: %s", Version::getGitCommitAuthorName()));
			DEBUG_LOG(("================================================================================"));
		}
#endif

	#if defined(PERF_TIMERS) || defined(DUMP_PERF_STATS)
		DEBUG_LOG(("Calculating CPU frequency for performance timers."));
		InitPrecisionTimer();
	#endif
	#ifdef PERF_TIMERS
		PerfGather::initPerfDump("AAAPerfStats", PerfGather::PERF_NETTIME);
	#endif




	#ifdef DUMP_PERF_STATS////////////////////////////////////////////////////////////
	__int64 startTime64;//////////////////////////////////////////////////////////////
	__int64 endTime64,freq64;///////////////////////////////////////////////////////////
	GetPrecisionTimerTicksPerSec(&freq64);///////////////////////////////////////////////
	GetPrecisionTimer(&startTime64);////////////////////////////////////////////////////
  char Buf[256];//////////////////////////////////////////////////////////////////////
	#endif//////////////////////////////////////////////////////////////////////////////


		TheSubsystemList = MSGNEW("GameEngineSubsystem") SubsystemInterfaceList;

		TheSubsystemList->addSubsystem(this);

		// initialize the random number system
		InitRandom();

		// Create the low-level file system interface
		TheFileSystem = createFileSystem();

		// not part of the subsystem list, because it should normally never be reset!
		TheNameKeyGenerator = MSGNEW("GameEngineSubsystem") NameKeyGenerator;
		TheNameKeyGenerator->init();


    	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheNameKeyGenerator  = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		// not part of the subsystem list, because it should normally never be reset!
		TheCommandList = MSGNEW("GameEngineSubsystem") CommandList;
		TheCommandList->init();

    	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheCommandList  = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		XferCRC xferCRC;
		xferCRC.open("lightCRC");


		initSubsystem(TheLocalFileSystem, "TheLocalFileSystem", createLocalFileSystem(), nullptr);


    	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheLocalFileSystem  = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		initSubsystem(TheArchiveFileSystem, "TheArchiveFileSystem", createArchiveFileSystem(), nullptr); // this MUST come after TheLocalFileSystem creation

    	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheArchiveFileSystem  = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		DEBUG_ASSERTCRASH(TheWritableGlobalData,("TheWritableGlobalData expected to be created"));
	initSubsystem(TheWritableGlobalData, "TheWritableGlobalData", TheWritableGlobalData, &xferCRC, "Data\\INI\\Default\\GameData", "Data\\INI\\GameData");
	TheWritableGlobalData->parseCustomDefinition();

	// GeneralsX @feature felipebraz 08/06/2026 Auto-create SagePatch.ini in user data dir with defaults.
	// This replaces the run.sh copy approach with engine-managed defaults.
	{
		AsciiString sagePatchPath = TheWritableGlobalData->getPath_UserData();
		sagePatchPath.concat("SagePatch.ini");

		if (!TheLocalFileSystem->doesFileExist(sagePatchPath.str()))
		{
			FILE *f = fopen(sagePatchPath.str(), "w");
			if (f)
			{
				fprintf(f,
				"; -----------------------------------------------------------------------------\n"
				"; SagePatch - Casual QoL overrides for GeneralsX\n"
				";\n"
				"; Loaded by the engine after the BIG-archived Data/INI/GameData.ini, so values\n"
				"; here override (not append to) the originals.\n"
				"; -----------------------------------------------------------------------------\n"
				"\n"
				"GameData\n"
				"  ; Slightly higher than vanilla (310); further out without seeing past the map border.\n"
				"  MaxCameraHeight = 350.0\n"
				"  ; Slightly lower than vanilla (120) so casual zoom-in feels useful.\n"
				"  MinCameraHeight = 100.0\n"
					"  ; Still soft-disabled so the user can push past max without a hard clamp.\n"
					"  EnforceMaxCameraHeight = No\n"
					"  ; Keyboard scroll - vanilla 0.5 is sluggish, double it.\n"
					"  KeyboardScrollSpeedFactor = 1.0\n"
					"  ; ~5% more terrain drawn at max zoom to fix terrain pop-in.\n"
					"  TerrainDrawDistanceScale = 1.05\n"
					"End\n"
				);
				fclose(f);
			}
		}

		if (TheLocalFileSystem->doesFileExist(sagePatchPath.str()))
		{
			ini.load(sagePatchPath, INI_LOAD_OVERWRITE, nullptr);
		}
	}

	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After  TheWritableGlobalData = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////



	#if defined(RTS_DEBUG)
		// If we're in Debug, load the Debug settings as well.
		ini.loadFileDirectory( "Data\\INI\\GameDataDebug", INI_LOAD_OVERWRITE, nullptr );
	#endif

		// special-case: parse command-line parameters after loading global data
		CommandLine::parseCommandLineForEngineInit();

		TheArchiveFileSystem->loadMods();

		// doesn't require resets so just create a single instance here.
		TheGameLODManager = MSGNEW("GameEngineSubsystem") GameLODManager;
		TheGameLODManager->init();

		// after parsing the command line, we may want to perform dds stuff. Do that here.
		if (TheGlobalData->m_shouldUpdateTGAToDDS) {
			// update any out of date targas here.
			updateTGAtoDDS();
		}

		// read the water settings from INI (must do prior to initing GameClient, apparently)
		ini.loadFileDirectory( "Data\\INI\\Default\\Water", INI_LOAD_OVERWRITE, &xferCRC );
		ini.loadFileDirectory( "Data\\INI\\Water", INI_LOAD_OVERWRITE, &xferCRC );
		ini.loadFileDirectory( "Data\\INI\\Default\\Weather", INI_LOAD_OVERWRITE, &xferCRC );
		ini.loadFileDirectory( "Data\\INI\\Weather", INI_LOAD_OVERWRITE, &xferCRC );



	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After water INI's = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


#ifdef DEBUG_CRC
		initSubsystem(TheDeepCRCSanityCheck, "TheDeepCRCSanityCheck", MSGNEW("GameEngineSubystem") DeepCRCSanityCheck, nullptr);
#endif // DEBUG_CRC
		initSubsystem(TheGameText, "TheGameText", CreateGameTextInterface(), nullptr);
		updateWindowTitle();

	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheGameText = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


#if RETAIL_COMPATIBLE_CRC
		if (xferCRC.getCRC() == 0xA1E7F8E6)
			TheNameKeyGenerator->verifyNameKeyID(1);
#endif

		initSubsystem(TheScienceStore,"TheScienceStore", MSGNEW("GameEngineSubsystem") ScienceStore(), &xferCRC, "Data\\INI\\Default\\Science", "Data\\INI\\Science");
		initSubsystem(TheMultiplayerSettings,"TheMultiplayerSettings", MSGNEW("GameEngineSubsystem") MultiplayerSettings(), &xferCRC, "Data\\INI\\Default\\Multiplayer", "Data\\INI\\Multiplayer");
		initSubsystem(TheTerrainTypes,"TheTerrainTypes", MSGNEW("GameEngineSubsystem") TerrainTypeCollection(), &xferCRC, "Data\\INI\\Default\\Terrain", "Data\\INI\\Terrain");
		initSubsystem(TheTerrainRoads,"TheTerrainRoads", MSGNEW("GameEngineSubsystem") TerrainRoadCollection(), &xferCRC, "Data\\INI\\Default\\Roads", "Data\\INI\\Roads");
		initSubsystem(TheGlobalLanguageData,"TheGlobalLanguageData",MSGNEW("GameEngineSubsystem") GlobalLanguage, nullptr); // must be before the game text
		TheGlobalLanguageData->parseCustomDefinition();
	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheGlobalLanguageData = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////
		initSubsystem(TheAudio,"TheAudio", createAudioManager(TheGlobalData->m_headless), nullptr);
		if (!TheAudio->isMusicAlreadyLoaded())
		{
			// GeneralsX @bugfix Android port 06/09/2026 This one missing file used
			// to end the session outright: init ran to completion, the main menu
			// was built, and the loop's first check quit -- a black screen, then
			// the app closing with status 0 and not one line of explanation. A
			// real report cost a whole debugging session to get this far.
			//
			// On desktop the check is a reasonable "the game data was never
			// installed" guard. On Android people assemble their own file set by
			// hand, so a single absent music track is both far more likely and far
			// less serious, and the folder check in the Setup app now covers the
			// case this was really guarding against. Say what happened and keep
			// going: a game that runs without music beats one that vanishes.
			fprintf(stderr, "[gxaudio] music data missing -- see the [gxaudio] line above for the file\n");
#if defined(__ANDROID__)
			fprintf(stderr, "[gxaudio] continuing anyway (Android): music may be silent\n");
#else
			setQuitting(TRUE);
#endif
		}

#if RTS_ZEROHOUR && RETAIL_COMPATIBLE_CRC
		TheNameKeyGenerator->syncNameKeyID();
#endif

	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheAudio = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		initSubsystem(TheFunctionLexicon,"TheFunctionLexicon", createFunctionLexicon(), nullptr);
		initSubsystem(TheModuleFactory,"TheModuleFactory", createModuleFactory(), nullptr);
		initSubsystem(TheMessageStream,"TheMessageStream", createMessageStream(), nullptr);
		initSubsystem(TheSidesList,"TheSidesList", MSGNEW("GameEngineSubsystem") SidesList(), nullptr);
		initSubsystem(TheCaveSystem,"TheCaveSystem", MSGNEW("GameEngineSubsystem") CaveSystem(), nullptr);
		initSubsystem(TheRankInfoStore,"TheRankInfoStore", MSGNEW("GameEngineSubsystem") RankInfoStore(), &xferCRC, nullptr, "Data\\INI\\Rank");
		initSubsystem(ThePlayerTemplateStore,"ThePlayerTemplateStore", MSGNEW("GameEngineSubsystem") PlayerTemplateStore(), &xferCRC, "Data\\INI\\Default\\PlayerTemplate", "Data\\INI\\PlayerTemplate");
		initSubsystem(TheParticleSystemManager,"TheParticleSystemManager", createParticleSystemManager(TheGlobalData->m_headless), nullptr);

	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheParticleSystemManager = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		initSubsystem(TheFXListStore,"TheFXListStore", MSGNEW("GameEngineSubsystem") FXListStore(), &xferCRC, "Data\\INI\\Default\\FXList", "Data\\INI\\FXList");
		initSubsystem(TheWeaponStore,"TheWeaponStore", MSGNEW("GameEngineSubsystem") WeaponStore(), &xferCRC, nullptr, "Data\\INI\\Weapon");
		initSubsystem(TheObjectCreationListStore,"TheObjectCreationListStore", MSGNEW("GameEngineSubsystem") ObjectCreationListStore(), &xferCRC, "Data\\INI\\Default\\ObjectCreationList", "Data\\INI\\ObjectCreationList");
		initSubsystem(TheLocomotorStore,"TheLocomotorStore", MSGNEW("GameEngineSubsystem") LocomotorStore(), &xferCRC, nullptr, "Data\\INI\\Locomotor");
		initSubsystem(TheSpecialPowerStore,"TheSpecialPowerStore", MSGNEW("GameEngineSubsystem") SpecialPowerStore(), &xferCRC, "Data\\INI\\Default\\SpecialPower", "Data\\INI\\SpecialPower");
		initSubsystem(TheDamageFXStore,"TheDamageFXStore", MSGNEW("GameEngineSubsystem") DamageFXStore(), &xferCRC, nullptr, "Data\\INI\\DamageFX");
		initSubsystem(TheArmorStore,"TheArmorStore", MSGNEW("GameEngineSubsystem") ArmorStore(), &xferCRC, nullptr, "Data\\INI\\Armor");
		initSubsystem(TheBuildAssistant,"TheBuildAssistant", MSGNEW("GameEngineSubsystem") BuildAssistant, nullptr);


	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheBuildAssistant = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////



		initSubsystem(TheThingFactory,"TheThingFactory", createThingFactory(), &xferCRC, "Data\\INI\\Default\\Object", "Data\\INI\\Object");

	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheThingFactory = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


#if RETAIL_COMPATIBLE_CRC
		if (xferCRC.getCRC() == 0x6209AF6E)
			TheNameKeyGenerator->verifyNameKeyID(2265);
#endif

		initSubsystem(TheUpgradeCenter,"TheUpgradeCenter", MSGNEW("GameEngineSubsystem") UpgradeCenter, &xferCRC, "Data\\INI\\Default\\Upgrade", "Data\\INI\\Upgrade");
		initSubsystem(TheGameClient,"TheGameClient", createGameClient(), nullptr);


	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheGameClient = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		initSubsystem(TheAI,"TheAI", MSGNEW("GameEngineSubsystem") AI(), &xferCRC,  "Data\\INI\\Default\\AIData", "Data\\INI\\AIData");
		initSubsystem(TheGameLogic,"TheGameLogic", createGameLogic(), nullptr);
		initSubsystem(TheTeamFactory,"TheTeamFactory", MSGNEW("GameEngineSubsystem") TeamFactory(), nullptr);
		initSubsystem(TheCrateSystem,"TheCrateSystem", MSGNEW("GameEngineSubsystem") CrateSystem(), &xferCRC, "Data\\INI\\Default\\Crate", "Data\\INI\\Crate");
		initSubsystem(ThePlayerList,"ThePlayerList", MSGNEW("GameEngineSubsystem") PlayerList(), nullptr);
		initSubsystem(TheRecorder,"TheRecorder", createRecorder(), nullptr);
		initSubsystem(TheRadar,"TheRadar", createRadar(TheGlobalData->m_headless), nullptr);
		initSubsystem(TheVictoryConditions,"TheVictoryConditions", createVictoryConditions(), nullptr);



	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheVictoryConditions = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		AsciiString fname;
		fname.format("Data\\%s\\CommandMap", GetRegistryLanguage().str());
		// GeneralsX @bugfix Android port 09/09/2026 Only pass the per-language command map
		// when it is actually there. initSubsystem() loads it with loadFileDirectory(), which
		// throws on reading zero files, and Data\<language>\ exists only for the SKUs EA
		// shipped -- so an unofficial language died here during startup. The real command map
		// is the second path, Data\INI\CommandMap; the language one is an optional override.
		AsciiString fnameWithExt = fname;
		fnameWithExt.concat(".ini");
		const Bool haveLanguageCommandMap = TheFileSystem->doesFileExist(fnameWithExt.str());
		initSubsystem(TheMetaMap,"TheMetaMap", MSGNEW("GameEngineSubsystem") MetaMap(), nullptr,
			haveLanguageCommandMap ? fname.str() : nullptr, "Data\\INI\\CommandMap");

#if defined(RTS_DEBUG)
		ini.loadFileDirectory("Data\\INI\\CommandMapDebug", INI_LOAD_MULTIFILE, nullptr);
#endif

#if defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
		ini.loadFileDirectory("Data\\INI\\CommandMapDemo", INI_LOAD_MULTIFILE, nullptr);
#endif

		TheMetaMap->generateMetaMap();
		TheMetaMap->verifyMetaMap();


		initSubsystem(TheActionManager,"TheActionManager", MSGNEW("GameEngineSubsystem") ActionManager(), nullptr);
		//initSubsystem((CComObject<WebBrowser> *)TheWebBrowser,"(CComObject<WebBrowser> *)TheWebBrowser", (CComObject<WebBrowser> *)createWebBrowser(), nullptr);
		initSubsystem(TheGameStateMap,"TheGameStateMap", MSGNEW("GameEngineSubsystem") GameStateMap, nullptr );
		initSubsystem(TheGameState,"TheGameState", MSGNEW("GameEngineSubsystem") GameState, nullptr );

		// Create the interface for sending game results
		initSubsystem(TheGameResultsQueue,"TheGameResultsQueue", GameResultsInterface::createNewGameResultsInterface(), nullptr);


	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheGameResultsQueue = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		xferCRC.close();
		TheWritableGlobalData->m_iniCRC = xferCRC.getCRC();
		DEBUG_LOG(("INI CRC is 0x%8.8X", TheGlobalData->m_iniCRC));

		TheSubsystemList->postProcessLoadAll();

		// GeneralsX @bugfix Copilot 11/05/2026 Prevent uncapped render when FPS limiter is enabled but no valid limit value was loaded.
		if (TheGlobalData->m_useFpsLimit && TheGlobalData->m_framesPerSecondLimit <= 0)
		{
			TheWritableGlobalData->m_framesPerSecondLimit = BaseFps;
		}

		TheFramePacer->setFramesPerSecondLimit(TheGlobalData->m_framesPerSecondLimit);

		TheAudio->setOn(TheGlobalData->m_audioOn && TheGlobalData->m_musicOn, AudioAffect_Music);
		TheAudio->setOn(TheGlobalData->m_audioOn && TheGlobalData->m_soundsOn, AudioAffect_Sound);
		TheAudio->setOn(TheGlobalData->m_audioOn && TheGlobalData->m_sounds3DOn, AudioAffect_Sound3D);
		TheAudio->setOn(TheGlobalData->m_audioOn && TheGlobalData->m_speechOn, AudioAffect_Speech);

		// We're not in a network game yet, so set the network singleton to nullptr.
		TheNetwork = nullptr;

		//Create a default ini file for options if it doesn't already exist.
		//OptionPreferences prefs( TRUE );

		// If we turn m_quitting to FALSE here, then we throw away any requests to quit that
		// took place during loading. :-\ - jkmcd
		// If this really needs to take place, please make sure that pressing cancel on the audio
		// load music dialog will still cause the game to quit.
		// m_quitting = FALSE;

		// initialize the MapCache
		TheMapCache = MSGNEW("GameEngineSubsystem") MapCache;
		TheMapCache->updateCache();


	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheMapCache->updateCache = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		if (TheGlobalData->m_buildMapCache)
		{
			// just quit, since the map cache has already updated
			//populateMapListbox(nullptr, true, true);
			m_quitting = TRUE;
		}

		// load the initial shell screen
		TheShell->push( "Menus/MainMenu.wnd" );

		// This allows us to run a map from the command line
		if (TheGlobalData->m_initialFile.isEmpty() == FALSE)
		{
			AsciiString fname = TheGlobalData->m_initialFile;
			fname.toLower();

			if (fname.endsWithNoCase(".map"))
			{
				TheWritableGlobalData->m_shellMapOn = FALSE;
				TheWritableGlobalData->m_playIntro = FALSE;
				TheWritableGlobalData->m_pendingFile = TheGlobalData->m_initialFile;

				// shutdown the top, but do not pop it off the stack
	//			TheShell->hideShell();

				// send a message to the logic for a new game
				GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_NEW_GAME );
				msg->appendIntegerArgument(GAME_SINGLE_PLAYER);
				msg->appendIntegerArgument(DIFFICULTY_NORMAL);
				msg->appendIntegerArgument(0);
				InitRandom(0);
			}
		}

		//
		if (TheMapCache && TheGlobalData->m_shellMapOn)
		{
			AsciiString lowerName = TheGlobalData->m_shellMapName;
			lowerName.toLower();

			MapCache::const_iterator it = TheMapCache->find(lowerName);
			if (it == TheMapCache->end())
			{
				TheWritableGlobalData->m_shellMapOn = FALSE;
			}
		}

		if(!TheGlobalData->m_playIntro)
			TheWritableGlobalData->m_afterIntro = TRUE;

	}
	catch (ErrorCode ec)
	{
		if (ec == ERROR_INVALID_D3D)
		{
			RELEASE_CRASHLOCALIZED("ERROR:D3DFailurePrompt", "ERROR:D3DFailureMessage");
		}
	}
	// GeneralsX @bugfix Android port 12/07/2026 - INIException has no
	// user-defined copy constructor, so catching it BY VALUE shallow-copies
	// the mFailureMessage pointer; both the local copy and the original
	// propagating exception object then delete[] the same pointer when
	// their destructors run, a double-free that corrupts the heap right as
	// we're reporting an INI parse error. Catch by const reference instead
	// (only ever read here) so no copy happens at all.
	catch (const INIException& e)
	{
		if (e.mFailureMessage)
			RELEASE_CRASH((e.mFailureMessage));
		else
			RELEASE_CRASH(("Uncaught Exception during initialization."));

	}
	catch (...)
	{
		RELEASE_CRASH(("Uncaught Exception during initialization."));
	}

	if(!TheGlobalData->m_playIntro)
		TheWritableGlobalData->m_afterIntro = TRUE;

	resetSubsystems();

	HideControlBar();
}

/** -----------------------------------------------------------------------------------------------
	* Reset all necessary parts of the game engine to be ready to accept new game data
	*/
void GameEngine::reset()
{

	WindowLayout *background = TheWindowManager->winCreateLayout("Menus/BlankWindow.wnd");
	DEBUG_ASSERTCRASH(background,("We Couldn't Load Menus/BlankWindow.wnd"));
	background->hide(FALSE);
	background->bringForward();
	background->getFirstWindow()->winClearStatus(WIN_STATUS_IMAGE);
	Bool deleteNetwork = false;
	if (TheGameLogic->isInMultiplayerGame())
		deleteNetwork = true;

	resetSubsystems();

	if (deleteNetwork)
	{
		DEBUG_ASSERTCRASH(TheNetwork, ("Deleting null TheNetwork!"));
		delete TheNetwork;
		TheNetwork = nullptr;
	}
	if(background)
	{
		background->destroyWindows();
		deleteInstance(background);
		background = nullptr;
	}
}

/// -----------------------------------------------------------------------------------------------
void GameEngine::resetSubsystems()
{
	// TheSuperHackers @fix xezon 09/06/2025 Reset GameLogic first to purge all world objects early.
	// This avoids potentially catastrophic issues when objects and subsystems have cross dependencies.
	TheGameLogic->reset();

	TheSubsystemList->resetAll();
}

/// -----------------------------------------------------------------------------------------------
Bool GameEngine::canUpdateGameLogic(UnsignedInt logicTimeQueryFlags)
{
	// This updates the paused game status of the game logic.
	TheGameLogic->preUpdate();

	TheFramePacer->setTimeFrozen(isTimeFrozen());
	TheFramePacer->setGameHalted(isGameHalted());

	if (TheNetwork != nullptr)
	{
		return canUpdateNetworkGameLogic();
	}
	else
	{
		return canUpdateRegularGameLogic(logicTimeQueryFlags);
	}
}

/// -----------------------------------------------------------------------------------------------
Bool GameEngine::canUpdateNetworkGameLogic()
{
	DEBUG_ASSERTCRASH(TheNetwork != nullptr, ("TheNetwork is null"));

	if (TheNetwork->isFrameDataReady())
	{
		// Important: The Network is definitely no longer stalling.
		TheFramePacer->setGameHalted(false);

		return true;
	}

	return false;
}

/// -----------------------------------------------------------------------------------------------
Bool GameEngine::canUpdateRegularGameLogic(UnsignedInt logicTimeQueryFlags)
{
	const Int logicTimeScaleFps = TheFramePacer->getActualLogicTimeScaleFps(logicTimeQueryFlags);
	const Int maxRenderFps = TheFramePacer->getActualFramesPerSecondLimit();

#if defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
	const Bool useFastMode = TheGlobalData->m_TiVOFastMode;
#else	//always allow this cheat key if we're in a replay game.
	const Bool useFastMode = TheGlobalData->m_TiVOFastMode && TheGameLogic->isInReplayGame();
#endif

	if (useFastMode || logicTimeScaleFps >= maxRenderFps)
	{
		// Logic time scale is uncapped or larger equal Render FPS. Update straight away.
		return true;
	}
	else
	{
		// TheSuperHackers @tweak xezon 06/08/2025
		// The logic time step is now decoupled from the render update.
		const Real targetFrameTime = 1.0f / logicTimeScaleFps;
		m_logicTimeAccumulator += min(TheFramePacer->getUpdateTime(), targetFrameTime);

		if (m_logicTimeAccumulator >= targetFrameTime)
		{
			m_logicTimeAccumulator -= targetFrameTime;
			return true;
		}
	}

	return false;
}

/// -----------------------------------------------------------------------------------------------
DECLARE_PERF_TIMER(GameEngine_update)

/** -----------------------------------------------------------------------------------------------
 * Update the game engine by updating the GameClient and GameLogic singletons.
 */
// GeneralsX @perf Android port 31/07/2026 lightweight per-subsystem
// frame-phase timing for GameEngine::update(), gated by the GX_PERF opt-in
// (gx_perf.txt marker / GX_PERF env var, off by default -- gx_trace.txt/
// GX_TRACE also enables it, see GXTrace.h). A dedicated flag rather than
// reusing GX_TRACE outright: a real multi-minute session with full tracing
// on produced a >70 MB stderr log, and the in-app log export's head+tail
// cap then elided the entire middle of the session -- exactly where the
// [GX-PERF] samples that matter live. gx_perf.txt alone gets just this
// ~1-line-per-second summary, cheap enough to leave on for a whole session.
// DXVK's own HUD counters (already logged elsewhere as "DXVK_HUD: ...")
// showed near-zero GPU wait/submission cost on Mali-G76 real-device runs
// -- syncs=0 almost always, low draw/submit/barrier counts -- so whatever
// is capping FPS on that device has to be on the CPU side of this
// function, not in DXVK/Vulkan submission. This times each subsystem
// UPDATE() call and flushes an aggregate once a second, so a real-device
// log can show which subsystem actually owns the frame budget instead of
// guessing. Cost when disabled is the single bool check below -- none of
// the now()/duration calls execute.
static void gxTraceEngineUpdatePhase(
	double radarUs, double audioUs, double clientUs,
	double networkUs, double logicUs, double stepUs)
{
	static std::chrono::steady_clock::time_point s_windowStart = std::chrono::steady_clock::now();
	static double s_radarUs = 0, s_audioUs = 0, s_clientUs = 0,
		s_networkUs = 0, s_logicUs = 0, s_stepUs = 0;
	static int s_frames = 0;

	s_radarUs += radarUs;
	s_audioUs += audioUs;
	s_clientUs += clientUs;
	s_networkUs += networkUs;
	s_logicUs += logicUs;
	s_stepUs += stepUs;
	++s_frames;

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	double elapsedUs = std::chrono::duration<double, std::micro>(now - s_windowStart).count();
	if (elapsedUs >= 1'000'000.0 && s_frames > 0)
	{
		GX_PERF_TRACE("[GX-PERF] frames=%d avgFrameMs=%.2f radar=%.2fms audio=%.2fms client=%.2fms network=%.2fms logic=%.2fms step=%.2fms\n",
			s_frames,
			(elapsedUs / 1000.0) / s_frames,
			s_radarUs / 1000.0 / s_frames,
			s_audioUs / 1000.0 / s_frames,
			s_clientUs / 1000.0 / s_frames,
			s_networkUs / 1000.0 / s_frames,
			s_logicUs / 1000.0 / s_frames,
			s_stepUs / 1000.0 / s_frames);

		s_windowStart = now;
		s_radarUs = s_audioUs = s_clientUs = s_networkUs = s_logicUs = s_stepUs = 0;
		s_frames = 0;
	}
}

void GameEngine::update()
{
	USE_PERF_TIMER(GameEngine_update)
	{
		// GeneralsX @bugfix Android port 31/07/2026 gated on isPerfEnabled(),
		// not isEnabled() -- a gx_perf.txt-only session should still get
		// this cheap ~1-line-per-second summary without also paying for
		// (and drowning the exported log in) full GX_TRACE font/UI tracing.
		const bool gxPerfTrace = GXTrace::isPerfEnabled();
		std::chrono::steady_clock::time_point gxT0, gxT1, gxT2, gxT3, gxT4, gxT5;

		{
			// VERIFY CRC needs to be in this code block.  Please to not pull TheGameLogic->update() inside this block.
			VERIFY_CRC

			if (gxPerfTrace) gxT0 = std::chrono::steady_clock::now();
			TheRadar->UPDATE();
			if (gxPerfTrace) gxT1 = std::chrono::steady_clock::now();

			/// @todo Move audio init, update, etc, into GameClient update

			TheAudio->UPDATE();
			if (gxPerfTrace) gxT2 = std::chrono::steady_clock::now();
			TheGameClient->UPDATE();
			if (gxPerfTrace) gxT3 = std::chrono::steady_clock::now();
			TheMessageStream->propagateMessages();

			if (TheNetwork != nullptr)
			{
				TheNetwork->UPDATE();
			}
			if (gxPerfTrace) gxT4 = std::chrono::steady_clock::now();

			// GeneralsX @bugfix Android port 07/11/2026 - ported from upstream GeneralsOnline: process a deferred TearDownGeneralsOnline() request
			if (g_bTearDownGeneralsOnlineRequested) // delayed tear down
			{
				g_bTearDownGeneralsOnlineRequested = false;

				NGMP_OnlineServicesManager::DestroyInstance();
			}

			if (NGMP_OnlineServicesManager::GetInstance() != nullptr)
			{
				NGMP_OnlineServicesManager::GetInstance()->Tick();
			}
		}

		// TheSuperHackers @info Ignores frozen time because the script engine needs updating in the logic update regardless.
		double logicUs = 0.0, stepUs = 0.0;
		if (canUpdateGameLogic(FramePacer::IgnoreFrozenTime))
		{
			TheGameLogic->UPDATE();
			if (gxPerfTrace) gxT5 = std::chrono::steady_clock::now();

			if (!TheFramePacer->isTimeFrozen())
			{
				TheGameClient->step();
			}

			if (gxPerfTrace)
			{
				std::chrono::steady_clock::time_point gxT6 = std::chrono::steady_clock::now();
				logicUs = std::chrono::duration<double, std::micro>(gxT5 - gxT4).count();
				stepUs = std::chrono::duration<double, std::micro>(gxT6 - gxT5).count();
			}
		}

		if (gxPerfTrace)
		{
			// gxT4->gxT5 also covers propagateMessages()/online-services tick (unlabeled,
			// folded into "logic" below since they're consistently cheap message-queue work).
			gxTraceEngineUpdatePhase(
				std::chrono::duration<double, std::micro>(gxT1 - gxT0).count(),
				std::chrono::duration<double, std::micro>(gxT2 - gxT1).count(),
				std::chrono::duration<double, std::micro>(gxT3 - gxT2).count(),
				std::chrono::duration<double, std::micro>(gxT4 - gxT3).count(),
				logicUs, stepUs);
		}
	}
}

// Horrible reference, but we really, really need to know if we are windowed.
extern bool DX8Wrapper_IsWindowed;
extern HWND ApplicationHWnd;

// GeneralsX @bugfix Android port 18/07/2026 Survive exceptions escaping
// GameEngine::update() instead of killing the session (issue #2). The
// original engine turns ANY exception reaching execute()'s per-frame catch
// into RELEASE_CRASH -- the "Technical Difficulties..." box -- which on the
// affected devices fires right as the shell menu comes up, from a throw
// whose type doesn't match any catch clause we've enumerated (a per-TU
// anonymous-enum identity per the '$_0' typeinfo, see the catch chain
// below). Three earlier issue-#2 fixes (science lookup, CommandButton
// tokens, Eva events) were all the same pattern: a runtime lookup throwing
// over one unrecognized data token that the game could perfectly well keep
// running without. Rather than keep fixing one thrower per tester round,
// treat a per-frame exception like those fixes treat a bad token: log it,
// abandon the rest of that frame, and keep the session alive. A generous
// cap guards against a session that's genuinely wedged (throwing every
// frame forever burns the log and the battery for nothing); below it, a
// glitched frame beats a dead game.
static Bool recoverFromUpdateException()
{
	static Int total = 0;
	++total;
	if (total > 2000)
		return FALSE;
	if (total <= 20 || (total % 200) == 0)
	{
		fprintf(stderr, "[GX-RECOVER] GameEngine::update threw (occurrence %d) -- skipping rest of frame, continuing\n", total);
		fflush(stderr);
	}
	return TRUE;
}

/** -----------------------------------------------------------------------------------------------
 * The "main loop" of the game engine. It will not return until the game exits.
 */
void GameEngine::execute()
{
	// pretty basic for now
	while( !m_quitting )
	{
		executeSingleFrame();
	}
}

// GeneralsX @feature XR port Phase 1.0 - one iteration of the main loop,
// extracted from execute() so the XR frame loop can drive the game
// synchronously (single thread, single GL context). Returns FALSE once
// the game wants to quit. execute() above is byte-identical in behavior.
Bool GameEngine::executeSingleFrame()
{
#if defined(RTS_DEBUG)
	static DWORD startTime = timeGetTime() / 1000;
#endif

	//if (TheGlobalData->m_vTune)
	{
#ifdef PERF_TIMERS
		PerfGather::resetAll();
#endif
	}

	{

#if defined(RTS_DEBUG)
		{
			// enter only if in benchmark mode
			if (TheGlobalData->m_benchmarkTimer > 0)
			{
				DWORD currentTime = timeGetTime() / 1000;
				if (TheGlobalData->m_benchmarkTimer < currentTime - startTime)
				{
					if (TheGameLogic->isInGame())
					{
						if (TheRecorder->getMode() == RECORDERMODETYPE_RECORD)
						{
							TheRecorder->stopRecording();
						}
						TheGameLogic->clearGameData();
					}
					TheGameEngine->setQuitting(TRUE);
				}
			}
		}
#endif

		{
			try
			{
				// compute a frame
				update();
			}
			// GeneralsX @bugfix Android port 12/07/2026 - Catch by const reference,
			// see the same fix earlier in this file for why (INIException lacks a
			// copy constructor, so catching by value double-frees mFailureMessage).
			catch (const INIException& e)
			{
				fprintf(stderr, "[GX-RELEASECRASH] GameEngine::update threw INIException: %s\n",
					e.mFailureMessage ? e.mFailureMessage : "<no message>");
				fflush(stderr);
				if (!recoverFromUpdateException())
				{
					// Release CRASH doesn't return, so don't worry about executing additional code.
					if (e.mFailureMessage)
						RELEASE_CRASH((e.mFailureMessage));
					else
						RELEASE_CRASH(("Uncaught Exception in GameEngine::update"));
				}
			}
			catch (...)
			{
				// try to save info off
				try
				{
					if (TheRecorder && TheRecorder->getMode() == RECORDERMODETYPE_RECORD && TheRecorder->isMultiplayer())
						TheRecorder->cleanUpReplayFile();
				}
				catch (...)
				{
				}
				// GeneralsX @bugfix Android port 16/07/2026 The bare catch(...)
				// discards which exception actually escaped update() -- on Android
				// (issue #2) that leaves the "Technical Difficulties..." box with no
				// clue in the log. Re-throw once to identify the common engine
				// throw types before falling through to RELEASE_CRASH.
				try
				{
					throw;
				}
				catch (ErrorCode ec)
				{
					fprintf(stderr, "[GX-RELEASECRASH] GameEngine::update threw ErrorCode=%d\n", (int)ec);
					fflush(stderr);
				}
				// GeneralsX @bugfix Android port 16/07/2026 The two families that
				// escape here as "unrecognized" are the anonymous INI_* enum
				// (INI_INVALID_DATA etc., all == ERROR_BAD_INI) and the named
				// SaveCode enum (SC_INVALID_DATA etc.). Catch both by their real
				// types so the log says which family threw -- INI-style data
				// validation vs save/map load -- to pin down the culprit path
				// during shell/menu load (issue #2).
				catch (SaveCode sc)
				{
					fprintf(stderr, "[GX-RELEASECRASH] GameEngine::update threw SaveCode=%d\n", (int)sc);
					fflush(stderr);
				}
				catch (decltype(INI_INVALID_DATA) ie)
				{
					fprintf(stderr, "[GX-RELEASECRASH] GameEngine::update threw INI-family anonymous enum=%d\n", (int)ie);
					fflush(stderr);
				}
				// GeneralsX @bugfix Android port 16/07/2026 Build 168 ruled out
				// SaveCode and the INI anonymous enum -- still "unrecognized". The
				// only remaining engine runtime throw type that lands here is the
				// XferStatus enum (XFER_* codes, Xfer.h) from the save/load/xfer
				// serialization system -- consistent with the 'GUI:RecentSave'
				// window reading a save whose data this build can't parse. Log the
				// exact XFER_* value so we know which failure (e.g. XFER_UNKNOWN_STRING
				// == an unrecognized enum-name in the saved data, same pattern as the
				// earlier menu-load fixes).
				catch (XferStatus xs)
				{
					fprintf(stderr, "[GX-RELEASECRASH] GameEngine::update threw XferStatus=%d\n", (int)xs);
					fflush(stderr);
				}
				// GeneralsX @bugfix Android port 16/07/2026 Also cover bare
				// primitive throws (some engine paths throw a literal int, and
				// a stray C-string throw is possible) so those don't masquerade
				// as "unrecognized" too (issue #2).
				catch (int n)
				{
					fprintf(stderr, "[GX-RELEASECRASH] GameEngine::update threw int=%d\n", n);
					fflush(stderr);
				}
				catch (const char* s)
				{
					fprintf(stderr, "[GX-RELEASECRASH] GameEngine::update threw const char*='%s'\n", s ? s : "<null>");
					fflush(stderr);
				}
				catch (const std::exception& se)
				{
					fprintf(stderr, "[GX-RELEASECRASH] GameEngine::update threw std::exception what='%s'\n", se.what());
					fflush(stderr);
				}
				catch (...)
				{
					// GeneralsX @bugfix Android port 16/07/2026 Every explicit catch
					// above (ErrorCode/SaveCode/INI-anon/XferStatus/int/const char*/
					// std::exception) has now been ruled out across builds 167-170 on
					// two different devices (Adreno 750, Exynos Xclipse), including a
					// confirmed-fresh install of this exact build (issue #2). Rather
					// than keep guessing engine types one at a time, ask the C++ ABI
					// runtime directly what's actually in flight -- __cxa_current_
					// exception_type() reads it from the exception object's own
					// typeinfo, independent of any catch clause matching. Android/
					// bionic uses libc++abi, which supports this. If even this comes
					// back null/garbage, that itself is strong evidence the exception
					// object (or the unwind machinery) is memory-corrupted, not just
					// an engine type we haven't enumerated.
#if !defined(_MSC_VER)
					std::type_info* ti = abi::__cxa_current_exception_type();
					if (ti != nullptr)
					{
						int demangleStatus = 0;
						char* demangled = abi::__cxa_demangle(ti->name(), nullptr, nullptr, &demangleStatus);
						fprintf(stderr, "[GX-RELEASECRASH] GameEngine::update threw type='%s' (mangled='%s', demangle_status=%d)\n",
							demangled ? demangled : "<demangle failed>", ti->name(), demangleStatus);
						if (demangled)
							free(demangled);
					}
					else
					{
						fprintf(stderr, "[GX-RELEASECRASH] GameEngine::update threw unrecognized exception type (__cxa_current_exception_type returned null)\n");
					}
#else
					fprintf(stderr, "[GX-RELEASECRASH] GameEngine::update threw unrecognized exception type\n");
#endif
					fflush(stderr);
				}
				if (!recoverFromUpdateException())
					RELEASE_CRASH(("Uncaught Exception in GameEngine::update"));
			}
		}

		TheFramePacer->update();

		// NOTE: TheDisplay->draw() is called via TheGameClient->UPDATE() above.
		// GameClient::update() dispatches TheDisplay->DRAW() each frame.
		// Do NOT add an extra draw() call here - it would double-present per frame.
	}

#ifdef PERF_TIMERS
	if (!m_quitting && TheGameLogic->isInGame() && !TheGameLogic->isInShellGame() && !TheGameLogic->isGamePaused())
	{
		PerfGather::dumpAll(TheGameLogic->getFrame());
		PerfGather::displayGraph(TheGameLogic->getFrame());
		PerfGather::resetAll();
	}
#endif

	return !m_quitting;
}

/** -----------------------------------------------------------------------------------------------
	* Factory for the message stream
	*/
MessageStream *GameEngine::createMessageStream()
{
	// if you change this update the tools that use the engine systems
	// like GUIEdit, it creates a message stream to run in "test" mode
	return MSGNEW("GameEngineSubsystem") MessageStream;
}

//-------------------------------------------------------------------------------------------------
FileSystem *GameEngine::createFileSystem()
{
	return MSGNEW("GameEngineSubsystem") FileSystem;
}

//-------------------------------------------------------------------------------------------------
Bool GameEngine::isMultiplayerSession()
{
	return TheRecorder->isMultiplayer();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
#define CONVERT_EXEC1	"..\\Build\\nvdxt -list buildDDS.txt -dxt5 -full -outdir Art\\Textures > buildDDS.out"

void updateTGAtoDDS()
{
	// Here's the scoop. We're going to traverse through all of the files in the Art\Textures folder
	// and determine if there are any .tga files that are newer than associated .dds files. If there
	// are, then we will re-run the compression tool on them.

	File *fp = TheLocalFileSystem->openFile("buildDDS.txt", File::WRITE | File::CREATE | File::TRUNCATE | File::TEXT);
	if (!fp) {
		return;
	}

	FilenameList files;
	TheLocalFileSystem->getFileListInDirectory("Art\\Textures\\", "", "*.tga", files, TRUE);
	FilenameList::iterator it;
	for (it = files.begin(); it != files.end(); ++it) {
		AsciiString filenameTGA = *it;
		AsciiString filenameDDS = *it;
		FileInfo infoTGA;
		TheLocalFileSystem->getFileInfo(filenameTGA, &infoTGA);

		// skip the water textures, since they need to be NOT compressed
		filenameTGA.toLower();
		if (strstr(filenameTGA.str(), "caust"))
		{
			continue;
		}
		// and the recolored stuff.
		if (strstr(filenameTGA.str(), "zhca"))
		{
			continue;
		}

		// replace tga with dds
		filenameDDS.truncateBy(3); // tga
		filenameDDS.concat("dds");

		Bool needsToBeUpdated = FALSE;
		FileInfo infoDDS;
		if (TheFileSystem->doesFileExist(filenameDDS.str())) {
			TheFileSystem->getFileInfo(filenameDDS, &infoDDS);
			if (infoTGA.timestampHigh > infoDDS.timestampHigh ||
					(infoTGA.timestampHigh == infoDDS.timestampHigh &&
					 infoTGA.timestampLow > infoDDS.timestampLow)) {
				needsToBeUpdated = TRUE;
			}
		} else {
			needsToBeUpdated = TRUE;
		}

		if (!needsToBeUpdated) {
			continue;
		}

		filenameTGA.concat("\n");
		fp->write(filenameTGA.str(), filenameTGA.getLength());
	}

	fp->close();

// TheSuperHackers @build fighter19 11/02/2026 Windows-only texture conversion
#ifdef _WIN32
	system(CONVERT_EXEC1);
#else
	// Linux: TGA to DDS conversion not needed (or handle differently)
#endif
}

//-------------------------------------------------------------------------------------------------
// System things

// If we're using the Wide character version of MessageBox, then there's no additional
// processing necessary. Please note that this is a sleazy way to get this information,
// but pending a better one, this'll have to do.
// TheSuperHackers @build fighter19 11/02/2026 MessageBox detection (Windows-only)
#ifdef _WIN32
extern const Bool TheSystemIsUnicode = (((void*) (::MessageBox)) == ((void*) (::MessageBoxW)));
#else
extern const Bool TheSystemIsUnicode = true;  // Linux: Always Unicode (UTF-8)
#endif
