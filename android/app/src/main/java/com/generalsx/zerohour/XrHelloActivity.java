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
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.graphics.Color;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.Log;
import android.view.Gravity;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;

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
    private Thread mDataCheck;
    private Boolean mStartupReady;
    private boolean mResumed, mStartDispatched;

    static {
        System.loadLibrary("main");
    }

    private static native void runHello(Object activity, int initialLanguage);
    private static native void stopHello();
    private static native void nativeXrTextChanged(long token, String text, boolean done);

    private EditText xrKeyboardEditor;
    private boolean xrKeyboardInternalChange;
    private long xrKeyboardToken;

    // GeneralsX @feature Codex 23/09/2026 The native OpenXR activity has no
    // SDL View, so provide one nearly invisible Android editor solely to let
    // Quest's system IME serve every original game entry gadget.
    public void showXrKeyboard(long token, String value, int inputMode, int maxLength) {
        runOnUiThread(() -> {
            if (isFinishing() || isDestroyed() || token == 0) return;
            if (xrKeyboardEditor == null) {
                xrKeyboardEditor = new EditText(this);
                xrKeyboardEditor.setSingleLine(true);
                xrKeyboardEditor.setBackgroundColor(Color.TRANSPARENT);
                xrKeyboardEditor.setTextColor(Color.TRANSPARENT);
                xrKeyboardEditor.setCursorVisible(false);
                xrKeyboardEditor.setAlpha(0.01f);
                xrKeyboardEditor.setShowSoftInputOnFocus(true);
                xrKeyboardEditor.setImeOptions(EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
                xrKeyboardEditor.addTextChangedListener(new TextWatcher() {
                    @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
                    @Override public void onTextChanged(CharSequence s, int start, int before, int count) {}
                    @Override public void afterTextChanged(Editable s) {
                        if (!xrKeyboardInternalChange && xrKeyboardToken != 0)
                            nativeXrTextChanged(xrKeyboardToken, s.toString(), false);
                    }
                });
                xrKeyboardEditor.setOnEditorActionListener((view, actionId, event) -> {
                    if (actionId == EditorInfo.IME_ACTION_DONE ||
                            (event != null && event.getKeyCode() == android.view.KeyEvent.KEYCODE_ENTER)) {
                        nativeXrTextChanged(xrKeyboardToken, xrKeyboardEditor.getText().toString(), true);
                        hideXrKeyboard(xrKeyboardToken);
                        return true;
                    }
                    return false;
                });
                FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(2, 2, Gravity.TOP | Gravity.START);
                addContentView(xrKeyboardEditor, params);
            }
            xrKeyboardToken = token;
            int type = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
            if (inputMode == 1) type = InputType.TYPE_CLASS_PHONE;
            else if (inputMode == 2) type = InputType.TYPE_CLASS_NUMBER;
            else if (inputMode == 3)
                type = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD;
            xrKeyboardEditor.setInputType(type);
            xrKeyboardEditor.setFilters(new InputFilter[]{new InputFilter.LengthFilter(Math.max(1, maxLength))});
            xrKeyboardInternalChange = true;
            xrKeyboardEditor.setText(value == null ? "" : value);
            xrKeyboardEditor.setSelection(xrKeyboardEditor.length());
            xrKeyboardInternalChange = false;
            xrKeyboardEditor.requestFocus();
            xrKeyboardEditor.postDelayed(() -> {
                InputMethodManager ime = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
                if (ime != null) {
                    boolean requested = ime.showSoftInput(xrKeyboardEditor, InputMethodManager.SHOW_IMPLICIT);
                    Log.i(TAG, "Quest system keyboard requested: " + requested);
                }
            }, 100);
        });
    }

    public void hideXrKeyboard(long token) {
        runOnUiThread(() -> {
            if (xrKeyboardEditor == null || (token != 0 && token != xrKeyboardToken)) return;
            InputMethodManager ime = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
            if (ime != null) ime.hideSoftInputFromWindow(xrKeyboardEditor.getWindowToken(), 0);
            xrKeyboardEditor.clearFocus();
            xrKeyboardToken = 0;
        });
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // GeneralsX @bugfix Codex 14/09/2026 Stay in the immersive launcher:
        // showing a 2D importer on every launch makes Horizon switch environments.
        mDataCheck = new Thread(() -> {
            boolean ready = false;
            try {
                GameDataValidator.Result checked = GameDataConfiguration.checkSaved(getApplicationContext());
                if (Thread.currentThread().isInterrupted()) return;
                if (checked.ready()) {
                    GameDataConfiguration.ensureNativePaths(getApplicationContext(), checked);
                    ready = true;
                }
            } catch (IOException | RuntimeException e) {
                Log.w(TAG, "Saved data unavailable; setup required", e);
            }
            final boolean approved = ready;
            runOnUiThread(() -> {
                if (isFinishing() || isDestroyed()) return;
                mStartupReady = approved;
                continueStartup();
            });
        }, "xr-data-check");
        mDataCheck.start();
    }

    @Override protected void onResume() {
        super.onResume();
        mResumed = true;
        continueStartup();
    }

    private void continueStartup() {
        if (!mResumed || mStartupReady == null || mStartDispatched || isFinishing() || isDestroyed()) return;
        mStartDispatched = true;
        if (!mStartupReady) {
            Log.i(TAG, "Saved data needs setup; opening importer");
            startActivity(new Intent(this, GameDataSetupActivity.class));
            finish();
            return;
        }
        Log.i(TAG, "Saved data ready; direct XR startup (no importer)");
        startGame();
    }

    private void startGame() {
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
        mResumed = false;
        super.onPause();
        // GeneralsX @fix Codex 13/09/2026 System overlays and doffing are
        // transient. OpenXR visibility/focus owns suspension, not Activity pause.
        Log.i(TAG, "onPause: OpenXR session state controls suspension");
    }

    @Override
    protected void onDestroy() {
        if (mDataCheck != null) mDataCheck.interrupt();
        // A setup redirect must not stop a newly approved XR activity when
        // Android destroys the old, never-started entry asynchronously.
        if (mXrStarted) stopHello();
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
