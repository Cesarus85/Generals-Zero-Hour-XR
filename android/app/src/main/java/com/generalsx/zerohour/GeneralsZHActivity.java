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

// GeneralsX @build Android port 06/07/2026, reworked 07/07/2026
// Thin shell over SDL3's SDLActivity. Responsibilities:
//  1. Name the native libraries to load (libmain.so = the game).
//  2. On launch, extract the small bundled runtime files (fonts/, dxvk.conf,
//     DefaultOptions.ini) from APK assets into the external files dir, which
//     SDL3Main.cpp makes the game's working directory. Game .big archives are
//     NOT bundled — the user picks their own via the GeneralsZH Setup app.
//  3. If no valid game folder is configured yet (checked via SetupActivity's
//     saved preference, mirroring the marker file SDL3Main.cpp reads),
//     redirect to Setup INSTEAD OF calling super.onCreate() — this means
//     libmain.so is never dlopen'd on a misconfigured install, so a missing
//     game data folder can never look like (or mask) a native crash.

package com.generalsx.zerohour;

import android.content.Intent;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;

import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class GeneralsZHActivity extends SDLActivity {

    private static final String TAG = "GeneralsZH";

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL3",
            // libmain.so — the game itself (z_generals target, android-vulkan
            // preset). Its DT_NEEDED entries (SDL3_image, openal, c++_shared,
            // gamespy) resolve from the same APK; the DXVK d3d8/d3d9
            // libraries are dlopen()ed by the engine at D3D init.
            "main"
        };
    }

    // TheSuperHackers @bugfix Android port 08/07/2026 THE reason the game kept
    // rotating despite the manifest's screenOrientation="landscape" AND the
    // setRequestedOrientation() call in onCreate(): SDL3's native window
    // creation calls SDLActivity.setOrientation() over JNI, which lands here
    // (setOrientationBis) and — for a non-resizable landscape window with no
    // SDL_HINT_ORIENTATIONS hint — applies SCREEN_ORIENTATION_SENSOR_LANDSCAPE,
    // silently overriding both earlier locks and re-enabling accelerometer
    // rotation (including the 180° landscape flip the user kept seeing).
    // SDL documents this method as "This can be overridden": pin it to the
    // absolute landscape orientation unconditionally.
    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        setRequestedOrientation(android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // TheSuperHackers @bugfix Android port 07/07/2026 Belt-and-suspenders
        // on top of the manifest's screenOrientation="landscape": a real
        // device log still showed Resolve_Present_BackBuffer_Size catching a
        // portrait-sized window during the Setup -> Launch transition, so the
        // manifest lock alone isn't settling fast enough on every device/OEM
        // skin. Setting it again here in code takes effect before this
        // Activity's window is even measured, closing the gap further.
        setRequestedOrientation(android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        extractBundledRuntime();

        String gamePath = getSavedGamePath();
        boolean haveCustomPath = gamePath != null && SetupActivity.isValidGameFolder(new File(gamePath));
        boolean haveLegacyPath = !haveCustomPath && isValidGameFolder(legacyGameDataDir());

        if (!haveCustomPath && !haveLegacyPath) {
            // Never touch libmain.so on a misconfigured install: redirect to
            // Setup instead of letting SDLActivity load the native library
            // into an app state that can only end in a black screen or a
            // confusing crash the user has no way to diagnose.
            Log.i(TAG, "no valid game folder configured; redirecting to Setup");
            startActivity(new Intent(this, SetupActivity.class));
            finish();
            return;
        }

        // GeneralsX @bugfix Android port 12/07/2026 GeneralsOnline session
        // tokens expire server-side within hours, but native code reads a
        // static token from the session marker file -- a player who signed in
        // earlier the same day got "Could not connect to GeneralsOnline (HTTP
        // response code said error)" (401 on MOTD + WebSocket, confirmed by
        // device log). Trade the cached refresh_token for a fresh session on
        // every launch, in the background, before the player can reach the
        // Online button; on failure the old marker stays (nothing regresses
        // offline).
        GeneralsOnlineSession.refreshSessionAsync(this);

        // GeneralsX @bugfix Android port 07/07/2026 Apply the fonts/dxvk.conf/
        // DefaultOptions.ini copy-if-missing fix retroactively on every launch,
        // not just when the folder is freshly picked in Setup — an install that
        // already had a custom path saved before this fix shipped would
        // otherwise keep missing fonts/ forever (every button renders with no
        // text; see SetupActivity.copyBundledRuntimeIfMissing for why).
        if (haveCustomPath) {
            File bundledRoot = getExternalFilesDir(null);
            if (bundledRoot != null) {
                SetupActivity.copyBundledRuntimeIfMissing(bundledRoot, gamePath);
            }
        }

        super.onCreate(savedInstanceState);
    }

    // GeneralsX @bugfix Android port 02/08/2026 A tester reported the camera
    // panning to the map's left edge and then not responding to further
    // swipes to bring it back -- until opening and closing the in-game menu
    // "fixed" it. A real device log showed the exact culprit: after the pan
    // that hit the edge, zero touch events of any kind reached
    // handleTouchEvent() (SDL3GameEngine.cpp) for the rest of the session --
    // not a camera-math bug (screenToTerrain never failed), a touch-DELIVERY
    // one. SDLActivity already requests SYSTEM_UI_FLAG_IMMERSIVE_STICKY, but
    // that only suppresses the legacy 3-button nav bar; on gesture-navigation
    // Android (10+), a drag starting near the left/right screen edge is
    // reserved for the system back gesture regardless of immersive-sticky,
    // and is never delivered to the app at all. A camera already pinned at a
    // map boundary is exactly when the player's next recovery swipe is most
    // likely to start right at that edge -- explaining both symptoms in one
    // shot. setSystemGestureExclusionRects() (API 29+) is the documented fix;
    // the system doesn't enforce its usual per-app exclusion-height cap on
    // windows already in sticky immersive mode, which this one is.
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyGestureNavBackBehavior();
        }
    }

    // GeneralsX @bugfix Android port 04/08/2026, corrected 04/08/2026 A
    // first attempt here set BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE, which
    // was backwards -- traced through AOSP (ViewRootImpl / NavigationBar /
    // QuickStepContract): with the nav bar hidden, THAT specific behavior
    // is exactly what disables the edge back gesture at the source
    // (EdgeBackGestureHandler never arms; SystemUI's
    // SYSUI_STATE_NAV_BAR_HIDDEN disables back unless
    // SYSUI_STATE_ALLOW_GESTURE_IGNORING_BAR_VISIBILITY is also set, which
    // only happens when behavior != SHOW_TRANSIENT_BARS_BY_SWIPE). It's
    // also SDLActivity's own IMPLICIT default whenever
    // SYSTEM_UI_FLAG_IMMERSIVE_STICKY/FLAG_FULLSCREEN is set and nothing
    // has explicitly overridden it -- so setting it explicitly here just
    // pinned the exact state that was already breaking the gesture.
    // BEHAVIOR_DEFAULT is what actually keeps back-gesture recognition
    // alive while the bars stay hidden. (SDLActivity.java's own
    // COMMAND_CHANGE_WINDOW_STYLE handler sets this too, right where the
    // conflicting legacy flags are (re)applied from native at unpredictable
    // times -- this call here is a secondary safety net on focus-change,
    // not the only place it's enforced.)
    private void applyGestureNavBackBehavior() {
        WindowInsetsControllerCompat controller =
            WindowCompat.getInsetsController(getWindow(), getWindow().getDecorView());
        if (controller != null) {
            controller.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_DEFAULT);
        }
    }

    // GeneralsX @bugfix Android port 04/08/2026, REMOVED 04/08/2026 This used
    // to call setSystemGestureExclusionRects() with a band capped at 200dp
    // and centered vertically on each edge, meant to protect an in-app
    // camera-pan-recovery swipe from being stolen by the OS back gesture.
    // The math assumed a tall portrait surface; this app is
    // android:screenOrientation="landscape" (AndroidManifest.xml), so
    // mSurface.getHeight() is the SHORT dimension -- on a typical 20:9
    // landscape phone that's ~360dp, giving only ~80dp of open margin on
    // each side (out of a "200dp centered" band that assumed hundreds of
    // dp more room than actually existed), not the few hundred dp intended.
    // Combined with AOSP's own bottom-gesture-height inset eating most of
    // the lower margin, the actually-usable open region shrank to a sliver
    // -- exactly the "works in some tiny millimeter" the tester reported.
    // Removed entirely rather than re-tuned: the touch-classification
    // state machine in SDL3GameEngine.cpp (PENDING -> PANNING dead-zone
    // logic) has been substantially rewritten since the original camera-
    // pinned-at-edge bug this was protecting against, so it's not
    // confirmed that bug still reproduces the same way -- removing this
    // is also the only way to find out, and a live back gesture is worth
    // more than a speculative fix for a bug that may no longer exist in
    // its original form.

    private String getSavedGamePath() {
        return SetupActivity.getSavedGamePath(this);
    }

    // Legacy convention from before the in-app Setup flow existed (an adb
    // push into <external>/GameData) — still honored so nothing breaks for
    // anyone who already has files there.
    private File legacyGameDataDir() {
        File root = getExternalFilesDir(null);
        return root != null ? new File(root, "GameData") : null;
    }

    private boolean isValidGameFolder(File dir) {
        return SetupActivity.isValidGameFolder(dir);
    }

    /**
     * Copy the APK's bundled runtime files into the external files dir
     * (the game's working directory). Existing files are left alone so a
     * user-edited dxvk.conf or replaced font survives updates; delete the
     * file to get a fresh copy on next launch. The one exception is
     * gamedata/Window/ -- see copyAssetTree's comment on ALWAYS_OVERWRITE_PREFIX.
     */
    private void extractBundledRuntime() {
        File root = getExternalFilesDir(null);
        if (root == null) {
            Log.e(TAG, "external files dir unavailable; asset extraction skipped");
            return;
        }
        copyAssetTree("gamedata", root);
    }

    // GeneralsX @bugfix Android port 02/08/2026 gamedata/Window/ holds loose
    // .wnd screens WE inject (GroupPanel.wnd) that the player never edits --
    // unlike dxvk.conf/DefaultOptions.ini/fonts/ below it, which are meant to
    // survive an update untouched, a stale copy of OUR OWN file here just
    // means every change we ship (this exact feature went through several
    // rounds of position/behavior fixes) silently never reaches an existing
    // install. Always overwrite this one subtree; everything else keeps the
    // normal "leave it alone if it already exists" behavior.
    private static final String ALWAYS_OVERWRITE_PREFIX = "Window/";

    private void copyAssetTree(String assetPath, File destRoot) {
        AssetManager assets = getAssets();
        try {
            String[] children = assets.list(assetPath);
            if (children == null || children.length == 0) {
                // Leaf: a real file
                String rel = assetPath.substring("gamedata".length());
                if (rel.startsWith("/")) rel = rel.substring(1);
                if (rel.isEmpty()) return;
                File dest = new File(destRoot, rel);
                boolean alwaysOverwrite = rel.startsWith(ALWAYS_OVERWRITE_PREFIX);
                if (dest.exists() && !alwaysOverwrite) return;
                File parent = dest.getParentFile();
                if (parent != null && !parent.exists() && !parent.mkdirs()) {
                    Log.e(TAG, "mkdirs failed for " + parent);
                    return;
                }
                try (InputStream in = assets.open(assetPath);
                     OutputStream out = new FileOutputStream(dest)) {
                    byte[] buf = new byte[65536];
                    int n;
                    while ((n = in.read(buf)) > 0) {
                        out.write(buf, 0, n);
                    }
                }
                Log.i(TAG, "extracted " + rel);
            } else {
                for (String child : children) {
                    copyAssetTree(assetPath + "/" + child, destRoot);
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "asset extraction failed for " + assetPath, e);
        }
    }
}
