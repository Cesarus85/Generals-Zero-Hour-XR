// GeneralsX @feature Android port 11/07/2026 This file is NOT ported from
// upstream -- it's our own glue reading the Android launcher's session
// marker file and kicking off GeneralsOnline login. TheNGMPGame and
// OnKickedFromLobby() used to be stubbed here (upstream defines them in
// WOLGameSetupMenu.cpp, which this port originally skipped); now that the
// real WOLGameSetupMenu.cpp/WOLWelcomeMenu.cpp/etc. are ported too, those
// stubs were deleted to avoid duplicate-symbol link errors.

#include "GameNetwork/GeneralsOnline/NGMP_interfaces.h"
#include "GameNetwork/GeneralsOnline/GeneralsOnline_AndroidGlue.h"
#include "GameNetwork/GeneralsOnline/OnlineServices_Auth.h"
#include "GameNetwork/GameSpyOverlay.h"
#include "GameClient/Shell.h"
#include <cstdlib>
#include <string>

#if defined(__ANDROID__)
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#endif

#if defined(__ANDROID__)
namespace
{
	struct AndroidSession
	{
		std::string sessionToken;
		std::string userId;
		std::string displayName;
		std::string wsUri;
	};

	// Mirrors SDL3Main.cpp's gamedata_path.txt reader: the Android launcher
	// (GeneralsOnlineActivity.java) writes this plain "key=value" marker file
	// straight into getFilesDir() -- the same directory
	// SDL_GetAndroidInternalStoragePath() resolves to -- there is no JNI
	// plumbing involved on either side, just a shared filesystem convention.
	bool ReadAndroidSession(AndroidSession& outSession)
	{
		const char* internalPath = SDL_GetAndroidInternalStoragePath();
		if (internalPath == nullptr)
		{
			fprintf(stderr, "DEBUG-ONLINE: ReadAndroidSession bail -- SDL_GetAndroidInternalStoragePath() returned null\n");
			fflush(stderr);
			return false;
		}

		char markerPath[1024];
		snprintf(markerPath, sizeof(markerPath), "%s/generalsonline_session.txt", internalPath);
		FILE* f = fopen(markerPath, "r");
		if (f == nullptr)
		{
			fprintf(stderr, "DEBUG-ONLINE: ReadAndroidSession bail -- could not open '%s' (not signed in yet?)\n", markerPath);
			fflush(stderr);
			return false;
		}

		char line[2048];
		while (fgets(line, sizeof(line), f) != nullptr)
		{
			size_t len = strlen(line);
			while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			{
				line[--len] = '\0';
			}

			char* eq = strchr(line, '=');
			if (eq == nullptr)
			{
				continue;
			}
			*eq = '\0';
			const char* key = line;
			const char* value = eq + 1;

			if (strcmp(key, "session_token") == 0) outSession.sessionToken = value;
			else if (strcmp(key, "user_id") == 0) outSession.userId = value;
			else if (strcmp(key, "display_name") == 0) outSession.displayName = value;
			else if (strcmp(key, "ws_uri") == 0) outSession.wsUri = value;
		}
		fclose(f);

		bool bValid = !outSession.sessionToken.empty() && !outSession.wsUri.empty();
		fprintf(stderr, "DEBUG-ONLINE: ReadAndroidSession parsed '%s' -- hasToken=%d hasWsUri=%d valid=%d\n",
			markerPath, !outSession.sessionToken.empty(), !outSession.wsUri.empty(), (int)bValid);
		fflush(stderr);
		return bValid;
	}
}
#endif // __ANDROID__

bool TryStartGeneralsOnline()
{
#if defined(__ANDROID__)
	fprintf(stderr, "DEBUG-ONLINE: TryStartGeneralsOnline enter\n");
	fflush(stderr);
	AndroidSession session;
	if (!ReadAndroidSession(session))
	{
		// Not signed in yet -- the player needs to use the GeneralsOnline
		// Account screen in the Settings app first. Fall through to the
		// legacy path (which will show its own "can't connect" message);
		// a friendlier prompt here is a follow-up.
		fprintf(stderr, "DEBUG-ONLINE: TryStartGeneralsOnline -- no valid session, falling through to legacy path\n");
		fflush(stderr);
		return false;
	}
	fprintf(stderr, "DEBUG-ONLINE: TryStartGeneralsOnline -- session found, userId=%s displayName=%s\n",
		session.userId.c_str(), session.displayName.c_str());
	fflush(stderr);

	if (NGMP_OnlineServicesManager::GetInstance() == nullptr)
	{
		NGMP_OnlineServicesManager::CreateInstance();
		NGMP_OnlineServicesManager::GetInstance()->Init();
	}

	// GeneralsX @bugfix Android port 10/07/2026 the launcher already exchanged
	// its device code for a session token before the game even started; that
	// token was being read from the marker file and then silently discarded
	// here, so NGMP_OnlineServices_AuthInterface::IsLoggedIn() stayed false
	// and every authenticated call (GetFriendsList/GetBlockList, fired right
	// after WS connect below) went out with no Authorization header and got
	// rejected 401 by the server (confirmed via a real device log).
	NGMP_OnlineServices_AuthInterface* pAuthInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_AuthInterface>();
	if (pAuthInterface != nullptr)
	{
		int64_t userID = static_cast<int64_t>(std::strtoll(session.userId.c_str(), nullptr, 10));
		pAuthInterface->SetExternalSession(session.sessionToken, userID, session.displayName);

		// GeneralsX @bugfix Android port 11/07/2026 upstream's real login
		// flow (NGMP_OnlineServices_AuthInterface::BeginLogin()) always calls
		// this before reaching the Welcome screen -- it's what actually
		// fetches the MOTD (ProcessMOTD()) that WOLWelcomeMenu.cpp's news
		// listbox reads. Our Android glue bypasses BeginLogin() entirely
		// (the launcher already has a session token), so the MOTD was never
		// fetched and the listbox only ever showed its fallback string.
		// This call doesn't need auth (empty headers) and doesn't block --
		// it races the WebSocket connect below, but a single small MOTD GET
		// is comfortably faster in practice.
		pAuthInterface->GoToDetermineNetworkCaps();
	}

	ClearGSMessageBoxes();
	GSMessageBoxNoButtons(UnicodeString(L"GeneralsOnline"), UnicodeString(L"Connecting..."), false);

	fprintf(stderr, "DEBUG-ONLINE: TryStartGeneralsOnline -- calling OnLogin, wsUri=%s\n", session.wsUri.c_str());
	fflush(stderr);
	NGMP_OnlineServicesManager::GetInstance()->OnLogin(ELoginResult::Success, session.wsUri.c_str(), []()
		{
			// GeneralsX @feature Android port 11/07/2026 the real upstream
			// WOLWelcomeMenu (ported from GeneralsOnlineDevelopmentTeam/
			// GameClient) replaces our earlier hand-rolled GeneralsOnlineHome
			// screen -- same entry point upstream's own MainMenu -> Online
			// button flow uses.
			fprintf(stderr, "DEBUG-ONLINE: OnLogin callback fired, pushing WOLWelcomeMenu.wnd\n");
			fflush(stderr);
			ClearGSMessageBoxes();
			TheShell->push("Menus/WOLWelcomeMenu.wnd");
		});
	fprintf(stderr, "DEBUG-ONLINE: TryStartGeneralsOnline -- OnLogin call returned, exiting\n");
	fflush(stderr);

	return true;
#else
	return false;
#endif
}
