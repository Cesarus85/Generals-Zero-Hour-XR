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

// GeneralsX @bugfix Android port 12/07/2026
//
// Shared GeneralsOnline session store + HTTP auth calls, extracted from
// GeneralsOnlineActivity so the GAME activity can refresh the session too.
//
// Why: the native game reads a static session_token from the marker file
// written at sign-in time. GeneralsOnline session tokens expire server-side
// after a few hours, so a player who signed in earlier in the day got
// "Could not connect to GeneralsOnline (HTTP response code said error)"
// (WebSocket + MOTD both rejected 401, confirmed by device log) even though
// their sign-in "looked" fine. The launcher already caches a refresh_token
// and knows how to trade it for a fresh session (LoginWithToken) -- the fix
// is simply to do that on every game launch, before native code reads the
// marker file, which GeneralsZHActivity.onCreate() now does via
// refreshSessionAsync().

package com.generalsx.zerohour;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import org.json.JSONObject;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

final class GeneralsOnlineSession {

    private static final String TAG = "GeneralsOnlineSession";

    static final String API_BASE = "https://api.playgenerals.online/env/prod/contract/1/";

    // GeneralsX @bugfix Android port 08/30/2026 Mirrors Network_
    // UseAlternativeEndpoint() in the native client (GeneralsOnline_Settings.h/
    // OnlineServices_Init.cpp), which exists specifically because some
    // players can't reach api.playgenerals.online directly (ISP/DNS-level
    // filtering is the documented reason for that setting existing at all)
    // even though the login WEBSITE (www.playgenerals.online, a different
    // host) works fine for them. The native client only exposes this as a
    // manual settings toggle; this launcher has no settings screen for it
    // yet, so postJson() below falls back to it automatically whenever the
    // primary host doesn't answer.
    static final String API_BASE_ALT = "https://api-ru.playgenerals.online/env/prod/contract/1/";

    static final String PREFS_NAME = "generalsonline_session";
    static final String PREF_SESSION_TOKEN = "session_token";
    static final String PREF_REFRESH_TOKEN = "refresh_token";
    static final String PREF_USER_ID = "user_id";
    static final String PREF_DISPLAY_NAME = "display_name";
    static final String PREF_WS_URI = "ws_uri";

    // Native code reads this -- same plain-marker-file convention as
    // gamedata_path.txt (see GeneralsOnline_AndroidGlue.cpp).
    static final String SESSION_MARKER_NAME = "generalsonline_session.txt";

    static class AuthResult {
        int state = -1;
        String sessionToken = "";
        String refreshToken = "";
        long userId = -1;
        String displayName = "";
        String wsUri = "";
    }

    // GeneralsX @bugfix Android port 08/30/2026 A user reported the network-
    // error screen with no way to see WHY -- no adb, no logcat access, just
    // a generic "check your connection" string. This captures the actual
    // failure (host tried, HTTP status + a body snippet, or the exception)
    // from the most recent postJson() call so the caller can put it right
    // on screen. Single mutable field is fine: this launcher only ever runs
    // one login/refresh attempt at a time.
    static volatile String lastNetworkErrorDetail = "";

    private GeneralsOnlineSession() {
    }

    // Runs on a background thread.
    //
    // GeneralsX @bugfix Android port 08/30/2026 Falls back to API_BASE_ALT
    // when the primary host is unreachable or rejects the request at the
    // transport level. Confirmed live (curl) that api.playgenerals.online
    // can answer a CheckLogin call with HTTP 403 while still returning a
    // body shaped exactly like a normal AuthResponse
    // ({"result":2,"session_token":"",...}) -- postJsonOnce() below now
    // treats any non-2xx status as a transport failure (null) rather than
    // trusting that body's "result" field, so a rejected/blocked request
    // can no longer be misread as "the user's login attempt failed" (it
    // previously was: state=2 is FAILED, same as a real failed login,
    // and the UI has no way to tell the two apart). That alone fixed the
    // mislabeling; this fallback additionally gives blocked requests a
    // second real chance via the alternate host before giving up.
    static AuthResult postJson(String endpoint, JSONObject body, String bearerToken) {
        StringBuilder errors = new StringBuilder();
        AuthResult result = postJsonOnce(API_BASE, endpoint, body, bearerToken, errors);
        if (result != null) {
            lastNetworkErrorDetail = "";
            return result;
        }
        Log.w(TAG, "primary API endpoint (" + API_BASE + ") unreachable or rejected the request; retrying via alternate endpoint");
        result = postJsonOnce(API_BASE_ALT, endpoint, body, bearerToken, errors);
        lastNetworkErrorDetail = errors.toString().trim();
        if (result != null) {
            lastNetworkErrorDetail = "";
        } else {
            Log.w(TAG, "both API endpoints failed for " + endpoint + ": " + lastNetworkErrorDetail);
        }
        return result;
    }

