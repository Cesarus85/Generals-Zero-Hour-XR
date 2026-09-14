// GeneralsX @feature Codex 14/09/2026 Commit only validated paths; recover interrupted paired writes.
package com.generalsx.zerohour;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.AtomicFile;
import java.io.*;
import java.nio.charset.StandardCharsets;

final class GameDataConfiguration {
    private GameDataConfiguration() {}

    // GeneralsX @bugfix Codex 14/09/2026 Resolve configured paths without
    // constructing the importer (a 2D Activity would interrupt XR startup).
    static String[] savedFolders(Context ctx) {
        SharedPreferences prefs = ctx.getSharedPreferences(SetupActivity.PREFS_NAME, Context.MODE_PRIVATE);
        String game = prefs.getString(SetupActivity.PREF_GAME_PATH, null);
        String base = prefs.getString(SetupActivity.PREF_BASE_GENERALS_PATH, null);
        if (game == null) game = readMarker(ctx, "gamedata_path.txt");
        if (game == null) game = SetupActivity.readExternalMarker();
        if (base == null) base = readMarker(ctx, "generals_base_path.txt");
        File external = ctx.getExternalFilesDir(null);
        if (game == null && external != null) {
            File legacy = new File(external, "GameData");
            if (GameDataValidator.named(legacy, "INIZH.big") != null) game = legacy.getAbsolutePath();
        }
        return new String[]{game, base};
    }

    static GameDataValidator.Result checkSaved(Context ctx) {
        String[] paths = savedFolders(ctx);
        return GameDataValidator.check(paths[0] == null ? null : new File(paths[0]),
            paths[1] == null ? null : new File(paths[1]));
    }

    static void ensureNativePaths(Context ctx, GameDataValidator.Result result) throws IOException {
        if (!result.ready()) throw new IOException("Game data not ready");
        SharedPreferences prefs = ctx.getSharedPreferences(SetupActivity.PREFS_NAME, Context.MODE_PRIVATE);
        String game = result.game.getAbsolutePath(), base = result.base.getAbsolutePath();
        if (game.equals(prefs.getString(SetupActivity.PREF_GAME_PATH, null))
                && base.equals(prefs.getString(SetupActivity.PREF_BASE_GENERALS_PATH, null))
                && game.equals(readMarker(ctx, "gamedata_path.txt"))
                && base.equals(readMarker(ctx, "generals_base_path.txt"))) return;
        save(ctx, result);
    }

    private static String readMarker(Context ctx, String name) {
        try (BufferedReader in = new BufferedReader(new FileReader(new File(ctx.getFilesDir(), name)))) {
            String value = in.readLine();
            return value == null || value.trim().isEmpty() ? null : value.trim();
        } catch (IOException e) { return null; }
    }

    static synchronized void save(Context ctx, GameDataValidator.Result result) throws IOException {
        if (!result.ready()) throw new IOException("Game data not ready");
        String game = result.game.getAbsolutePath(), base = result.base.getAbsolutePath();
        SharedPreferences prefs = ctx.getSharedPreferences(SetupActivity.PREFS_NAME, Context.MODE_PRIVATE);
        // The saved preferences are authoritative. If a prior write was interrupted,
        // this launch gate runs again and repairs both markers before engine startup.
        if (!prefs.edit().putString(SetupActivity.PREF_GAME_PATH, game)
                .putString(SetupActivity.PREF_BASE_GENERALS_PATH, base).commit())
            throw new IOException("Could not save game folders");
        write(new File(ctx.getFilesDir(), "generals_base_path.txt"), base);
        write(new File(ctx.getFilesDir(), "gamedata_path.txt"), game);
        // Optional reinstall recovery hint. Failure must not prevent use of
        // app-private data or a valid native configuration.
        try {
            write(new File(android.os.Environment.getExternalStorageDirectory(), ".generalszh_gamepath.txt"), game);
        } catch (IOException | SecurityException e) {
            android.util.Log.i("GameDataSetup", "External recovery hint unavailable", e);
        }
    }

    private static void write(File path, String text) throws IOException {
        AtomicFile target = new AtomicFile(path);
        FileOutputStream stream = null;
        try {
            stream = target.startWrite(); stream.write((text + "\n").getBytes(StandardCharsets.UTF_8));
            target.finishWrite(stream);
        } catch (IOException e) { if (stream != null) target.failWrite(stream); throw e; }
    }
}
