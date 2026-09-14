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

// GeneralsX @feature Android port 10/07/2026
//
// Account login for GeneralsOnline (playgenerals.online / TheSuperHackers
// GeneralsOnlineServices), the actively-maintained GameSpy replacement for
// Zero Hour multiplayer -- see docs/port/... for how this was chosen over
// Revora/CnC-Online (which retired Generals/ZH support and redirects here).
//
// GeneralsX @bugfix Android port 10/07/2026 First cut of this screen made
// the user manually copy a code FROM the browser INTO the app -- wrong.
// The real client (github.com/GeneralsOnlineDevelopmentTeam/GameClient,
// OnlineServices_Auth.cpp: NGMP_OnlineServices_AuthInterface::BeginLogin)
// generates the code itself, embeds it directly in the URL it opens
// (playgenerals.online/login/?gamecode=<code>), and polls CheckLogin with
// that same code -- the user never sees or types the code at all, only
// picks Steam/Discord/GameReplays on the site and comes back. Ported that
// exact flow here, including the refresh_token cache so repeat logins skip
// the browser entirely (LoginWithToken).

package com.generalsx.zerohour;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import com.google.android.material.button.MaterialButton;

import org.json.JSONObject;

import java.security.SecureRandom;

public class GeneralsOnlineActivity extends Activity {

    // GeneralsX @bugfix Android port 12/07/2026 session store + HTTP auth
    // calls moved to GeneralsOnlineSession so GeneralsZHActivity can refresh
    // the session token at game launch (they expire server-side within
    // hours; a stale marker file made the game's Online button fail with
    // "HTTP response code said error"/401 despite a "valid" local session).
    // GeneralsX @bugfix Android port 10/07/2026 the reference client
    // (OnlineServices_Auth.cpp BeginLogin) always appends &client=<id> to
    // this URL -- without it the site apparently doesn't reliably associate
    // the code with a pending login (the site says "return to the game" but
    // CheckLogin never resolves it, so the launcher sits on "Not signed in"
    // with a network-error toast until POLL_MAX_ATTEMPTS gives up).
    private static final String LOGIN_URL_FMT = "https://www.playgenerals.online/login/?gamecode=%s&client=%s";
    private static final String CLIENT_ID = "custom_third_party_client";

    private static final String PREFS_NAME = GeneralsOnlineSession.PREFS_NAME;
    private static final String PREF_SESSION_TOKEN = GeneralsOnlineSession.PREF_SESSION_TOKEN;
    private static final String PREF_REFRESH_TOKEN = GeneralsOnlineSession.PREF_REFRESH_TOKEN;
    private static final String PREF_USER_ID = GeneralsOnlineSession.PREF_USER_ID;
    private static final String PREF_DISPLAY_NAME = GeneralsOnlineSession.PREF_DISPLAY_NAME;
    private static final String PREF_WS_URI = GeneralsOnlineSession.PREF_WS_URI;

    // Matches the reference client's own 1s poll cadence
    // (OnlineServices_Auth.cpp::Tick, timeBetweenChecks = 1000).
    private static final int POLL_INTERVAL_MS = 1000;
    private static final int POLL_MAX_ATTEMPTS = 180; // ~3 minutes

    private static final String CODE_CHARSET =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    private static final int CODE_LENGTH = 32;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final SecureRandom random = new SecureRandom();

    private TextView statusText;
    private MaterialButton signInButton;
    private MaterialButton signOutButton;

    private int pollAttempt = 0;
    private boolean busy = false;

    @Override
    protected void attachBaseContext(android.content.Context newBase) {
        super.attachBaseContext(LocaleHelper.wrap(newBase));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setTitle(R.string.online_window_title);
        buildUi();
        refreshStatus();
        maybeSilentReauth();
    }

    // GeneralsX @feature Android port launcher-ui-2026 08/09/2026 Same shell
    // as every other launcher screen now: an app bar with the title, a
    // scrolling column of UiKit cards, one accent action.
    private void buildUi() {
        LinearLayout shell = new LinearLayout(this);
        shell.setOrientation(LinearLayout.VERTICAL);
        shell.setBackgroundColor(UiKit.color(this, R.color.gzh_background));
        setContentView(shell);
        InsetUtil.applySafeInsets(shell);

        UiKit.appBar(shell, getString(R.string.online_subtitle),
            getString(R.string.online_window_title), 0, null, null);

        android.widget.FrameLayout host = new android.widget.FrameLayout(this);
        shell.addView(host, new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));
        LinearLayout page = UiKit.scrollingPage(host);