    private static AuthResult postJsonOnce(String base, String endpoint, JSONObject body, String bearerToken, StringBuilder errorOut) {
        HttpURLConnection conn = null;
        try {
            URL url = new URL(base + endpoint);
            conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("POST");
            conn.setRequestProperty("Content-Type", "application/json");
            if (bearerToken != null) {
                conn.setRequestProperty("Authorization", "Bearer " + bearerToken);
            }
            conn.setConnectTimeout(10000);
            conn.setReadTimeout(10000);
            conn.setDoOutput(true);

            try (OutputStream os = conn.getOutputStream()) {
                os.write(body.toString().getBytes(StandardCharsets.UTF_8));
            }

            int status = conn.getResponseCode();
            if (status < 200 || status >= 300) {
                // See the class-level comment on postJson(): a non-2xx
                // response is a transport/policy rejection, not an answer
                // about the login attempt itself, even when its body
                // happens to parse as a valid-looking AuthResponse.
                errorOut.append(hostOf(base)).append(": HTTP ").append(status);
                String snippet = readSnippet(conn.getErrorStream());
                if (!snippet.isEmpty()) {
                    errorOut.append(" ").append(snippet);
                }
                errorOut.append("; ");
                return null;
            }
            java.io.InputStream in = conn.getInputStream();
            if (in == null) {
                errorOut.append(hostOf(base)).append(": empty response body; ");
                return null;
            }
            JSONObject json = new JSONObject(readAll(in));

            AuthResult result = new AuthResult();
            result.state = json.optInt("result", -1);
            result.sessionToken = json.optString("session_token", "");
            result.refreshToken = json.optString("refresh_token", "");
            result.userId = json.optLong("user_id", -1);
            result.displayName = json.optString("display_name", "");
            result.wsUri = json.optString("ws_uri", "");
            return result;
        } catch (Exception e) {
            errorOut.append(hostOf(base)).append(": ").append(e.getClass().getSimpleName());
            if (e.getMessage() != null) {
                errorOut.append(": ").append(e.getMessage());
            }
            errorOut.append("; ");
            return null;
        } finally {
            if (conn != null) {
                conn.disconnect();
            }
        }
    }

    private static String hostOf(String base) {
        try {
            return new URL(base).getHost();
        } catch (Exception e) {
            return base;
        }
    }

    // Best-effort, truncated: this is for on-screen diagnostics, not a full
    // dump -- a WAF/proxy error page can be arbitrarily large.
    private static String readSnippet(java.io.InputStream in) {
        if (in == null) {
            return "";
        }
        try {
            String body = readAll(in);
            body = body.replaceAll("\\s+", " ").trim();
            if (body.length() > 200) {
                body = body.substring(0, 200) + "...";
            }
            return body;
        } catch (Exception e) {
            return "";
        }
    }

    private static String readAll(java.io.InputStream in) throws IOException {
        java.io.ByteArrayOutputStream buf = new java.io.ByteArrayOutputStream();
        byte[] chunk = new byte[4096];
        int n;
        while ((n = in.read(chunk)) != -1) {
            buf.write(chunk, 0, n);
        }
        return buf.toString("UTF-8");
    }

    // Runs on a background thread. Mirrors the reference client's
    // GetCredentials()/LoginWithToken silent-reauth branch.
    static AuthResult loginWithToken(String refreshToken) {
        JSONObject body = new JSONObject();
        try {
            body.put("reserved_0", "");
            body.put("reserved_1", "");
            body.put("reserved_2", "");
        } catch (Exception e) {
            return null;
        }
        return postJson("LoginWithToken", body, refreshToken);
    }

    static void saveSession(Context ctx, AuthResult result) {
        ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE).edit()
            .putString(PREF_SESSION_TOKEN, result.sessionToken)
            .putString(PREF_REFRESH_TOKEN, result.refreshToken)
            .putLong(PREF_USER_ID, result.userId)
            .putString(PREF_DISPLAY_NAME, result.displayName)
            .putString(PREF_WS_URI, result.wsUri)
            .apply();

        // Plain marker file for native code -- one "key=value" per line, no
        // secrets beyond what's already only readable by this app's own uid.
        File marker = new File(ctx.getFilesDir(), SESSION_MARKER_NAME);
        try (FileWriter w = new FileWriter(marker, false)) {
            w.write("session_token=" + result.sessionToken + "\n");
            w.write("user_id=" + result.userId + "\n");
            w.write("display_name=" + result.displayName + "\n");
            w.write("ws_uri=" + result.wsUri + "\n");
        } catch (IOException e) {
            // Not fatal: the game will report the connection failure itself.
        }
    }

    static void clearSession(Context ctx) {
        ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE).edit().clear().apply();
        new File(ctx.getFilesDir(), SESSION_MARKER_NAME).delete();
    }

    /**
     * Fire-and-forget session refresh at game launch: if a refresh_token is
     * cached, trade it for a fresh session token and rewrite the marker file
     * BEFORE the player can reach the Online button (one small HTTPS POST vs
     * tens of seconds of engine startup -- the race is theoretical). On any
     * failure the existing marker is left untouched: if the old token is
     * still valid the game works as before, and if it expired the game shows
     * the same connect error it always did (nothing gets worse offline).
     */
    static void refreshSessionAsync(Context appContext) {
        final Context ctx = appContext.getApplicationContext();
        new Thread(() -> {
            SharedPreferences prefs = ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
            String refreshToken = prefs.getString(PREF_REFRESH_TOKEN, null);
            if (refreshToken == null || refreshToken.isEmpty()) {
                Log.i(TAG, "no cached refresh_token; skipping launch-time session refresh");
                return;
            }
            AuthResult result = loginWithToken(refreshToken);
            if (result != null && result.state == 1) {
                saveSession(ctx, result);
                Log.i(TAG, "session refreshed at launch for user " + result.userId);
            } else {
                Log.w(TAG, "launch-time session refresh failed (state="
                    + (result != null ? result.state : "network-error")
                    + "); keeping existing session marker");
            }
        }, "GeneralsOnlineSessionRefresh").start();
    }
}
