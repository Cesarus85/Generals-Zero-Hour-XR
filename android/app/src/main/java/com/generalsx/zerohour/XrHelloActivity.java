// GeneralsX @spike XR port Phase 0.4 - launcher Activity for the native
// OpenXR loop (XrHello.cpp). Deliberately NOT an SDLActivity: the XR loop
// renders into OpenXR swapchains and needs no window surface. Own launcher
// icon ("Generals XR"); the loop runs on a background thread,
// onPause/onDestroy stop it.
//
// GeneralsX @feature XR port Phase 1.0 - the loop now boots the full game
// offscreen, so this activity mirrors GeneralsZHActivity's launch gate:
// extract the bundled runtime, redirect to Setup when no game folder is
// configured (Setup is kept iconless in this flavor for exactly this),
// copy bundled files into a Setup-selected folder. Never touch libmain.so
// on a misconfigured install.
package com.generalsx.zerohour;

import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Scanner;

public class XrHelloActivity extends Activity {

    private static final String TAG = "gx-xr-hello";

    // GeneralsX @feature Codex 14/09/2026 Optional spatial data permission.
    // Called by the native XR thread; only Android's UI thread opens the prompt.
    private static final String SCENE_PERMISSION = "com.oculus.permission.USE_SCENE";
    private static final int SCENE_REQUEST = 1901;
    private volatile boolean sceneRequestPending = false;
    public synchronized int scenePermission(boolean request) {
        if (checkSelfPermission(SCENE_PERMISSION) == android.content.pm.PackageManager.PERMISSION_GRANTED) return 1;
        if (sceneRequestPending) return 2;
        if (!request || isFinishing() || isDestroyed()) return 0;
        sceneRequestPending = true;
        runOnUiThread(() -> {
            if (isFinishing() || isDestroyed()) { sceneRequestPending = false; return; }
            try { requestPermissions(new String[]{SCENE_PERMISSION}, SCENE_REQUEST); }
            catch (RuntimeException failure) {
                sceneRequestPending = false;
                Log.w(TAG, "Scene permission unavailable; manual layout remains available", failure);
            }
        });
        return 2;
    }
    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (requestCode == SCENE_REQUEST) sceneRequestPending = false;
    }

    private Thread mThread;
    private boolean mXrStarted = false;

    static {
        System.loadLibrary("main");
    }

    private static native void runHello(Object activity, int initialLanguage);
    private static native void stopHello();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        extractBundledRuntime();

        String gamePath = readSavedGamePath();
        boolean haveCustomPath = gamePath != null && SetupActivity.isValidGameFolder(new File(gamePath));
        File externalRoot = getExternalFilesDir(null);
        boolean haveLegacyPath = !haveCustomPath && externalRoot != null
                && SetupActivity.isValidGameFolder(new File(externalRoot, "GameData"));

        if (!haveCustomPath && !haveLegacyPath) {
            Log.i(TAG, "no valid game folder configured; redirecting to Setup");
            startActivity(new Intent(this, SetupActivity.class));
            finish();
            return;
        }

        // Same retroactive copy-if-missing as the 2D entry: an install that
        // already had a path saved keeps missing fonts/ otherwise.
        if (haveCustomPath && externalRoot != null) {
            SetupActivity.copyBundledRuntimeIfMissing(externalRoot, gamePath);
        }

        Log.i(TAG, "onCreate: starting XR thread");
        // GeneralsX @feature Codex 14/09/2026 Also cover legacy direct XR
        // launches that did not pass through the Setup language screen.
        SetupActivity.seedInitialGameTextLanguage(this, haveCustomPath ? gamePath :
                new File(externalRoot, "GameData").getAbsolutePath());
        final int initialLanguage = InitialLanguage.xrCode(LocaleHelper.systemLanguage());
        mThread = new Thread(() -> {
            try {
                runHello(XrHelloActivity.this, initialLanguage);
            } catch (Throwable t) {
                Log.e(TAG, "XR thread died", t);
            } finally {
                runOnUiThread(() -> { if (!isFinishing()) finish(); });
            }
        }, "xr-hello");
        mThread.start();
        mXrStarted = true;
    }

    // The Setup-selected folder, same marker file the 2D flavor's native
    // boot reads (written by SetupActivity into this package's own
    // internal storage when the folder is picked).
    private String readSavedGamePath() {
        File marker = new File(getFilesDir(), "gamedata_path.txt");
        if (!marker.exists()) {
            return null;
        }
        try (Scanner scanner = new Scanner(marker)) {
            if (scanner.hasNextLine()) {
                String path = scanner.nextLine().trim();
                return path.isEmpty() ? null : path;
            }
        } catch (IOException e) {
            Log.w(TAG, "could not read game path marker", e);
        }
        return null;
    }

    // Asset -> external files dir copy. Duplicated from
    // GeneralsZHActivity (not shared) so the proven 2D launch path stays
    // byte-identical; keep the two in sync when the runtime set changes.
    private void extractBundledRuntime() {
        File root = getExternalFilesDir(null);
        if (root == null) {
            Log.e(TAG, "external files dir unavailable; asset extraction skipped");
            return;
        }
        copyAssetTree("gamedata", root);
    }

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

    @Override
    protected void onPause() {
        super.onPause();
        // GeneralsX @fix Codex 13/09/2026 System overlays and doffing are
        // transient. OpenXR visibility/focus owns suspension, not Activity pause.
        Log.i(TAG, "onPause: OpenXR session state controls suspension");
    }

    @Override
    protected void onDestroy() {
        stopHello();
        super.onDestroy();
        // The engine's singletons are not restart-safe: a second boot in
        // this process would re-init over live state. End the process with
        // the activity so every launch starts fresh -- but only when the
        // XR thread actually ran (the Setup redirect above finishes without
        // starting it, and must not kill Setup with it).
        if (mXrStarted) {
            Log.i(TAG, "onDestroy: ending process for a fresh next launch");
            System.exit(0);
        }
    }
}