        LinearLayout statusCard = UiKit.card(page);
        UiKit.sectionHeader(statusCard, R.drawable.ic_gzh_account,
            getString(R.string.online_window_title), false);
        statusText = UiKit.body(statusCard, null);
        statusText.setTextIsSelectable(true);
        signOutButton = UiKit.button(statusCard, UiKit.BTN_DANGER, R.drawable.ic_gzh_trash,
            getString(R.string.online_button_sign_out), this::onSignOut);

        LinearLayout stepsCard = UiKit.card(page);
        UiKit.sectionHeader(stepsCard, R.drawable.ic_gzh_check,
            getString(R.string.online_card_sign_in), false);
        UiKit.supporting(stepsCard, getString(R.string.online_signin_help));
        signInButton = UiKit.button(stepsCard, UiKit.BTN_PRIMARY, R.drawable.ic_gzh_account,
            getString(R.string.online_button_sign_in), this::onSignIn);
    }

    private String generateGameCode() {
        StringBuilder sb = new StringBuilder(CODE_LENGTH);
        for (int i = 0; i < CODE_LENGTH; ++i) {
            sb.append(CODE_CHARSET.charAt(random.nextInt(CODE_CHARSET.length())));
        }
        return sb.toString();
    }

    // If we already have a refresh_token from a previous sign-in, try to
    // silently re-authenticate instead of making the user go through the
    // browser again -- mirrors BeginLogin()'s GetCredentials()/LoginWithToken
    // branch in the reference client.
    private void maybeSilentReauth() {
        String refreshToken = getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
            .getString(PREF_REFRESH_TOKEN, null);
        if (refreshToken == null || refreshToken.isEmpty() || busy) {
            return;
        }
        busy = true;
        signInButton.setEnabled(false);
        statusText.setText(R.string.online_status_signing_in);
        new Thread(() -> {
            GeneralsOnlineSession.AuthResult result = callLoginWithToken(refreshToken);
            handler.post(() -> {
                busy = false;
                signInButton.setEnabled(true);
                if (result != null && result.state == 1) {
                    saveSession(result);
                    refreshStatus();
                } else if (result != null && result.state == 2) {
                    // Server explicitly says the refresh token is dead --
                    // fall back to a fresh browser sign-in next time the
                    // user taps Sign In.
                    clearSession();
                    refreshStatus();
                } else {
                    // GeneralsX @bugfix Android port 08/30/2026 result==null
                    // (or an unexpected state) means the request never got a
                    // real answer -- network error or a blocked/rejected
                    // request (see GeneralsOnlineSession.postJson). That is
                    // NOT the same as "this refresh token is invalid", so
                    // don't clearSession() here: a transient connectivity
                    // problem used to silently wipe a perfectly good cached
                    // session, forcing the full browser flow again on next
                    // launch even though nothing was actually wrong with the
                    // account. Leave the cached session alone; the game (or
                    // a later launch) will just try refreshing again.
                    refreshStatus();
                    if (result == null) {
                        statusText.setText(withNetworkErrorDetail(getString(R.string.online_status_network_error)));
                    }
                }
            });
        }).start();
    }

    // GeneralsX @bugfix Android port 10/07/2026 the sign-in flow backgrounds
    // this Activity for the whole browser round-trip; without a battery
    // exemption, some OEM battery managers throttle the poll timer hard
    // enough that CheckLogin never actually runs, so a login that visibly
    // succeeded on the website never completes here. First tap requests the
    // exemption (and stops there -- no browser yet); once granted (or if it
    // already was), the next tap proceeds with the real sign-in.
    private boolean ensureNotBatteryOptimized() {
        android.os.PowerManager pm = (android.os.PowerManager) getSystemService(POWER_SERVICE);
        if (pm != null && pm.isIgnoringBatteryOptimizations(getPackageName())) {
            return true;
        }
        try {
            Intent intent = new Intent(android.provider.Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS,
                Uri.parse("package:" + getPackageName()));
            startActivity(intent);
        } catch (Exception e) {
            // Some OEMs don't support this action; fall through and let sign-in
            // proceed anyway rather than blocking the user entirely.
            return true;
        }
        statusText.setText(R.string.online_status_battery_opt);
        return false;
    }

    private void onSignIn() {
        if (busy) {
            return;
        }
        if (!ensureNotBatteryOptimized()) {
            return;
        }
        busy = true;
        pollAttempt = 0;
        signInButton.setEnabled(false);

        String code = generateGameCode();
        String url = String.format(LOGIN_URL_FMT, code, CLIENT_ID);
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)));
        } catch (Exception e) {
            busy = false;
            signInButton.setEnabled(true);
            Toast.makeText(this, getString(R.string.online_toast_no_browser, e.getMessage()), Toast.LENGTH_LONG).show();
            return;
        }

        statusText.setText(R.string.online_status_continue_browser);
        handler.postDelayed(() -> pollOnce(code), POLL_INTERVAL_MS);
    }

    private void pollOnce(String code) {
        new Thread(() -> {
            GeneralsOnlineSession.AuthResult result = callCheckLogin(code);
            handler.post(() -> handlePollResult(code, result));
        }).start();
    }

    private void handlePollResult(String code, GeneralsOnlineSession.AuthResult result) {
        if (result == null) {
            busy = false;
            signInButton.setEnabled(true);
            statusText.setText(withNetworkErrorDetail(getString(R.string.online_status_network_error)));
            return;
        }

        switch (result.state) {
            case 1: // SUCCEEDED
                busy = false;
                signInButton.setEnabled(true);
                saveSession(result);
                refreshStatus();
                Toast.makeText(this, getString(R.string.online_toast_signed_in_as, result.displayName), Toast.LENGTH_LONG).show();
                break;
            case 2: // FAILED
                busy = false;
                signInButton.setEnabled(true);
                statusText.setText(R.string.online_status_signin_failed);
                break;
            case 0: // WAITING_USER_ACTION
            case -1: // CODE_INVALID (not registered yet server-side -- keep polling, it's a timing thing)
                ++pollAttempt;
                if (pollAttempt >= POLL_MAX_ATTEMPTS) {
                    busy = false;
                    signInButton.setEnabled(true);
                    statusText.setText(R.string.online_status_timed_out);
                } else {
                    handler.postDelayed(() -> pollOnce(code), POLL_INTERVAL_MS);
                }
                break;
            default:
                busy = false;
                signInButton.setEnabled(true);
                statusText.setText(R.string.online_status_unexpected);
                break;
        }
    }

    // GeneralsX @bugfix Android port 08/30/2026 A user reported the network-
    // error screen with no way to see WHY it failed (no adb/logcat access).
    // GeneralsOnlineSession.lastNetworkErrorDetail now captures the actual
    // host + HTTP status/body snippet (or exception) from the failed
    // request -- surface it right on screen instead of just the generic
    // string. statusText already has setTextIsSelectable(true), so this is
    // also copyable to paste into a bug report.
    private String withNetworkErrorDetail(String baseMessage) {
        String detail = GeneralsOnlineSession.lastNetworkErrorDetail;
        if (detail == null || detail.isEmpty()) {
            return baseMessage;
        }
        return baseMessage + "\n\n" + detail;
    }

    // Runs on a background thread.
    private GeneralsOnlineSession.AuthResult callCheckLogin(String code) {
        JSONObject body = new JSONObject();
        try {
            body.put("code", code);
            body.put("client_id", CLIENT_ID);
            body.put("reserved_0", "");
            body.put("reserved_1", "");
            body.put("reserved_2", "");
        } catch (Exception e) {
            return null;
        }
        return GeneralsOnlineSession.postJson("CheckLogin", body, null);
    }

    // Runs on a background thread.
    private GeneralsOnlineSession.AuthResult callLoginWithToken(String refreshToken) {
        return GeneralsOnlineSession.loginWithToken(refreshToken);
    }

    private void saveSession(GeneralsOnlineSession.AuthResult result) {
        GeneralsOnlineSession.saveSession(this, result);
    }

    private void clearSession() {
        GeneralsOnlineSession.clearSession(this);
    }

    private void onSignOut() {
        // TODO: also DELETE /User/{user_id} server-side like the reference
        // client's LogoutOfMyAccount(), once we're sure "sign out" here
        // should mean "forget this device" rather than just "clear local
        // session" -- left local-only for now since that's the safer default.
        clearSession();
        refreshStatus();
        Toast.makeText(this, R.string.online_toast_signed_out, Toast.LENGTH_SHORT).show();
    }

    private void refreshStatus() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String displayName = prefs.getString(PREF_DISPLAY_NAME, null);
        String sessionToken = prefs.getString(PREF_SESSION_TOKEN, null);

        if (displayName != null && sessionToken != null && !sessionToken.isEmpty()) {
            statusText.setText(getString(R.string.online_status_signed_in_as, displayName));
            signOutButton.setEnabled(true);
        } else {
            statusText.setText(R.string.online_status_not_signed_in);
            signOutButton.setEnabled(false);
        }
    }

    // Static helper so other screens (SetupActivity) can show a one-line
    // status without duplicating the SharedPreferences keys.
    static String getSignedInDisplayName(android.content.Context ctx) {
        SharedPreferences prefs = ctx.getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String displayName = prefs.getString(PREF_DISPLAY_NAME, null);
        String sessionToken = prefs.getString(PREF_SESSION_TOKEN, null);
        if (displayName != null && sessionToken != null && !sessionToken.isEmpty()) {
            return displayName;
        }
        return null;
    }

}
