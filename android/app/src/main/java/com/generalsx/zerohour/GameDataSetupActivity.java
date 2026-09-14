// GeneralsX @feature Codex 14/09/2026 Guided data setup and common launch gate.
package com.generalsx.zerohour;

import android.app.*;
import android.content.*;
import android.os.*;
import android.provider.Settings;
import android.net.Uri;
import android.widget.*;
import android.view.View;
import java.io.*;
import java.util.concurrent.*;
import com.google.android.material.button.MaterialButton;

public class GameDataSetupActivity extends Activity {
    private static final int PICK_GAME = 710, PICK_BASE = 711, PERMISSION = 712;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private String gamePath, basePath;
    private boolean busy, autoLaunch, committing;
    private int pendingPicker, generation;
    private TextView status;
    private MaterialButton launch, gameButton, baseButton, retry, languageButton, cancel;
    private GameDataValidator.Result result;

    @Override protected void attachBaseContext(Context base) { super.attachBaseContext(LocaleHelper.wrap(base)); }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        String[] paths = GameDataConfiguration.savedFolders(this);
        gamePath = paths[0]; basePath = paths[1];
        autoLaunch = getIntent().getBooleanExtra("auto_launch", Intent.ACTION_MAIN.equals(getIntent().getAction()));
        if (state != null) {
            gamePath = state.getString("draft_game"); basePath = state.getString("draft_base");
            pendingPicker = state.getInt("picker"); autoLaunch = state.getBoolean("auto");
        }
        // Horizon can recreate a stopped panel using an older saved Bundle.
        // Keep the latest explicit draft in this Activity's Intent as well;
        // this is not a committed configuration or an engine launch approval.
        if (getIntent().getBooleanExtra("has_draft", false)) {
            gamePath = getIntent().getStringExtra("draft_game");
            basePath = getIntent().getStringExtra("draft_base");
            autoLaunch = false;
        }
        buildUi();
        if (pendingPicker == 0) check();
    }

    @Override protected void onSaveInstanceState(Bundle state) {
        super.onSaveInstanceState(state);
        state.putString("draft_game", gamePath); state.putString("draft_base", basePath);
        state.putInt("picker", pendingPicker); state.putBoolean("auto", autoLaunch);
    }

    private void buildUi() {
        setTitle(R.string.data_setup_title);
        LinearLayout shell = new LinearLayout(this); shell.setOrientation(LinearLayout.VERTICAL);
        shell.setBackgroundColor(UiKit.color(this, R.color.gzh_background));
        setContentView(shell); InsetUtil.applySafeInsets(shell);
        UiKit.appBar(shell, getString(R.string.app_name), getString(R.string.data_setup_title), 0, null, null);
        LinearLayout page = UiKit.scrollingPage(shell);
        languageButton = UiKit.button(page, UiKit.BTN_OUTLINE, R.drawable.ic_gzh_globe,
            "English / Deutsch", this::chooseLanguage);
        LinearLayout intro = UiKit.card(page);
        UiKit.sectionHeader(intro, R.drawable.ic_gzh_info, getString(R.string.data_step_prepare), false);
        UiKit.body(intro, getString(R.string.data_prepare));
        UiKit.button(intro, UiKit.BTN_OUTLINE, R.drawable.ic_gzh_doc,
            getString(R.string.data_sources), () -> new AlertDialog.Builder(this)
                .setTitle(R.string.data_sources).setMessage(R.string.data_sources_help)
                .setPositiveButton(android.R.string.ok, null).show());
        LinearLayout data = UiKit.card(page);
        UiKit.sectionHeader(data, R.drawable.ic_gzh_folder, getString(R.string.data_step_select), false);
        UiKit.supporting(data, getString(R.string.data_select_help));
        gameButton = UiKit.button(data, UiKit.BTN_TONAL, R.drawable.ic_gzh_folder,
            getString(R.string.data_pick_zh), () -> requestPicker(PICK_GAME));
        baseButton = UiKit.button(data, UiKit.BTN_OUTLINE, R.drawable.ic_gzh_folder,
            getString(R.string.data_pick_base), () -> requestPicker(PICK_BASE));
        LinearLayout review = UiKit.card(page);
        UiKit.sectionHeader(review, R.drawable.ic_gzh_check, getString(R.string.data_step_check), false);
        status = UiKit.body(review, getString(R.string.data_no_folder)); status.setTextIsSelectable(true);
        retry = UiKit.button(review, UiKit.BTN_OUTLINE, R.drawable.ic_gzh_check,
            getString(R.string.data_recheck), this::check);
        launch = UiKit.button(page, UiKit.BTN_PRIMARY, R.drawable.ic_gzh_play,
            getString(R.string.data_use_launch), this::commitAndLaunch);
        launch.setEnabled(false);
        cancel = UiKit.button(page, UiKit.BTN_OUTLINE, 0, getString(R.string.common_cancel), this::finish);
    }

    private void chooseLanguage() {
        autoLaunch = false;
        retainDraft();
        String[] tags = {"", "en", "de"};
        String[] names = {getString(R.string.setup_language_system_default), "English", "Deutsch"};
        new AlertDialog.Builder(this).setTitle(R.string.setup_language_dialog_title)
            .setItems(names, (d, i) -> { LocaleHelper.setSavedLanguageTag(this, tags[i]); recreate(); })
            .setNegativeButton(R.string.common_cancel, null).show();
    }

    private boolean storageAllowed() {
        return Build.VERSION.SDK_INT >= 30 ? Environment.isExternalStorageManager()
            : checkSelfPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE) == android.content.pm.PackageManager.PERMISSION_GRANTED
                && checkSelfPermission(android.Manifest.permission.WRITE_EXTERNAL_STORAGE) == android.content.pm.PackageManager.PERMISSION_GRANTED;
    }

    private void requestPicker(int request) {
        autoLaunch = false;
        if (!storageAllowed()) {
            new AlertDialog.Builder(this).setTitle(R.string.data_permission_title)
                .setMessage(R.string.data_permission_help)
                .setPositiveButton(R.string.data_continue, (d, w) -> {
                    pendingPicker = request;
                    if (Build.VERSION.SDK_INT >= 30) {
                        try { startActivityForResult(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                            Uri.parse("package:" + getPackageName())), PERMISSION); }
                        catch (ActivityNotFoundException e) {
                            try { startActivityForResult(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION), PERMISSION); }
                            catch (ActivityNotFoundException missing) { pendingPicker = 0; status.setText(R.string.data_permission_denied); }
                        }
                    } else requestPermissions(new String[]{android.Manifest.permission.READ_EXTERNAL_STORAGE,
                        android.Manifest.permission.WRITE_EXTERNAL_STORAGE}, PERMISSION);
                }).setNegativeButton(R.string.common_cancel, null).show();
        } else openPicker(request);
    }

    private void openPicker(int request) {
        pendingPicker = request;
        startActivityForResult(new Intent(this, FolderPickerActivity.class)
            .putExtra("base_generals", request == PICK_BASE)
            .putExtra("initial_path", request == PICK_BASE ? basePath : gamePath), request);
    }

    private void permissionReturned() {
        int next = pendingPicker; pendingPicker = 0;
        if (next == 0) return;
        if (storageAllowed()) openPicker(next); else status.setText(R.string.data_permission_denied);
    }

    @Override public void onRequestPermissionsResult(int code, String[] permissions, int[] grants) {
        super.onRequestPermissionsResult(code, permissions, grants);
        if (code == PERMISSION) permissionReturned();
    }

    @Override protected void onActivityResult(int request, int code, Intent data) {
        super.onActivityResult(request, code, data);
        if (request == PERMISSION) { permissionReturned(); return; }
        pendingPicker = 0;
        if (code == RESULT_OK && data != null) {
            String picked = data.getStringExtra(FolderPickerActivity.EXTRA_SELECTED_PATH);
            if (request == PICK_GAME) { gamePath = picked; basePath = null; }
            else if (request == PICK_BASE) basePath = picked;
            retainDraft();
        }
        check();
    }

    private void retainDraft() {
        getIntent().putExtra("has_draft", true).putExtra("draft_game", gamePath).putExtra("draft_base", basePath);
    }

    private void setBusy(boolean value) {
        busy = value; gameButton.setEnabled(!value); baseButton.setEnabled(!value);
        retry.setEnabled(!value); languageButton.setEnabled(!value);
        launch.setEnabled(!value && result != null && result.ready());
    }

    private void check() {
        if (busy) return;
        result = null; setBusy(true); status.setText(R.string.data_checking);
        final String game = gamePath, base = basePath;
        final int request = ++generation;
        worker.execute(() -> {
            GameDataValidator.Result checked;
            try { checked = GameDataValidator.check(file(game), file(base)); }
            catch (RuntimeException e) { checked = new GameDataValidator.Result(); checked.unreadable = true; }
            final GameDataValidator.Result value = checked;
            runOnUiThread(() -> {
                if (isFinishing() || isDestroyed() || request != generation) return;
                result = value; setBusy(false); showResult();
                if (autoLaunch && value.ready()) { autoLaunch = false; commitAndLaunch(); }
                else autoLaunch = false;
            });
        });
    }

    private void showResult() {
        StringBuilder text = new StringBuilder();
        if (gamePath == null) text.append(getString(R.string.data_no_folder));
        else if (result.installer && result.game == null) text.append(getString(R.string.data_installer));
        else if (result.unreadable) text.append(getString(R.string.data_unreadable));
        else if (result.choices.size() > 1) {
            text.append(getString(R.string.data_multiple));
            for (File choice : result.choices) text.append("\n").append(choice.getAbsolutePath());
        } else if (result.game == null) text.append(getString(R.string.data_no_zh));
        else {
            text.append(getString(R.string.data_paths, result.game.getAbsolutePath(),
                result.base == null ? getString(R.string.data_not_found) : result.base.getAbsolutePath()));
            if (!result.missing.isEmpty()) text.append("\n\n").append(getString(R.string.data_missing))
                .append("\n").append(android.text.TextUtils.join("\n", result.missing));
            if (!result.damaged.isEmpty()) text.append("\n\n").append(getString(R.string.data_damaged))
                .append("\n").append(android.text.TextUtils.join("\n", result.damaged));
            if (!result.weather) text.append("\n\n").append(getString(R.string.data_no_weather));
            if (!result.language) text.append("\n\n").append(getString(R.string.data_no_language));
            if (result.ready()) text.append("\n\n").append(getString(R.string.data_ready));
        }
        status.setText(text);
    }

    private static File file(String path) { return path == null ? null : new File(path); }

    private void commitAndLaunch() {
        if (busy || result == null || !result.ready()) return;
        final File game = result.game, base = result.base;
        committing = true; cancel.setEnabled(false);
        setBusy(true); status.setText(R.string.data_checking);
        worker.execute(() -> {
            String error = null;
            try {
                GameDataValidator.Result fresh = GameDataValidator.check(game, base);
                if (!fresh.ready()) throw new IOException(getString(R.string.data_changed));
                GameDataConfiguration.save(this, fresh);
                // The approved game entry extracts/copies its bundled runtime.
            } catch (Exception e) { error = e.getMessage() == null ? e.getClass().getSimpleName() : e.getMessage(); }
            final String failure = error;
            runOnUiThread(() -> {
                if (isFinishing() || isDestroyed()) return;
                committing = false; cancel.setEnabled(true);
                setBusy(false);
                if (failure != null) { status.setText(getString(R.string.data_save_failed, failure)); return; }
                Class<?> target = getPackageName().endsWith(".xr") ? XrHelloActivity.class : GeneralsZHActivity.class;
                startActivity(new Intent(this, target)); finish();
            });
        });
    }

    @Override public void onBackPressed() { if (!committing) super.onBackPressed(); }

    @Override protected void onDestroy() { generation++; worker.shutdownNow(); super.onDestroy(); }
}
