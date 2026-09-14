// GeneralsX @feature Codex 14/09/2026 Non-destructive setup smoke on a configured device.
package com.generalsx.zerohour;

import android.app.*;
import android.content.*;
import android.content.res.Configuration;
import android.graphics.*;
import android.os.*;
import android.view.View;
import android.widget.TextView;
import java.io.*;
import java.lang.reflect.*;
import java.nio.file.Files;
import java.util.*;

/** Run only on a device with installed, configured game data. Never launches a match. */
public final class GameDataSetupSmoke extends Instrumentation {
    private int checks;
    private GameDataSetupActivity screen;
    @Override public void runOnMainSync(Runnable action) {
        final Throwable[] failure = {null};
        super.runOnMainSync(() -> { try { action.run(); } catch (Throwable t) { failure[0] = t; } });
        if (failure[0] != null) throw new AssertionError("UI assertion", failure[0]);
    }
    private void expect(boolean value, String message) {
        if (!value) throw new AssertionError(message);
        checks++;
    }
    private Object field(String name) {
        try { Field f = GameDataSetupActivity.class.getDeclaredField(name); f.setAccessible(true); return f.get(screen); }
        catch (Exception e) { throw new RuntimeException(e); }
    }
    private void idleCheck() {
        long end = SystemClock.elapsedRealtime() + 30000;
        final boolean[] busy = {true};
        do {
            waitForIdleSync(); runOnMainSync(() -> busy[0] = (boolean) field("busy"));
            if (!busy[0]) return;
            SystemClock.sleep(30);
        } while (SystemClock.elapsedRealtime() < end);
        throw new AssertionError("Validation timed out");
    }
    @Override public void onCreate(Bundle args) { super.onCreate(args); start(); }
    @Override public void onStart() {
        Bundle output = new Bundle();
        try {
            Context ctx = getTargetContext();
            SharedPreferences prefs = ctx.getSharedPreferences(SetupActivity.PREFS_NAME, Context.MODE_PRIVATE);
            Map<String, ?> before = new HashMap<>(prefs.getAll());
            File gameMarker = new File(ctx.getFilesDir(), "gamedata_path.txt");
            File baseMarker = new File(ctx.getFilesDir(), "generals_base_path.txt");
            byte[] gameBefore = Files.readAllBytes(gameMarker.toPath());
            byte[] baseBefore = Files.readAllBytes(baseMarker.toPath());
            GameDataValidator.Result real = GameDataValidator.check(
                new File(new String(gameBefore, "UTF-8").trim()), new File(new String(baseBefore, "UTF-8").trim()));
            expect(real.ready(), "Live Quest files rejected: " + real.missing + real.damaged);
            ActivityMonitor silent = addMonitor(GameDataSetupActivity.class.getName(), null, false);
            long gameStamp = gameMarker.lastModified(), baseStamp = baseMarker.lastModified();
            GameDataValidator.Result saved = GameDataConfiguration.checkSaved(ctx);
            expect(saved.ready(), "Silent saved-data check rejected working install");
            expect(real.game.equals(saved.game) && real.base.equals(saved.base), "Silent check changed resolved paths");
            GameDataConfiguration.ensureNativePaths(ctx, saved);
            expect(silent.getHits() == 0, "Silent check opened importer");
            expect(gameMarker.lastModified() == gameStamp, "Unchanged game marker rewritten during launch");
            expect(baseMarker.lastModified() == baseStamp, "Unchanged base marker rewritten during launch");
            removeMonitor(silent);
            screen = (GameDataSetupActivity) startActivitySync(new Intent(ctx, GameDataSetupActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK).putExtra("auto_launch", false));
            idleCheck();
            runOnMainSync(() -> {
                expect(((View)field("launch")).isEnabled(), "Complete install not launchable");
                expect(((TextView)field("languageButton")).getText().toString().contains("Deutsch"), "Visible language switch");
                // Capture actual Android widget rendering, no engine or room pixels.
                try {
                    View view = screen.getWindow().getDecorView();
                    Bitmap image = Bitmap.createBitmap(view.getWidth(), view.getHeight(), Bitmap.Config.ARGB_8888);
                    view.draw(new Canvas(image));
                    try (OutputStream out = new FileOutputStream(new File(ctx.getCacheDir(), "data-setup-qa.png"))) {
                        image.compress(Bitmap.CompressFormat.PNG, 100, out);
                    }
                    image.recycle();
                } catch (IOException e) { throw new RuntimeException(e); }
            });
            for (String tag : new String[]{"en", "de"}) {
                Configuration config = new Configuration(ctx.getResources().getConfiguration());
                config.setLocale(Locale.forLanguageTag(tag));
                Context localized = ctx.createConfigurationContext(config);
                expect(localized.getString(R.string.data_pick_zh).contains(tag.equals("en") ? "Choose" : "wählen"),
                    "Missing setup translation " + tag);
            }
            // Exercise an unconfigured draft without clearing the installed app.
            runOnMainSync(() -> screen.onActivityResult(710, Activity.RESULT_OK, new Intent()));
            idleCheck();
            runOnMainSync(() -> {
                expect(!((View)field("launch")).isEnabled(), "Unconfigured draft enabled launch");
                expect(((TextView)field("status")).getText().toString().equals(screen.getString(R.string.data_no_folder)),
                    "First-run instruction missing");
            });
            File fixture = Files.createTempDirectory(ctx.getCacheDir().toPath(), "setup-disc-").toFile();
            new File(fixture, "setup.exe").createNewFile();
            runOnMainSync(() -> screen.onActivityResult(710, Activity.RESULT_OK,
                new Intent().putExtra(FolderPickerActivity.EXTRA_SELECTED_PATH, fixture.getAbsolutePath())));
            idleCheck();
            runOnMainSync(() -> {
                expect(!((View)field("launch")).isEnabled(), "Installer-only folder enabled launch");
                expect(((GameDataValidator.Result)field("result")).installer, "Installer explanation missing");
            });
            ActivityMonitor monitor = addMonitor(GameDataSetupActivity.class.getName(), null, false);
            runOnMainSync(() -> screen.recreate());
            screen = (GameDataSetupActivity)waitForMonitorWithTimeout(monitor, 10000);
            removeMonitor(monitor);
            expect(screen != null, "Setup recreation failed"); idleCheck();
            runOnMainSync(() -> {
                expect(fixture.getAbsolutePath().equals(field("gamePath")), "Draft lost during recreation");
                expect(!((View)field("launch")).isEnabled(), "Invalid draft accepted after recreation");
            });
            boolean refused = false;
            try { GameDataConfiguration.save(ctx, new GameDataValidator.Result()); }
            catch (IOException expected) { refused = true; }
            expect(refused, "Invalid save did not fail");
            runOnMainSync(() -> screen.finish());
            expect(before.equals(prefs.getAll()), "Setup changed saved preferences without confirmation");
            expect(Arrays.equals(gameBefore, Files.readAllBytes(gameMarker.toPath())), "Game marker changed");
            expect(Arrays.equals(baseBefore, Files.readAllBytes(baseMarker.toPath())), "Base marker changed");
            output.putString("stream", checks + " setup checks passed; existing data and language preferences unchanged\n");
            finish(Activity.RESULT_OK, output);
        } catch (Throwable e) {
            if (screen != null) runOnMainSync(() -> screen.finish());
            output.putString("stream", android.util.Log.getStackTraceString(e));
            finish(Activity.RESULT_CANCELED, output);
        }
    }
}
