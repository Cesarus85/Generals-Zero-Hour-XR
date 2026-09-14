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

// GeneralsX @build Android port 07/07/2026
//
// A standalone launcher icon ("GeneralsZH Setup"), separate from the game
// itself, so configuring the game folder or reading a crash log never
// depends on the game having launched successfully first — and never
// requires adb. This is the practical answer to "there's no launcher": a
// full mod-manager-style launcher (à la GenLauncher) is future scope, but
// picking where the game lives and seeing why it crashed are needed on
// every single install, so they live here now.

package com.generalsx.zerohour;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.text.style.UnderlineSpan;
import android.view.ContextThemeWrapper;
import android.view.Menu;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.widget.SwitchCompat;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.google.android.material.bottomnavigation.BottomNavigationView;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.navigation.NavigationBarView;
import com.google.android.material.slider.LabelFormatter;
import com.google.android.material.slider.Slider;
import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import java.net.HttpURLConnection;
import java.net.URL;
import java.io.File;

public class SetupActivity extends Activity {

    static final String PREFS_NAME = "generalszh_setup";
    static final String PREF_GAME_PATH = "game_path";
    // GeneralsX @feature Android port 06/09/2026 Optional folder holding the
    // BASE Generals archives, for copies that keep them somewhere the engine
    // will not find on its own.
    static final String PREF_BASE_GENERALS_PATH = "base_generals_path";

    // TheSuperHackers @bugfix Android port 07/07/2026 SharedPreferences and
    // getFilesDir() both live under /data/data/<pkg>/ and are wiped the
    // moment the app is uninstalled -- which is exactly what a sideloaded
    // APK update often requires if the installer treats it as a fresh
    // install rather than an in-place update. Mirror the chosen path into a
    // small marker file on shared external storage (survives uninstall,
    // since it's outside the app's private/package-scoped directories) so a
    // fresh install can recover it automatically instead of re-prompting.
    private static final String EXTERNAL_MARKER_NAME = ".generalszh_gamepath.txt";

    // Marker files SDL3Main.cpp / GeneralsZHActivity check for on launch —
    // must match GameEngine/CMake's GeneralsMD/Code/Main/SDL3Main.cpp exactly.
    private static final String[] REQUIRED_GAME_FILES = { "INIZH.big", "INI.big" };

    private TextView statusText;

    @Override
    protected void attachBaseContext(android.content.Context newBase) {
        super.attachBaseContext(LocaleHelper.wrap(newBase));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // GeneralsX @bugfix Android port 31/07/2026 No longer forced to
        // landscape here -- see the matching AndroidManifest.xml comment.
        // This screen now starts portrait-first like every other non-game
        // screen; onLaunchGame()/onConfigurationChanged() below handle the
        // Setup -> Launch rotation race that used to be sidestepped by never
        // rotating Setup at all.
        super.onCreate(savedInstanceState);
        setTitle(R.string.setup_window_title);

        // GeneralsX @feature Codex 14/09/2026 First-time setup precedes advanced settings.
        if (getSavedGamePath() == null) {
            startActivity(new Intent(this, GameDataSetupActivity.class));
            finish();
            return;
        }

        // GeneralsX @feature Android port launcher-ui-2026 08/09/2026 Which
        // bottom-navigation section to open on. Survives the recreate() the
        // language picker performs, so changing the launcher language leaves
        // you looking at the section you changed it from rather than being
        // dropped back on Home.
        if (savedInstanceState != null) {
            currentTab = savedInstanceState.getInt(STATE_TAB, TAB_HOME);
        }

        // GeneralsX @bugfix Android port 08/07/2026 This screen is the ONLY
        // way to reach "View Logs" without adb, so it must never be the thing
        // that crashes. Any future Material/theme incompatibility falls back
        // to a bare-bones plain-widget UI (same actions, no styling) instead
        // of taking the whole Settings app down with it.
        try {
            buildUi();
        } catch (Throwable t) {
            buildFallbackUi(t);
        }
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putInt(STATE_TAB, currentTab);
    }

    // GeneralsX @bugfix Android port launcher-ui-2026 08/09/2026 The fallback
    // exists precisely for the case where a Material widget or theme attribute
    // is what blew up, so it must not itself construct one -- it used to build
    // MaterialButtons, which would have failed again for exactly the reason it
    // was reached. Plain framework widgets only, from here down.
    private void buildFallbackUi(Throwable failure) {
        clearPageReferences();
        // No tabs in the fallback: showTab() must become a no-op if anything
        // still calls it (onActivityResult does).
        contentHost = null;
        appBarTitle = null;
        ScrollView scroll = new ScrollView(this);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(20);
        root.setPadding(pad, pad, pad, pad);
        scroll.addView(root);
        setContentView(scroll);
        InsetUtil.applySafeInsets(scroll);

        TextView warning = new TextView(this);
        warning.setText(getString(R.string.setup_fallback_warning, String.valueOf(failure)));
        warning.setPadding(0, 0, 0, dp(16));
        root.addView(warning);

        statusText = new TextView(this);
        statusText.setTextIsSelectable(true);
        statusText.setPadding(0, 0, 0, dp(24));
        root.addView(statusText);

        addPlainButton(root, getString(R.string.setup_button_select_game_folder), this::onSelectGameFolder);
        addPlainButton(root, getString(R.string.setup_button_view_logs), this::onViewLogs);
        addPlainButton(root, getString(R.string.setup_button_launch_game), this::onLaunchGame);
        addPlainButton(root, getString(R.string.setup_button_clear_game_folder), this::onClearGameFolder);
    }

    private void addPlainButton(LinearLayout root, String label, Runnable action) {
        Button b = new Button(this);
        b.setText(label);
        b.setOnClickListener(v -> action.run());
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0, dp(4), 0, dp(4));
        root.addView(b, lp);
    }

    @Override
    protected void onResume() {
        super.onResume();
        // GeneralsX @bugfix Android port 31/07/2026 onLaunchGame() forces this
        // Activity to landscape right before starting the game (see its
        // comment) so the rotation settles before GeneralsZHActivity's native
        // window-size probe runs. That request otherwise sticks on this
        // Activity instance indefinitely, so coming back here (Back from the
        // game, or from any child screen) left Setup stuck landscape instead
        // of returning to its normal portrait-first state. Reset it every
        // time this screen comes back to the foreground; onLaunchGame()
        // re-applies the landscape lock itself the next time it's needed.
        setRequestedOrientation(android.content.pm.ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
        refreshStatus();
        refreshGeneralsOnlineStatus();
        loadDxvkConfigIntoEditor();
        refreshDiagnosticsSwitches();
    }

    // GeneralsX @feature Android port launcher-ui-2026 08/09/2026 The launcher
    // was one endless scroll: eleven stacked cards, every one of them visible
    // whether or not it had anything to do with what you came here for, and
    // the two controls people actually open this app for (pick a folder,
    // launch) sat in the middle of it. It is now five sections behind a
    // Material 3 bottom navigation bar -- Home, Graphics, Interface, Tools,
    // Help -- each a short page you can take in at a glance.
    //
    // Nothing was dropped in the process: every card, button, switch, slider
    // and status line that existed before still exists, and every string
    // resource is still used. See showTab() for where each one landed.
    private static final String STATE_TAB = "gzh_tab";
    private static final int TAB_HOME = 1;
    private static final int TAB_GRAPHICS = 2;
    private static final int TAB_INTERFACE = 3;
    private static final int TAB_TOOLS = 4;
    private static final int TAB_HELP = 5;

    private int currentTab = TAB_HOME;
    private FrameLayout contentHost;
    private TextView appBarTitle;

    private void buildUi() {
        clearPageReferences();

        LinearLayout shell = new LinearLayout(this);
        shell.setOrientation(LinearLayout.VERTICAL);
        shell.setBackgroundColor(UiKit.color(this, R.color.gzh_background));
        setContentView(shell);
        // Edge-to-edge still handled the same way: pad the outermost view by
        // the system bars/cutout so the app bar clears the status bar and the
        // navigation bar below clears the gesture handle.
        InsetUtil.applySafeInsets(shell);

        appBarTitle = UiKit.appBar(shell, getString(R.string.setup_title),
            getString(R.string.nav_tab_home),
            R.drawable.ic_gzh_doc, getString(R.string.setup_button_view_logs), this::onViewLogs);

        contentHost = new FrameLayout(this);
        shell.addView(contentHost, new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));

        shell.addView(buildBottomNav(), new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        if (contentHost.getChildCount() == 0) {
            showTab(currentTab);
        }
    }

    private BottomNavigationView buildBottomNav() {
        BottomNavigationView nav = new BottomNavigationView(this);

        // Keep the tab order left-to-right in every language, Arabic and Farsi
        // included. Android mirrors layouts in RTL locales, which is correct for
        // the content -- and the rest of this launcher is built on start/end so
        // it mirrors properly -- but it also reversed the five tabs, putting Home
        // on the right, and that was reported as wrong. The tabs are a fixed rail
        // of destinations rather than a line of reading, so pin the bar itself to
        // LTR and leave text direction on the locale, so the labels still shape
        // and read right-to-left inside their items.
        nav.setLayoutDirection(android.view.View.LAYOUT_DIRECTION_LTR);
        nav.setTextDirection(android.view.View.TEXT_DIRECTION_LOCALE);

        nav.setBackgroundColor(UiKit.color(this, R.color.gzh_surface_container_low));
        nav.setElevation(0f);
        nav.setLabelVisibilityMode(NavigationBarView.LABEL_VISIBILITY_LABELED);
        nav.setItemIconSize(UiKit.dp(this, 22));
        // Checked/unchecked pair: the selected item is the one sitting in the
        // active-indicator pill, so it takes the on-container colour.
        android.content.res.ColorStateList itemTint = new android.content.res.ColorStateList(
            new int[][] { new int[] { android.R.attr.state_checked }, new int[0] },
            new int[] { UiKit.color(this, R.color.gzh_on_primary_container),
                        UiKit.color(this, R.color.gzh_on_surface_faint) });
        nav.setItemIconTintList(itemTint);
        nav.setItemTextColor(itemTint);
        nav.setItemActiveIndicatorColor(UiKit.tint(this, R.color.gzh_primary_container));
        nav.setItemRippleColor(UiKit.tint(this, R.color.gzh_ripple_primary));

        Menu menu = nav.getMenu();
        menu.add(Menu.NONE, TAB_HOME, 0, R.string.nav_tab_home).setIcon(R.drawable.ic_gzh_home);
        menu.add(Menu.NONE, TAB_GRAPHICS, 1, R.string.nav_tab_graphics).setIcon(R.drawable.ic_gzh_display);
        menu.add(Menu.NONE, TAB_INTERFACE, 2, R.string.nav_tab_interface).setIcon(R.drawable.ic_gzh_globe);
        menu.add(Menu.NONE, TAB_TOOLS, 3, R.string.nav_tab_tools).setIcon(R.drawable.ic_gzh_wrench);
        menu.add(Menu.NONE, TAB_HELP, 4, R.string.nav_tab_help).setIcon(R.drawable.ic_gzh_info);

        nav.setOnItemSelectedListener(item -> {
            showTab(item.getItemId());
            return true;
        });
        nav.setSelectedItemId(currentTab);
        return nav;
    }

    private int tabTitle(int tab) {
        switch (tab) {
            case TAB_GRAPHICS:  return R.string.nav_tab_graphics;
            case TAB_INTERFACE: return R.string.nav_tab_interface;
            case TAB_TOOLS:     return R.string.nav_tab_tools;
            case TAB_HELP:      return R.string.nav_tab_help;
            default:            return R.string.nav_tab_home;
        }
    }

    /**
     * Swaps the page inside the shell. Every view a refresh* method touches is
     * page-scoped, so the references are dropped first and the refreshers are
     * re-run at the end against whatever the new page actually built -- they
     * all null-check, so a page that has no status line simply doesn't get one
     * updated.
     */
    private void showTab(int tab) {
        currentTab = tab;
        if (contentHost == null) {
            return;  // the plain-widget fallback UI is up; there are no tabs
        }
        clearPageReferences();
        contentHost.removeAllViews();
        if (appBarTitle != null) {
            appBarTitle.setText(tabTitle(tab));
        }

        LinearLayout page = UiKit.scrollingPage(contentHost);
        switch (tab) {
            case TAB_GRAPHICS:
                buildRenderBackendSection(page);
                // Custom Vulkan driver / dxvk.conf only matter when Vulkan is
                // the selected backend -- the GLES/GLES+ANGLE paths never
                // touch DXVK at all, see
                // Core/Libraries/Source/d3d8gles/CMakeLists.txt.
                if (RENDER_BACKEND_VULKAN.equals(getRenderBackendChoice())) {
                    applyRecommendedDriverIfNeeded();
                    buildCustomDriverSection(page);
                    buildDxvkConfigSection(page);
                }
                break;
            case TAB_INTERFACE:
                buildLanguageSection(page);
                buildUiScaleSection(page);
                break;
            case TAB_TOOLS:
                buildLogsSection(page);
                buildDiagnosticsSection(page);
                break;
            case TAB_HELP:
                buildHelpSection(page);
                break;
            case TAB_HOME:
            default:
                buildHomeSection(page);
                break;
        }

        refreshStatus();
        refreshGeneralsOnlineStatus();
        loadDxvkConfigIntoEditor();
        refreshDiagnosticsSwitches();
    }

    /** Forgets every page-scoped view so a stale one is never written to. */
    private void clearPageReferences() {
        statusText = null;
        onlineStatusView = null;
        gameLanguageStatusView = null;
        renderBackendStatusView = null;
        customDriverStatusView = null;
        diagnosticsNoFolderHint = null;
        dxvkConfigEdit = null;
        uiScaleSlider = null;
        uiScaleLabel = null;
        java.util.Arrays.fill(diagnosticSwitches, null);
    }

    // ------------------------------------------------------------ Home page

    private void buildHomeSection(LinearLayout page) {
        // The one thing this app exists to do, as the first thing on it.
        UiKit.button(page, UiKit.BTN_PRIMARY, R.drawable.ic_gzh_play,
            getString(R.string.setup_button_launch_game), this::onLaunchGame);

        LinearLayout folder = UiKit.card(page);
        UiKit.sectionHeader(folder, R.drawable.ic_gzh_folder,
            getString(R.string.setup_card_game_folder), false);

        statusText = UiKit.body(folder, null);
        statusText.setTextIsSelectable(true);

        UiKit.button(folder, UiKit.BTN_TONAL, R.drawable.ic_gzh_folder,
            getString(R.string.setup_button_select_game_folder), this::onSelectGameFolder);
        UiKit.button(folder, UiKit.BTN_TONAL, R.drawable.ic_gzh_folder,
            getString(R.string.setup_button_select_base_generals), this::onSelectBaseGeneralsFolder);
        UiKit.button(folder, UiKit.BTN_DANGER, R.drawable.ic_gzh_broom,
            getString(R.string.setup_button_clear_game_folder), this::onClearGameFolder);
        if (getBaseGeneralsPath() != null) {
            UiKit.button(folder, UiKit.BTN_DANGER, R.drawable.ic_gzh_broom,
                getString(R.string.setup_button_clear_base_generals), this::onClearBaseGeneralsFolder);
        }

        // GeneralsX @bugfix Android port 01/08/2026 kept above the advanced
        // settings -- signing into GeneralsOnline is a primary action most
        // people want right after picking their game folder, not something to
        // bury under settings most players never touch.
        buildGeneralsOnlineSection(page);
    }

    // ------------------------------------------------------------ Help page

    private void buildHelpSection(LinearLayout page) {
        LinearLayout about = UiKit.card(page);
        UiKit.sectionHeader(about, R.drawable.ic_gzh_info,
            getString(R.string.setup_window_title), false);
        UiKit.supporting(about, getString(R.string.setup_subtitle));
        UiKit.chip(about, R.drawable.ic_gzh_check, versionLabel(),
            R.color.gzh_primary, R.color.gzh_surface_container_high);

        LinearLayout help = UiKit.card(page);
        UiKit.sectionHeader(help, R.drawable.ic_gzh_doc,
            getString(R.string.setup_card_how_it_works), false);
        UiKit.supporting(help, getString(R.string.setup_how_it_works_body));
    }

    // The build's own version, as the manifest carries it -- no new string
    // resource needed, and it is the first thing any bug report needs.
    private String versionLabel() {
        try {
            return getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (Exception e) {
            return "";
        }
    }

    // ------------------------------------------------------------ Logs entry

    private void buildLogsSection(LinearLayout page) {
        LinearLayout card = UiKit.card(page);
        UiKit.listRow(card, R.drawable.ic_gzh_doc,
            getString(R.string.setup_button_view_logs),
            getString(R.string.setup_status_logs_note),
            this::onViewLogs);
    }

    // GeneralsX @feature Android port 13/07/2026 GitHub issue #4: in-app
    // language override for this launcher (Setup/Log Viewer/folder browser)
    // -- see LocaleHelper for why it's a manual attachBaseContext() wrap
    // rather than androidx.appcompat's per-app language API, and why
    // "System Default" needs no explicit handling.
    private void buildLanguageSection(LinearLayout root) {
        LinearLayout content = UiKit.card(root);
        // The current language is the value the header carries, so the card
        // answers "what is it set to" before you read a word of it.
        TextView value = UiKit.sectionHeader(content, R.drawable.ic_gzh_globe,
            getString(R.string.setup_card_language), true);
        value.setText(LocaleHelper.displayNameFor(this, LocaleHelper.getSavedLanguageTag(this)));
        // The full "Language: X" sentence is still what a screen reader hears.
        content.setContentDescription(getString(R.string.setup_language_status,
            LocaleHelper.displayNameFor(this, LocaleHelper.getSavedLanguageTag(this))));

        UiKit.button(content, UiKit.BTN_TONAL, R.drawable.ic_gzh_globe,
            getString(R.string.setup_button_change_language), this::onChangeLanguage);

        migrateGameTextTokenFromMarker();
        gameLanguageStatusView = UiKit.supporting(content, null);
        updateGameLanguageStatusView();

        UiKit.button(content, UiKit.BTN_TONAL, R.drawable.ic_gzh_globe,
            getString(R.string.setup_button_game_text_language), this::onChangeGameTextLanguage);

        languagePackButton = UiKit.button(content, UiKit.BTN_OUTLINE, R.drawable.ic_gzh_download,
            getString(R.string.setup_button_download_langpack), this::onDownloadLanguagePack);

        UiKit.helpText(content, getString(R.string.setup_language_help));
    }

    // GeneralsX @feature Android port 13/07/2026 GitHub issue #4 follow-up:
    // status line showing whether the launcher's chosen language is also
    // being applied to the game's own text (see applyGameLanguageOverride()).
    private TextView gameLanguageStatusView;

    private void updateGameLanguageStatusView() {
        if (gameLanguageStatusView == null) {
            return;
        }
        String token = LocaleHelper.getGameTextToken(this);
        if (token == null || token.isEmpty()) {
            gameLanguageStatusView.setText(R.string.setup_game_text_status_system_default);
        } else {
            gameLanguageStatusView.setText(getString(R.string.setup_game_text_status,
                LocaleHelper.gameTextDisplayName(token)));
        }
    }

    // GeneralsX @feature Android port 09/09/2026 Which languages are actually installed.
    //
    // Not a fixed list of what the game might have shipped with: what is really in this
    // player's game folder right now, loose or inside a .big, as text or as the compiled
    // table. That is the list worth offering, and it is the one that answers "how do I get
    // back to English" -- English is simply one of the entries.
    private java.util.List<String> installedGameTextTokens() {
        java.util.TreeSet<String> found = new java.util.TreeSet<>();
        String gamePath = getSavedGamePath();
        if (gamePath == null) {
            return new java.util.ArrayList<>(found);
        }
        File root = new File(gamePath);

        // Loose folders: data/<language>/generals.str or .csf.
        File dataDir = new File(root, "data");
        File[] languageDirs = dataDir.listFiles();
        if (languageDirs != null) {
            for (File dir : languageDirs) {
                if (dir.isDirectory()
                    && (new File(dir, "generals.str").isFile() || new File(dir, "generals.csf").isFile())) {
                    found.add(dir.getName().toLowerCase(java.util.Locale.ROOT));
                }
            }
        }

        // And the archives, which is where the game's own language lives.
        collectArchiveLanguages(root, found);
        File[] children = root.listFiles();
        if (children != null) {
            for (File child : children) {
                if (child.isDirectory()) {
                    collectArchiveLanguages(child, found);
                }
            }
        }
        return new java.util.ArrayList<>(found);
    }

    // GeneralsX @feature Android port 09/09/2026 Read the language names out of the archives
    // instead of checking a list of names this file happens to know.
    //
    // A hardcoded list is wrong by construction here: the first language contributed after it
    // was written -- Ukrainian -- was not in it, so it could be neither offered nor detected.
    // Whatever is filed as data/<something>/generals.str or .csf is a language, whether anyone
    // anticipated it or not.
    private static void collectArchiveLanguages(File dir, java.util.Set<String> out) {
        File[] bigs = dir.listFiles((d, name) -> name.toLowerCase(java.util.Locale.ROOT).endsWith(".big"));
        if (bigs == null) {
            return;
        }
        for (File big : bigs) {
            forEachBigEntry(big, name -> {
                // data/<language>/generals.str|csf, and nothing deeper.
                if (!name.startsWith("data/") || !(name.endsWith("/generals.str") || name.endsWith("/generals.csf"))) {
                    return;
                }
                String middle = name.substring("data/".length(), name.lastIndexOf('/'));
                if (!middle.isEmpty() && middle.indexOf('/') < 0) {
                    out.add(middle);
                }
            });
        }
    }

    private void onChangeGameTextLanguage() {
        final java.util.List<String> tokens = installedGameTextTokens();
        if (tokens.isEmpty()) {
            toast(getString(R.string.setup_game_text_none_found));
            return;
        }
        // Entry 0 returns to the DE/EN system-based default.
        final String[] labels = new String[tokens.size() + 1];
        labels[0] = getString(R.string.setup_game_text_system_default);
        for (int i = 0; i < tokens.size(); i++) {
            labels[i + 1] = LocaleHelper.gameTextDisplayName(tokens.get(i));
        }
        String current = LocaleHelper.getGameTextToken(this);
        int checked = 0;
        for (int i = 0; i < tokens.size(); i++) {
            if (tokens.get(i).equals(current)) {
                checked = i + 1;
                break;
            }
        }
        new android.app.AlertDialog.Builder(this)
            .setTitle(R.string.setup_game_text_dialog_title)
            .setSingleChoiceItems(labels, checked, (dialog, which) -> {
                LocaleHelper.setGameTextToken(this, which == 0 ? "" : tokens.get(which - 1));
                applyGameLanguageOverride();
                updateGameLanguageStatusView();
                dialog.dismiss();
            })
            .setNegativeButton(R.string.common_cancel, null)
            .show();
    }

    private void onChangeLanguage() {
        String[] tags = LocaleHelper.SUPPORTED_TAGS;
        String[] labels = new String[tags.length];
        for (int i = 0; i < tags.length; i++) {
            labels[i] = LocaleHelper.displayNameFor(this, tags[i]);
        }
        String currentTag = LocaleHelper.getSavedLanguageTag(this);
        int currentIndex = 0;
        for (int i = 0; i < tags.length; i++) {
            if (tags[i].equals(currentTag)) {
                currentIndex = i;
                break;
            }
        }
        new android.app.AlertDialog.Builder(this)
            .setTitle(R.string.setup_language_dialog_title)
            .setSingleChoiceItems(labels, currentIndex, (dialog, which) -> {
                LocaleHelper.setSavedLanguageTag(this, tags[which]);
                applyGameLanguageOverride();
                dialog.dismiss();
                recreate();
            })
            .setNegativeButton(R.string.common_cancel, null)
            .show();
    }

    // GeneralsX @feature Android port 13/07/2026 GitHub issue #4 follow-up:
    // if the launcher's chosen UI language has a corresponding engine
    // language-folder token (LocaleHelper.gameDataLanguageFor()) AND the
    // user's own selected game folder actually has that data
    // (data/<token>/generals.csf -- lowercase "data", matching g_csfFile
    // in SDL3Main.cpp and the Linux case-sensitivity fix it documents), we
    // write <filesDir>/game_language.cfg so SDL3Main.cpp exports
    // CNC_ZH_LANGUAGE before any engine subsystem reads it
    // (GetRegistryLanguage() in registry.cpp checks that env var first,
    // ahead of registry.ini and the BIG-file auto-detect). If the data
    // isn't present -- most commonly Russian, which Zero Hour never
    // shipped officially and only exists via the user's own fan/licensed
    // copy -- we deliberately leave the game on its own default/auto-detect
    // rather than force a language with no text to show
    // (GameTextManager::init() in GameText.cpp fails gracefully, blank UI
    // text, if the CSF is missing -- silently wrong is worse than
    // untouched). Called both when the language changes and when the game
    // folder changes, since either one can flip the answer.
    // GeneralsX @feature Android port 09/09/2026 Adopt a marker written by an older build.
    //
    // Before the game's text language became a setting of its own it was derived from the
    // launcher's UI language and written straight to game_language.cfg. Someone upgrading has
    // that file and no preference, and without this they would silently be moved to English
    // on first launch of the new version. Read the old answer once and keep it.
    private void migrateGameTextTokenFromMarker() {
        if (LocaleHelper.hasGameTextToken(this)) {
            return;
        }
        File marker = new File(getFilesDir(), "game_language.cfg");
        if (!marker.isFile()) {
            return;
        }
        try (java.io.BufferedReader r = new java.io.BufferedReader(new java.io.FileReader(marker))) {
            String line = r.readLine();
            if (line != null && !line.trim().isEmpty()) {
                LocaleHelper.setGameTextToken(this, line.trim());
            }
        } catch (java.io.IOException e) {
            // Nothing to do: the player picks a language and the question answers itself.
        }
    }

    // GeneralsX @feature Codex 14/09/2026 No guessing from arbitrary BIG
    // names. Prefer German only for a German OS and an available text table.
    private static String initialGameTextToken(String gamePath) {
        String system = LocaleHelper.systemLanguage();
        return InitialLanguage.gameTextToken(system,
                InitialLanguage.xrCode(system) == 0 && gameHasTextFor(gamePath, "german"));
    }

    static void seedInitialGameTextLanguage(android.content.Context context, String gamePath) {
        File marker = new File(context.getFilesDir(), "game_language.cfg");
        if (marker.isFile() || LocaleHelper.hasGameTextToken(context)) return;
        String token = initialGameTextToken(gamePath);
        android.util.AtomicFile atomic = new android.util.AtomicFile(marker);
        java.io.FileOutputStream stream = null;
        try {
            stream = atomic.startWrite();
            stream.write((token + "\n").getBytes(java.nio.charset.StandardCharsets.UTF_8));
            atomic.finishWrite(stream);
            LocaleHelper.setGameTextToken(context, token);
        } catch (java.io.IOException failure) {
            if (stream != null) atomic.failWrite(stream);
            android.util.Log.w("gx-language", "Could not save first-run game language", failure);
        }
    }

    private void applyGameLanguageOverride() {
        migrateGameTextTokenFromMarker(); // Existing marker beats any new default.
        File marker = new File(getFilesDir(), "game_language.cfg");
        // GeneralsX @feature Android port 09/09/2026 Driven by the player's explicit choice,
        // not by the launcher's UI language.
        String engineToken = LocaleHelper.getGameTextToken(this);
        String gamePath = getSavedGamePath();
        boolean rememberInitial = !LocaleHelper.hasGameTextToken(this) && gamePath != null;
        boolean defaultedToEnglish = false;
        if (engineToken == null || engineToken.isEmpty()) {
            // GeneralsX @feature Codex 14/09/2026 First run: German OS ->
            // German when installed; every other OS -> English. Keep an
            // explicit token instead of the engine's archive auto-detection.
            engineToken = initialGameTextToken(gamePath);
            defaultedToEnglish = "english".equals(engineToken);
        }
        if (gamePath == null && !defaultedToEnglish) {
            marker.delete();
            return;
        }
        // A language the player CHOSE has to be there, or the game would come up with no text
        // at all. English is not checked: it is the answer of last resort, and writing it is
        // how the engine is kept off its own auto-detect.
        if (!defaultedToEnglish && !gameHasTextFor(gamePath, engineToken)) {
            marker.delete();
            return;
        }
        try (java.io.FileWriter w = new java.io.FileWriter(marker, false)) {
            w.write(engineToken);
            w.write("\n");
        } catch (java.io.IOException e) {
            // Not fatal: worst case the game just keeps its own default
            // language, same as before this feature existed.
            return;
        }
        if (rememberInitial) LocaleHelper.setGameTextToken(this, engineToken);
    }

    // TheSuperHackers @feature Android port 07/07/2026 Menu text size scaling
    // (GlobalLanguage::adjustFontSize()) has no in-game slider yet -- the
    // Options screen lives in the user's own game data (.wnd layout), not in
    // this engine source tree, so a real in-game control can't be added from
    // here. Expose the same "ResolutionFontAdjustment" percentage here
    // instead, writing straight into the Options.ini this Android build
    // actually reads (see SDL3Main.cpp: HOME=<internal storage>, so the file
    // is <filesDir>/.local/share/GeneralsX/GeneralsZH/Options.ini) -- no need
    // to wait for the game to visit its own Options menu first.
    private Slider uiScaleSlider;
    private TextView uiScaleLabel;

    // GeneralsX @bugfix Android port 08/07/2026 A prior "Interface Size (whole
    // UI)" option here rendered at a reduced internal resolution and let the
    // pillarbox blit upscale to the full panel, expecting buttons/controlbar
    // to grow along with text. On real devices only text actually changed
    // size — widget layout geometry is itself computed as a ratio of the
    // current resolution, so shrinking that resolution and then stretching
    // the result back up is a no-op for anything but text (whose *requested
    // font point size* is a genuinely bigger asset, not just a stretched
    // rect). Removed rather than ship a slider that visibly does nothing;
    // "Menu Text Size" below is the one scaling option that actually works.

    private void buildUiScaleSection(LinearLayout root) {
        LinearLayout content = UiKit.card(root);
        // The live percentage reads out of the header, right-aligned in the
        // accent, instead of a separate line above the track.
        uiScaleLabel = UiKit.sectionHeader(content, R.drawable.ic_gzh_sliders,
            getString(R.string.setup_card_text_size), true);

        int startPercent = readUiScalePercent();
        uiScaleSlider = new Slider(this);
        uiScaleSlider.setValueFrom(0f);
        uiScaleSlider.setValueTo(150f);
        uiScaleSlider.setStepSize(1f);
        uiScaleSlider.setValue(startPercent);
        // The floating bubble would show a bare untranslated number on top of
        // the value the header already spells out properly.
        uiScaleSlider.setLabelBehavior(LabelFormatter.LABEL_GONE);
        uiScaleSlider.setTrackActiveTintList(UiKit.tint(this, R.color.gzh_primary));
        uiScaleSlider.setTrackInactiveTintList(UiKit.tint(this, R.color.gzh_surface_container_highest));
        uiScaleSlider.setThumbTintList(UiKit.tint(this, R.color.gzh_primary));
        uiScaleSlider.setHaloTintList(UiKit.tint(this, R.color.gzh_ripple_primary));
        updateUiScaleLabel(startPercent);
        uiScaleSlider.addOnChangeListener((slider, value, fromUser) -> updateUiScaleLabel((int) value));
        LinearLayout.LayoutParams sliderLp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        sliderLp.topMargin = UiKit.dim(this, R.dimen.gzh_item_gap_tight);
        content.addView(uiScaleSlider, sliderLp);

        UiKit.button(content, UiKit.BTN_PRIMARY, R.drawable.ic_gzh_check,
            getString(R.string.setup_button_apply_text_size), () -> {
                writeUiScalePercent((int) uiScaleSlider.getValue());
                Toast.makeText(this, R.string.setup_toast_text_size_saved, Toast.LENGTH_LONG).show();
            });

        UiKit.helpText(content, getString(R.string.setup_text_size_help));
    }

    private void updateUiScaleLabel(int percent) {
        if (uiScaleLabel != null) {
            uiScaleLabel.setText(getString(R.string.setup_text_size_label, percent));
        }
    }

    // GeneralsX @feature Android port render-backend picker 07/09/2026 -
    // three render backends this branch supports, replacing the previous
    // adb-only GENERALSX_RENDER_BACKEND/GENERALSX_GLES_ANGLE env vars with a
    // real in-app choice (most users have no adb at all). Read/written the
    // same way as CUSTOM_DRIVER_CFG_NAME above: a plain-text marker file in
    // getFilesDir(), one line, read natively by SDL3Main.cpp's
    // UseVulkanBackend()/UseANGLE() (and duplicated in gles_pipeline.cpp's
    // initContext() -- see its comment for why) before the game's video
    // subsystem starts.
    private static final String RENDER_BACKEND_CFG_NAME = "render_backend.cfg";
    private static final String RENDER_BACKEND_VULKAN = "vulkan";
    private static final String RENDER_BACKEND_GLES = "gles";
    private static final String RENDER_BACKEND_GLES_ANGLE = "gles_angle";

    private TextView renderBackendStatusView;

    // No config file yet (fresh install) means "whatever UseVulkanBackend()/
    // UseANGLE() default to today when their env vars are unset" -- GLES,
    // per SDL3Main.cpp's own comment on why this branch defaults there.
    // Deliberately NOT auto-detecting "the best backend for this device"
    // here: preserves today's actual behavior for existing installs instead
    // of silently switching anyone's renderer the next time Setup runs.
    private String getRenderBackendChoice() {
        File cfg = new File(getFilesDir(), RENDER_BACKEND_CFG_NAME);
        if (!cfg.isFile()) {
            return RENDER_BACKEND_GLES;
        }
        String value = readFirstLine(cfg);
        if (RENDER_BACKEND_VULKAN.equals(value) || RENDER_BACKEND_GLES_ANGLE.equals(value)) {
            return value;
        }
        return RENDER_BACKEND_GLES;
    }

    // GeneralsX @feature Android port 06/09/2026 The GPU's own name, as its driver
    // reports it. This is the one piece of information the render-backend choice
    // actually turns on -- every Vulkan corruption report on this project so far has
    // been a driver family, not a device model (PowerVR BXM, Samsung Xclipse) -- and
    // until now the launcher asked the user to choose without showing it to them.
    //
    // Deliberately shown rather than acted on. Auto-switching the renderer from a
    // string match would be guesswork wearing a confident face: the same family can
    // ship a fixed driver in a later Android release, and silently moving someone off
    // a backend that works for them is worse than letting them read the name and
    // decide. See getRenderBackendChoice()'s comment on the same principle.
    private static String cachedGpuName;

    private String detectGpuName() {
        if (cachedGpuName != null) {
            return cachedGpuName;
        }
        cachedGpuName = "";
        android.opengl.EGLDisplay display = android.opengl.EGL14.EGL_NO_DISPLAY;
        android.opengl.EGLContext context = android.opengl.EGL14.EGL_NO_CONTEXT;
        android.opengl.EGLSurface surface = android.opengl.EGL14.EGL_NO_SURFACE;
        try {
            display = android.opengl.EGL14.eglGetDisplay(android.opengl.EGL14.EGL_DEFAULT_DISPLAY);
            if (display == android.opengl.EGL14.EGL_NO_DISPLAY) {
                return cachedGpuName;
            }
            int[] version = new int[2];
            if (!android.opengl.EGL14.eglInitialize(display, version, 0, version, 1)) {
                return cachedGpuName;
            }
            int[] cfgAttribs = {
                android.opengl.EGL14.EGL_RENDERABLE_TYPE, android.opengl.EGL14.EGL_OPENGL_ES2_BIT,
                android.opengl.EGL14.EGL_SURFACE_TYPE, android.opengl.EGL14.EGL_PBUFFER_BIT,
                android.opengl.EGL14.EGL_NONE
            };
            android.opengl.EGLConfig[] configs = new android.opengl.EGLConfig[1];
            int[] numConfigs = new int[1];
            if (!android.opengl.EGL14.eglChooseConfig(display, cfgAttribs, 0, configs, 0, 1, numConfigs, 0)
                    || numConfigs[0] == 0) {
                return cachedGpuName;
            }
            int[] ctxAttribs = { android.opengl.EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, android.opengl.EGL14.EGL_NONE };
            context = android.opengl.EGL14.eglCreateContext(display, configs[0],
                android.opengl.EGL14.EGL_NO_CONTEXT, ctxAttribs, 0);
            if (context == android.opengl.EGL14.EGL_NO_CONTEXT) {
                return cachedGpuName;
            }
            int[] surfAttribs = {
                android.opengl.EGL14.EGL_WIDTH, 1,
                android.opengl.EGL14.EGL_HEIGHT, 1,
                android.opengl.EGL14.EGL_NONE
            };
            surface = android.opengl.EGL14.eglCreatePbufferSurface(display, configs[0], surfAttribs, 0);
            if (surface == android.opengl.EGL14.EGL_NO_SURFACE) {
                return cachedGpuName;
            }
            if (!android.opengl.EGL14.eglMakeCurrent(display, surface, surface, context)) {
                return cachedGpuName;
            }
            String renderer = android.opengl.GLES20.glGetString(android.opengl.GLES20.GL_RENDERER);
            if (renderer != null && !renderer.isEmpty()) {
                cachedGpuName = renderer;
            }
        } catch (Throwable t) {
            // A launcher must never fail to open because a driver misbehaved while
            // being asked its own name.
            cachedGpuName = "";
        } finally {
            try {
                if (display != android.opengl.EGL14.EGL_NO_DISPLAY) {
                    android.opengl.EGL14.eglMakeCurrent(display, android.opengl.EGL14.EGL_NO_SURFACE,
                        android.opengl.EGL14.EGL_NO_SURFACE, android.opengl.EGL14.EGL_NO_CONTEXT);
                    if (surface != android.opengl.EGL14.EGL_NO_SURFACE) {
                        android.opengl.EGL14.eglDestroySurface(display, surface);
                    }
                    if (context != android.opengl.EGL14.EGL_NO_CONTEXT) {
                        android.opengl.EGL14.eglDestroyContext(display, context);
                    }
                    android.opengl.EGL14.eglTerminate(display);
                }
            } catch (Throwable ignored) {
                // nothing useful to do here
            }
        }
        return cachedGpuName;
    }

    // Short, segment-sized versions of the same three names. All three are
    // brand names, so every locale carries the same text -- they exist as
    // string resources rather than literals so a locale that needs to
    // transliterate them (Arabic, Farsi, Korean) can.
    private String shortRenderBackendLabel(String choice) {
        switch (choice) {
            case RENDER_BACKEND_VULKAN:
                return getString(R.string.setup_render_backend_vulkan_short);
            case RENDER_BACKEND_GLES_ANGLE:
                return getString(R.string.setup_render_backend_gles_angle_short);
            default:
                return getString(R.string.setup_render_backend_gles_short);
        }
    }

    private String renderBackendLabel(String choice) {
        switch (choice) {
            case RENDER_BACKEND_VULKAN:
                return getString(R.string.setup_render_backend_vulkan);
            case RENDER_BACKEND_GLES_ANGLE:
                return getString(R.string.setup_render_backend_gles_angle);
            default:
                return getString(R.string.setup_render_backend_gles);
        }
    }

    // Default first: the order is a recommendation in itself, and Vulkan has
    // not been the default since this branch started shipping GLES builds.
    private static final String[] RENDER_BACKEND_CHOICES = {
        RENDER_BACKEND_GLES, RENDER_BACKEND_GLES_ANGLE, RENDER_BACKEND_VULKAN
    };

    // GeneralsX @feature Android port launcher-ui-2026 08/09/2026 The three
    // backends were behind a "Change Render Backend" button that opened a
    // single-choice dialog -- two taps and a modal to see what you already
    // had selected. They are three fixed, mutually exclusive choices, which
    // is exactly what a segmented button row is for: the current one is
    // visible without touching anything, and switching is one tap.
    //
    // The dialog's own strings (setup_button_change_render_backend,
    // setup_render_backend_dialog_title) are deliberately left in the string
    // resources: they are still translated in every locale and the dialog is
    // one commit away if this ever needs to go back.
    private void buildRenderBackendSection(LinearLayout root) {
        LinearLayout content = UiKit.card(root);
        renderBackendStatusView = UiKit.sectionHeader(content, R.drawable.ic_gzh_display,
            getString(R.string.setup_card_render_backend), true);
        renderBackendStatusView.setText(shortRenderBackendLabel(getRenderBackendChoice()));
        renderBackendStatusView.setContentDescription(getString(R.string.setup_render_backend_status,
            renderBackendLabel(getRenderBackendChoice())));

        String current = getRenderBackendChoice();
        int currentIndex = 0;
        CharSequence[] labels = new CharSequence[RENDER_BACKEND_CHOICES.length];
        for (int i = 0; i < RENDER_BACKEND_CHOICES.length; i++) {
            labels[i] = shortRenderBackendLabel(RENDER_BACKEND_CHOICES[i]);
            if (RENDER_BACKEND_CHOICES[i].equals(current)) {
                currentIndex = i;
            }
        }
        final String initial = current;
        com.google.android.material.button.MaterialButtonToggleGroup group =
            UiKit.segmented(content, labels, currentIndex, index -> {
            String picked = RENDER_BACKEND_CHOICES[index];
            if (picked.equals(initial)) {
                return;  // programmatic/no-op selection: nothing to save
            }
            onPickRenderBackend(picked);
        });
        // Keeps the picker's own (translated) prompt as what a screen reader
        // announces for the row of three brand names.
        group.setContentDescription(getString(R.string.setup_render_backend_dialog_title));

        // The full sentence stays available under the segments: the short
        // segment labels are brand names, this is the one that says which is
        // the default and what "Current:" means.
        UiKit.supporting(content, getString(R.string.setup_render_backend_status,
            renderBackendLabel(current)));

        String gpu = detectGpuName();
        if (!gpu.isEmpty()) {
            UiKit.chip(content, R.drawable.ic_gzh_chip,
                getString(R.string.setup_render_backend_gpu, gpu),
                R.color.gzh_on_surface, R.color.gzh_surface_container_high);
        }

        UiKit.helpText(content, getString(R.string.setup_render_backend_help));
    }

    private void onPickRenderBackend(String choice) {
        File cfg = new File(getFilesDir(), RENDER_BACKEND_CFG_NAME);
        try (java.io.FileWriter w = new java.io.FileWriter(cfg, false)) {
            w.write(choice);
            w.write("\n");
        } catch (java.io.IOException e) {
            Toast.makeText(this, getString(R.string.setup_toast_render_backend_failed, e.getMessage()), Toast.LENGTH_LONG).show();
            return;
        }
        Toast.makeText(this, R.string.setup_toast_render_backend_saved, Toast.LENGTH_LONG).show();
        // The Custom Vulkan Driver / DXVK Config cards are only relevant for
        // the Vulkan backend -- rebuilding this one page shows/hides them
        // immediately, without the full recreate() (and the jump back to
        // Home) the old dialog needed.
        showTab(TAB_GRAPHICS);
    }

    // GeneralsX @feature Android port 10/07/2026 Optional custom Vulkan
    // driver (e.g. Mesa Turnip for Adreno GPUs), loaded natively via
    // libadrenotools -- see TryLoadCustomVulkanDriver() in SDL3Main.cpp.
    // Package format matches the convention Winlator/AetherSX2/PPSSPP all
    // use: a .zip containing meta.json (schemaVersion/name/description/
    // author/packageVersion/vendor/driverVersion/minApi/libraryName) plus
    // the driver .so (and any dependency .so's) alongside it. We never
    // bundle a driver ourselves -- the user supplies one (e.g. from
    // K11MCH1/AdrenoToolsDrivers or The412Banner/Banners-Turnip on GitHub),
    // matching every other app that uses this technique.
    private static final String CUSTOM_DRIVER_DIR_NAME = "custom_driver";
    private static final String CUSTOM_DRIVER_CFG_NAME = "custom_driver.cfg";
    // GeneralsX @feature Android port 13/07/2026 Marks that custom_driver.cfg
    // was populated by applyRecommendedDriverIfNeeded() rather than by the
    // user importing their own .zip -- lets the status text and the
    // "reset" button distinguish "we picked this for you" from "you chose
    // this", without changing anything on the native loading side (both
    // cases are the same custom_driver.cfg/custom_driver/ that
    // TryLoadCustomVulkanDriver() in SDL3Main.cpp already reads).
    private static final String CUSTOM_DRIVER_AUTO_MARKER_NAME = "custom_driver.auto";
    // Bundled fallback driver (staged by scripts/build/android/fetch-turnip.sh
    // into this asset folder at build time) -- see applyRecommendedDriverIfNeeded().
    private static final String DEFAULT_DRIVER_ASSET_DIR = "default_driver";
    private static final int REQUEST_IMPORT_DRIVER = 1002;
    private static final int REQUEST_PICK_BASE_GENERALS = 1003;

    private TextView customDriverStatusView;

    private void buildCustomDriverSection(LinearLayout root) {
        LinearLayout content = UiKit.card(root);
        UiKit.sectionHeader(content, R.drawable.ic_gzh_chip,
            getString(R.string.setup_card_vulkan_driver), false);

        customDriverStatusView = UiKit.supporting(content, customDriverStatusText());

        UiKit.button(content, UiKit.BTN_TONAL, R.drawable.ic_gzh_download,
            getString(R.string.setup_button_import_driver), this::onImportCustomDriver);
        UiKit.button(content, UiKit.BTN_DANGER, R.drawable.ic_gzh_refresh,
            getString(R.string.setup_button_reset_driver), this::onClearCustomDriver);

        UiKit.helpText(content, getString(R.string.setup_driver_help));
    }

    private String customDriverStatusText() {
        File cfg = new File(getFilesDir(), CUSTOM_DRIVER_CFG_NAME);
        if (!cfg.isFile()) {
            return getString(R.string.setup_driver_status_none);
        }
        String driverName = readFirstLine(cfg);
        boolean isAuto = new File(getFilesDir(), CUSTOM_DRIVER_AUTO_MARKER_NAME).isFile();
        String name = driverName != null ? driverName : getString(R.string.setup_driver_unknown);
        return getString(isAuto ? R.string.setup_driver_status_auto : R.string.setup_driver_status_manual, name);
    }

    // GeneralsX @feature Android port 13/07/2026 Auto-applies the bundled
    // Turnip driver on Adreno phones whose stock driver reports less than
    // Vulkan 1.3, without touching anything if the user already imported
    // their own driver or the phone doesn't need help. Called on every
    // Setup launch (cheap no-op once satisfied) and again from
    // onClearCustomDriver() so "reset" actually restores the recommended
    // state instead of just going blank.
    private void applyRecommendedDriverIfNeeded() {
        try {
            if (new File(getFilesDir(), CUSTOM_DRIVER_CFG_NAME).isFile()) {
                return;  // already configured (auto or user) -- leave it alone
            }
            if (deviceReportsVulkan13()) {
                return;  // stock driver already handles what DXVK needs
            }
            File destDir = new File(getFilesDir(), CUSTOM_DRIVER_DIR_NAME);
            deleteRecursive(destDir);
            if (!copyDriverAssetTree(DEFAULT_DRIVER_ASSET_DIR, destDir)) {
                return;  // no bundled driver in this build -- nothing to apply
            }
            File metaFile = new File(destDir, "meta.json");
            if (!metaFile.isFile()) {
                deleteRecursive(destDir);
                return;
            }
            String libraryName;
            try {
                org.json.JSONObject meta = new org.json.JSONObject(readWholeFile(metaFile));
                libraryName = meta.optString("libraryName", "");
            } catch (Exception e) {
                libraryName = "";
            }
            if (libraryName.isEmpty() || !new File(destDir, libraryName).isFile()) {
                deleteRecursive(destDir);
                return;
            }
            File cfg = new File(getFilesDir(), CUSTOM_DRIVER_CFG_NAME);
            try (java.io.FileWriter w = new java.io.FileWriter(cfg, false)) {
                w.write(libraryName);
                w.write("\n");
            }
            new File(getFilesDir(), CUSTOM_DRIVER_AUTO_MARKER_NAME).createNewFile();
        } catch (Exception e) {
            // Never let driver auto-selection take Setup down with it --
            // worst case the phone's stock driver loads, same as before
            // this feature existed.
        }
    }

    // FEATURE_VULKAN_HARDWARE_VERSION's reported "version" is a Vulkan
    // version int using the same VK_MAKE_API_VERSION encoding as the C API;
    // VK_API_VERSION_1_3 is (1<<22)|(3<<12) = 0x00403000. No feature entry
    // at all (some devices/emulators) is treated as "can't confirm 1.3" so
    // the recommended driver still gets a chance to help.
    private boolean deviceReportsVulkan13() {
        PackageManager pm = getPackageManager();
        FeatureInfo[] features = pm.getSystemAvailableFeatures();
        if (features == null) {
            return false;
        }
        for (FeatureInfo fi : features) {
            if (fi.name != null && fi.name.equals(PackageManager.FEATURE_VULKAN_HARDWARE_VERSION)) {
                return fi.version >= 0x00403000;
            }
        }
        return false;
    }

    // Recursively copies an assets/ subtree (raw files, not a zip) into
    // destDir. Returns false if the source asset folder doesn't exist/is
    // empty -- lets callers tell "not bundled in this build" apart from a
    // real I/O failure without throwing.
    private boolean copyDriverAssetTree(String assetDir, File destDir) {
        AssetManager assets = getAssets();
        String[] children;
        try {
            children = assets.list(assetDir);
        } catch (java.io.IOException e) {
            return false;
        }
        if (children == null || children.length == 0) {
            return false;
        }
        if (!destDir.mkdirs() && !destDir.isDirectory()) {
            return false;
        }
        for (String child : children) {
            String childAssetPath = assetDir + "/" + child;
            File childDest = new File(destDir, child);
            try {
                String[] grandchildren = assets.list(childAssetPath);
                if (grandchildren != null && grandchildren.length > 0) {
                    if (!copyDriverAssetTree(childAssetPath, childDest)) {
                        return false;
                    }
                    continue;
                }
            } catch (java.io.IOException e) {
                return false;
            }
            try (java.io.InputStream in = assets.open(childAssetPath);
                 java.io.FileOutputStream out = new java.io.FileOutputStream(childDest)) {
                byte[] buf = new byte[65536];
                int n;
                while ((n = in.read(buf)) > 0) {
                    out.write(buf, 0, n);
                }
            } catch (java.io.IOException e) {
                return false;
            }
        }
        return true;
    }

    private void refreshCustomDriverStatus() {
        if (customDriverStatusView != null) {
            customDriverStatusView.setText(customDriverStatusText());
        }
    }

    private void onImportCustomDriver() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{
            "application/zip", "application/x-zip-compressed", "application/octet-stream"});
        try {
            startActivityForResult(intent, REQUEST_IMPORT_DRIVER);
        } catch (Exception e) {
            Toast.makeText(this, getString(R.string.setup_toast_no_file_picker, e.getMessage()), Toast.LENGTH_LONG).show();
        }
    }

    private void onClearCustomDriver() {
        new File(getFilesDir(), CUSTOM_DRIVER_CFG_NAME).delete();
        new File(getFilesDir(), CUSTOM_DRIVER_AUTO_MARKER_NAME).delete();
        deleteRecursive(new File(getFilesDir(), CUSTOM_DRIVER_DIR_NAME));
        applyRecommendedDriverIfNeeded();
        refreshCustomDriverStatus();
        boolean autoApplied = new File(getFilesDir(), CUSTOM_DRIVER_AUTO_MARKER_NAME).isFile();
        Toast.makeText(this, autoApplied
            ? R.string.setup_toast_reset_auto
            : R.string.setup_toast_reset_stock,
            Toast.LENGTH_SHORT).show();
    }

    // customDriverDir passed to adrenotools_open_libvulkan() MUST NOT be on
    // sdcard/external storage (dlopen refuses world-writable paths) -- unzip
    // straight into getFilesDir() (app-private internal storage), the same
    // directory SDL_GetAndroidInternalStoragePath() resolves to in native code.
    private void importCustomDriver(Uri uri) {
        File destDir = new File(getFilesDir(), CUSTOM_DRIVER_DIR_NAME);
        File tmpDir = new File(getFilesDir(), CUSTOM_DRIVER_DIR_NAME + ".tmp");
        deleteRecursive(tmpDir);
        if (!tmpDir.mkdirs()) {
            Toast.makeText(this, R.string.setup_toast_driver_import_no_tmp, Toast.LENGTH_LONG).show();
            return;
        }

        try (java.io.InputStream in = getContentResolver().openInputStream(uri);
             java.util.zip.ZipInputStream zip = new java.util.zip.ZipInputStream(in)) {
            java.util.zip.ZipEntry entry;
            byte[] buf = new byte[8192];
            String tmpCanonical = tmpDir.getCanonicalPath();
            while ((entry = zip.getNextEntry()) != null) {
                File out = new File(tmpDir, entry.getName()).getCanonicalFile();
                // Zip-slip guard: never let an archive entry write outside tmpDir.
                if (!out.getPath().equals(tmpCanonical) && !out.getPath().startsWith(tmpCanonical + File.separator)) {
                    throw new java.io.IOException("zip entry escapes target folder: " + entry.getName());
                }
                if (entry.isDirectory()) {
                    out.mkdirs();
                    continue;
                }
                File parent = out.getParentFile();
                if (parent != null) {
                    parent.mkdirs();
                }
                try (java.io.FileOutputStream fos = new java.io.FileOutputStream(out)) {
                    int n;
                    while ((n = zip.read(buf)) > 0) {
                        fos.write(buf, 0, n);
                    }
                }
            }
        } catch (Exception e) {
            deleteRecursive(tmpDir);
            Toast.makeText(this, getString(R.string.setup_toast_driver_import_failed, e.getMessage()), Toast.LENGTH_LONG).show();
            return;
        }

        File metaFile = findFileByName(tmpDir, "meta.json");
        if (metaFile == null) {
            deleteRecursive(tmpDir);
            Toast.makeText(this, R.string.setup_toast_driver_import_no_meta, Toast.LENGTH_LONG).show();
            return;
        }

        String libraryName;
        try {
            org.json.JSONObject meta = new org.json.JSONObject(readWholeFile(metaFile));
            libraryName = meta.optString("libraryName", "");
            if (libraryName.isEmpty()) {
                throw new org.json.JSONException("meta.json has no libraryName");
            }
            if (!new File(metaFile.getParentFile(), libraryName).isFile()) {
                throw new org.json.JSONException("meta.json names '" + libraryName + "' but that file isn't in the package");
            }
        } catch (Exception e) {
            deleteRecursive(tmpDir);
            Toast.makeText(this, getString(R.string.setup_toast_driver_import_bad_meta, e.getMessage()), Toast.LENGTH_LONG).show();
            return;
        }

        // The driver .so + meta.json might be nested inside a subfolder of
        // the zip -- move THAT folder into place as custom_driver/ (not
        // tmpDir itself), so the native side's customDriverDir points at
        // exactly the folder containing libraryName.
        File driverSourceDir = metaFile.getParentFile();
        deleteRecursive(destDir);
        boolean moved = driverSourceDir.renameTo(destDir);
        deleteRecursive(tmpDir);  // no-op if driverSourceDir WAS tmpDir (already moved away)
        if (!moved) {
            Toast.makeText(this, R.string.setup_toast_driver_import_no_finalize, Toast.LENGTH_LONG).show();
            return;
        }

        File cfg = new File(getFilesDir(), CUSTOM_DRIVER_CFG_NAME);
        try (java.io.FileWriter w = new java.io.FileWriter(cfg, false)) {
            w.write(libraryName);
            w.write("\n");
        } catch (java.io.IOException e) {
            Toast.makeText(this, getString(R.string.setup_toast_driver_config_failed, e.getMessage()), Toast.LENGTH_LONG).show();
            return;
        }
        // This is an explicit user choice, not the auto-selected default --
        // see applyRecommendedDriverIfNeeded() / CUSTOM_DRIVER_AUTO_MARKER_NAME.
        new File(getFilesDir(), CUSTOM_DRIVER_AUTO_MARKER_NAME).delete();

        refreshCustomDriverStatus();
        Toast.makeText(this, getString(R.string.setup_toast_driver_imported, libraryName), Toast.LENGTH_LONG).show();
    }

    private static File findFileByName(File dir, String name) {
        File[] children = dir.listFiles();
        if (children == null) {
            return null;
        }
        for (File c : children) {
            if (c.isDirectory()) {
                File found = findFileByName(c, name);
                if (found != null) {
                    return found;
                }
            } else if (c.getName().equals(name)) {
                return c;
            }
        }
        return null;
    }

    private static void deleteRecursive(File f) {
        if (f == null || !f.exists()) {
            return;
        }
        if (f.isDirectory()) {
            File[] children = f.listFiles();
            if (children != null) {
                for (File c : children) {
                    deleteRecursive(c);
                }
            }
        }
        f.delete();
    }

    private static String readWholeFile(File f) throws java.io.IOException {
        StringBuilder sb = new StringBuilder();
        try (java.io.BufferedReader r = new java.io.BufferedReader(new java.io.FileReader(f))) {
            char[] buf = new char[4096];
            int n;
            while ((n = r.read(buf)) > 0) {
                sb.append(buf, 0, n);
            }
        }
        return sb.toString();
    }

    private static String readFirstLine(File f) {
        try (java.io.BufferedReader r = new java.io.BufferedReader(new java.io.FileReader(f))) {
            String line = r.readLine();
            return line != null ? line.trim() : null;
        } catch (java.io.IOException e) {
            return null;
        }
    }

    // GeneralsX @feature Android port 31/07/2026 dxvk.conf editor: real-device
    // Mali-G76 performance tuning kept coming back to "edit one line of
    // dxvk.conf on the phone" (e.g. d3d9.samplerAnisotropy), which meant a
    // file manager and manual editing outside the app every time. This edits
    // the actual file the engine reads (SDL3Main.cpp/DXVK read dxvk.conf
    // relative to CWD, i.e. the selected game folder -- see
    // copyBundledRuntimeIfMissing()'s comment above), as raw text rather than
    // a bespoke widget per key, so any current or future DXVK config option
    // works here without launcher code changes, and existing comments in the
    // file round-trip untouched instead of being reparsed away.
    private EditText dxvkConfigEdit;

    // GeneralsX @feature Android port launcher-ui-2026 08/09/2026 The raw
    // EditText is now a Material 3 outlined text field. TextInputLayout picks
    // its style from ?attr/textInputStyle, so an outlined box on a view built
    // in code (rather than inflated from a layout with a style= attribute) is
    // a matter of constructing it against a ContextThemeWrapper that swaps
    // that one attribute -- see ThemeOverlay.GeneralsZH.OutlinedField.
    private void buildDxvkConfigSection(LinearLayout root) {
        LinearLayout content = UiKit.card(root);
        UiKit.sectionHeader(content, R.drawable.ic_gzh_terminal,
            getString(R.string.setup_card_dxvk_config), false);

        ContextThemeWrapper fieldContext =
            new ContextThemeWrapper(this, R.style.ThemeOverlay_GeneralsZH_OutlinedField);
        TextInputLayout field = new TextInputLayout(fieldContext);
        field.setHint(R.string.setup_card_dxvk_config);
        field.setBoxStrokeColor(UiKit.color(this, R.color.gzh_primary));
        field.setHintTextColor(UiKit.tint(this, R.color.gzh_on_surface_variant));
        // Keep the label in its floated position even when the box is empty:
        // loadDxvkConfigIntoEditor() puts the "select a game folder first"
        // prompt in the field's own hint, and an expanded label would sit on
        // top of it.
        field.setExpandedHintEnabled(false);
        float boxRadius = UiKit.dim(this, R.dimen.gzh_radius_field);
        field.setBoxCornerRadii(boxRadius, boxRadius, boxRadius, boxRadius);

        TextInputEditText edit = new TextInputEditText(field.getContext());
        edit.setTypeface(android.graphics.Typeface.MONOSPACE);
        edit.setTextSize(12);
        edit.setTextColor(UiKit.color(this, R.color.gzh_on_surface));
        edit.setMinLines(6);
        edit.setMaxLines(20);
        edit.setGravity(android.view.Gravity.TOP | android.view.Gravity.START);
        edit.setInputType(android.text.InputType.TYPE_CLASS_TEXT
            | android.text.InputType.TYPE_TEXT_FLAG_MULTI_LINE
            | android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        field.addView(edit, new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));
        dxvkConfigEdit = edit;

        LinearLayout.LayoutParams fieldLp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        fieldLp.topMargin = UiKit.dim(this, R.dimen.gzh_item_gap_tight);
        content.addView(field, fieldLp);

        LinearLayout row = UiKit.buttonRow(content);
        UiKit.share(UiKit.button(row, UiKit.BTN_PRIMARY, R.drawable.ic_gzh_save,
            getString(R.string.setup_button_dxvk_config_save), this::onSaveDxvkConfig), true);
        UiKit.share(UiKit.button(row, UiKit.BTN_DANGER, R.drawable.ic_gzh_refresh,
            getString(R.string.setup_button_dxvk_config_reset), this::onResetDxvkConfig), false);

        UiKit.helpText(content, getString(R.string.setup_dxvk_config_help));

        loadDxvkConfigIntoEditor();
    }

    // Live copy the engine actually reads -- lives in the user-selected game
    // folder, same as DefaultOptions.ini and fonts/ (see
    // copyBundledRuntimeIfMissing()).
    private File dxvkConfFile() {
        String gamePath = getSavedGamePath();
        return gamePath != null ? new File(gamePath, "dxvk.conf") : null;
    }

    // Pristine template this build ships, staged into getExternalFilesDir()
    // by GeneralsZHActivity on first run -- same source
    // copyBundledRuntimeIfMissing() copies from when a game folder is first
    // selected.
    private File bundledDxvkConfFile() {
        File root = getExternalFilesDir(null);
        return root != null ? new File(root, "dxvk.conf") : null;
    }

    private void loadDxvkConfigIntoEditor() {
        if (dxvkConfigEdit == null) {
            return;
        }
        File live = dxvkConfFile();
        File source = (live != null && live.isFile()) ? live : bundledDxvkConfFile();
        String text = "";
        if (source != null && source.isFile()) {
            try {
                text = readWholeFile(source);
            } catch (java.io.IOException e) {
                // Leave the editor empty; Save will still work and create a fresh file.
            }
        }
        dxvkConfigEdit.setText(text);
        boolean haveFolder = getSavedGamePath() != null;
        dxvkConfigEdit.setEnabled(haveFolder);
        dxvkConfigEdit.setHint(haveFolder ? null : getString(R.string.setup_dxvk_config_no_folder));
    }

    private void onSaveDxvkConfig() {
        File dest = dxvkConfFile();
        if (dest == null) {
            Toast.makeText(this, R.string.setup_dxvk_config_no_folder, Toast.LENGTH_LONG).show();
            return;
        }
        try (java.io.Writer w = new java.io.FileWriter(dest, false)) {
            w.write(dxvkConfigEdit.getText().toString());
        } catch (java.io.IOException e) {
            Toast.makeText(this, getString(R.string.setup_toast_options_save_failed, e.getMessage()), Toast.LENGTH_LONG).show();
            return;
        }
        Toast.makeText(this, R.string.setup_toast_dxvk_config_saved, Toast.LENGTH_SHORT).show();
    }

    private void onResetDxvkConfig() {
        File bundled = bundledDxvkConfFile();
        if (bundled == null || !bundled.isFile()) {
            Toast.makeText(this, R.string.setup_toast_dxvk_config_no_default, Toast.LENGTH_LONG).show();
            return;
        }
        String text;
        try {
            text = readWholeFile(bundled);
        } catch (java.io.IOException e) {
            Toast.makeText(this, getString(R.string.setup_toast_options_save_failed, e.getMessage()), Toast.LENGTH_LONG).show();
            return;
        }
        dxvkConfigEdit.setText(text);
        File dest = dxvkConfFile();
        if (dest != null) {
            try (java.io.Writer w = new java.io.FileWriter(dest, false)) {
                w.write(text);
            } catch (java.io.IOException e) {
                Toast.makeText(this, getString(R.string.setup_toast_options_save_failed, e.getMessage()), Toast.LENGTH_LONG).show();
                return;
            }
        }
        Toast.makeText(this, R.string.setup_toast_dxvk_config_reset, Toast.LENGTH_SHORT).show();
    }

    // GeneralsX @feature Android port 02/08/2026 Diagnostic marker toggles:
    // GXTrace.h (gx_trace.txt/gx_perf.txt) and SDL3Main.cpp (dxvk_hud.txt/
    // dxvk_validation.txt/dxvk_verbose_log.txt) all gate opt-in logging
    // behind a plain marker file dropped into the selected game folder,
    // checked relative to CWD after the engine chdir()s there --
    // deliberately no-adb, no-rebuild, so a tester can enable them (see
    // docs/port/ANDROID_PORT.md's "Diagnostic marker files" table, the
    // canonical list this mirrors). In practice almost nobody who isn't
    // already comfortable with a file manager knows to create an empty file
    // with an exact name, so this just does it for them: each switch
    // creates/deletes the marker directly, no new native code needed since
    // the engine side only ever checked "does this file exist", never its
    // contents. Each switch gets its own plain-language title AND a "when to
    // turn this on" description (translated, not just the raw filename) --
    // the filename itself still appears at the end of the description in
    // parentheses so a tester can match it up with exact instructions from
    // an issue reporter/maintainer.
    private static final String[] DIAGNOSTIC_MARKERS = {
        "gx_trace.txt", "gx_perf.txt", "gx_audio_trace.txt", "gx_touch_debug.txt", "dxvk_hud.txt",
        "dxvk_validation.txt", "dxvk_verbose_log.txt"
    };
    private static final int[] DIAGNOSTIC_TITLES = {
        R.string.setup_switch_gx_trace, R.string.setup_switch_gx_perf,
        R.string.setup_switch_gx_audio_trace, R.string.setup_switch_touch_debug,
        R.string.setup_switch_dxvk_hud, R.string.setup_switch_dxvk_validation,
        R.string.setup_switch_dxvk_verbose_log
    };
    private static final int[] DIAGNOSTIC_DESCRIPTIONS = {
        R.string.setup_switch_gx_trace_desc, R.string.setup_switch_gx_perf_desc,
        R.string.setup_switch_gx_audio_trace_desc, R.string.setup_switch_touch_debug_desc,
        R.string.setup_switch_dxvk_hud_desc, R.string.setup_switch_dxvk_validation_desc,
        R.string.setup_switch_dxvk_verbose_log_desc
    };
    private final SwitchCompat[] diagnosticSwitches = new SwitchCompat[DIAGNOSTIC_MARKERS.length];

    // GeneralsX @feature Android port launcher-ui-2026 08/09/2026 Each toggle
    // used to be a SwitchCompat whose LABEL was the title, with the "when to
    // turn this on" paragraph as a separate, differently-indented TextView
    // underneath -- so a long translated title wrapped around the switch and
    // the two halves of one control drifted apart. They are now proper
    // Material 3 list rows: title and description in a text column, the
    // switch pinned to the end edge, a hairline between rows.
    private void buildDiagnosticsSection(LinearLayout root) {
        LinearLayout content = UiKit.card(root);
        UiKit.sectionHeader(content, R.drawable.ic_gzh_wrench,
            getString(R.string.setup_card_diagnostics), false);
        UiKit.supporting(content, getString(R.string.setup_diagnostics_help));

        diagnosticsNoFolderHint = UiKit.chip(content, R.drawable.ic_gzh_info,
            getString(R.string.setup_diagnostics_no_folder),
            R.color.gzh_status_warn, R.color.gzh_surface_container_high);

        for (int i = 0; i < DIAGNOSTIC_MARKERS.length; i++) {
            if (i > 0) {
                UiKit.divider(content);
            }
            diagnosticSwitches[i] = UiKit.switchRow(content,
                getString(DIAGNOSTIC_TITLES[i]), getString(DIAGNOSTIC_DESCRIPTIONS[i]));
        }

        refreshDiagnosticsSwitches();
    }

    private TextView diagnosticsNoFolderHint;

    private File diagnosticMarkerFile(String name) {
        String gamePath = getSavedGamePath();
        return gamePath != null ? new File(gamePath, name) : null;
    }

    private void setDiagnosticMarker(String name, boolean enabled) {
        File marker = diagnosticMarkerFile(name);
        if (marker == null) {
            return;
        }
        if (enabled) {
            try {
                marker.createNewFile();
            } catch (java.io.IOException e) {
                Toast.makeText(this, getString(R.string.setup_toast_options_save_failed, e.getMessage()), Toast.LENGTH_LONG).show();
            }
        } else {
            marker.delete();
        }
    }

    private void refreshDiagnosticsSwitches() {
        boolean haveFolder = getSavedGamePath() != null;
        if (diagnosticsNoFolderHint != null) {
            diagnosticsNoFolderHint.setVisibility(haveFolder ? android.view.View.GONE : android.view.View.VISIBLE);
        }
        for (int i = 0; i < DIAGNOSTIC_MARKERS.length; i++) {
            final int index = i;
            SwitchCompat sw = diagnosticSwitches[index];
            if (sw == null) {
                continue;
            }
            File marker = diagnosticMarkerFile(DIAGNOSTIC_MARKERS[index]);
            sw.setOnCheckedChangeListener(null);
            sw.setChecked(marker != null && marker.isFile());
            sw.setEnabled(haveFolder);
            sw.setOnCheckedChangeListener((button, checked) -> setDiagnosticMarker(DIAGNOSTIC_MARKERS[index], checked));
        }
    }

    // GeneralsX @feature Android port 10/07/2026 GeneralsOnline (playgenerals.online)
    // account status -- the actual sign-in flow lives in GeneralsOnlineActivity,
    // this is just a status line + entry point.
    private TextView onlineStatusView;

    private void buildGeneralsOnlineSection(LinearLayout root) {
        LinearLayout content = UiKit.card(root);
        UiKit.sectionHeader(content, R.drawable.ic_gzh_account,
            getString(R.string.setup_card_online), false);

        onlineStatusView = UiKit.supporting(content, null);

        UiKit.button(content, UiKit.BTN_TONAL, R.drawable.ic_gzh_account,
            getString(R.string.setup_button_online_account), () ->
                startActivity(new Intent(this, GeneralsOnlineActivity.class)));
    }

    private void refreshGeneralsOnlineStatus() {
        if (onlineStatusView == null) {
            return;
        }
        String displayName = GeneralsOnlineActivity.getSignedInDisplayName(this);
        onlineStatusView.setText(displayName != null
            ? getString(R.string.setup_online_signed_in, displayName)
            : getString(R.string.setup_online_signed_out));
    }

    private File optionsIniFile() {
        return new File(getFilesDir(), ".local/share/GeneralsX/GeneralsZH/Options.ini");
    }

    private File defaultOptionsIniFile() {
        String gamePath = getSavedGamePath();
        return gamePath != null ? new File(gamePath, "DefaultOptions.ini") : null;
    }

    private int readUiScalePercent() {
        java.util.Map<String, String> prefs = readKeyValueFile(optionsIniFile());
        String val = prefs.get("ResolutionFontAdjustment");
        if (val != null) {
            try {
                return Math.max(0, Math.min(150, Integer.parseInt(val.trim())));
            } catch (NumberFormatException ignored) {
                // Fall through to the engine's own default below.
            }
        }
        return 70;
    }

    private void writeUiScalePercent(int percent) {
        File file = optionsIniFile();

        // TheSuperHackers @bugfix Android port 07/07/2026 SDL3Main.cpp only
        // seeds Options.ini from the game folder's DefaultOptions.ini (full
        // GPU-detail defaults) the FIRST time it doesn't already exist. If we
        // create a bare Options.ini containing only ResolutionFontAdjustment
        // before the user ever launches the game once, that seeding is
        // permanently skipped and they silently lose those defaults. Seed
        // from DefaultOptions.ini ourselves first when the file is new.
        java.util.LinkedHashMap<String, String> prefs;
        if (!file.isFile()) {
            prefs = new java.util.LinkedHashMap<>(readKeyValueFile(defaultOptionsIniFile()));
        } else {
            prefs = new java.util.LinkedHashMap<>(readKeyValueFile(file));
        }
        prefs.put("ResolutionFontAdjustment", String.valueOf(percent));

        writeKeyValueFile(file, prefs);
    }

    private void writeKeyValueFile(File file, java.util.Map<String, String> prefs) {
        File parent = file.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            Toast.makeText(this, R.string.setup_toast_options_dir_failed, Toast.LENGTH_LONG).show();
            return;
        }
        try (java.io.PrintWriter w = new java.io.PrintWriter(new java.io.FileWriter(file, false))) {
            for (java.util.Map.Entry<String, String> e : prefs.entrySet()) {
                w.print(e.getKey());
                w.print(" = ");
                w.print(e.getValue());
                w.print('\n');
            }
        } catch (java.io.IOException e) {
            Toast.makeText(this, getString(R.string.setup_toast_options_save_failed, e.getMessage()), Toast.LENGTH_LONG).show();
        }
    }

    // Matches UserPreferences::load()'s own format exactly ("key = value"
    // lines) so a file the engine already wrote round-trips untouched.
    private static java.util.Map<String, String> readKeyValueFile(File file) {
        java.util.LinkedHashMap<String, String> result = new java.util.LinkedHashMap<>();
        if (file == null || !file.isFile()) {
            return result;
        }
        try (java.io.BufferedReader r = new java.io.BufferedReader(new java.io.FileReader(file))) {
            String line;
            while ((line = r.readLine()) != null) {
                int eq = line.indexOf('=');
                if (eq < 0) continue;
                String key = line.substring(0, eq).trim();
                String val = line.substring(eq + 1).trim();
                if (key.isEmpty() || val.isEmpty()) continue;
                result.put(key, val);
            }
        } catch (java.io.IOException ignored) {
            // Treat as empty; caller falls back to defaults.
        }
        return result;
    }

    // GeneralsX @feature Android port game-folder-integrity-check 07/09/2026
    // Plain text made "not set"/"looks valid"/"looks incomplete" too easy to
    // miss right after picking a folder -- the status line is now bold,
    // underlined and colored (green/amber/red) so it reads as a clear
    // verdict at a glance instead of blending into the paragraph around it.
    // The invalid/incomplete cases additionally get a blocking AlertDialog
    // right after picking (see onActivityResult()), since even a colored
    // line can be scrolled past unnoticed but a dialog can't.
    private void refreshStatus() {
        if (statusText == null) {
            return;  // the current page has no status line (see showTab())
        }
        String path = getSavedGamePath();
        SpannableStringBuilder sb = new SpannableStringBuilder();
        if (path == null) {
            sb.append(getString(R.string.setup_status_folder_not_set));
        } else {
            File dir = new File(path);
            boolean valid = isValidGameFolder(dir);
            sb.append(getString(R.string.setup_status_folder_line, path));
            int statusStart = sb.length();
            int statusColorRes;
            if (!valid) {
                sb.append(getString(R.string.setup_status_folder_invalid));
                statusColorRes = R.color.gzh_status_error;
            } else {
                java.util.List<String> issues = findGameFolderIntegrityIssues(dir);
                if (issues.isEmpty()) {
                    sb.append(getString(R.string.setup_status_folder_valid));
                    statusColorRes = R.color.gzh_status_ok;
                } else {
                    sb.append(getString(R.string.setup_status_folder_incomplete, joinLines(issues)));
                    statusColorRes = R.color.gzh_status_error;
                }
            }
            int statusEnd = sb.length();
            sb.setSpan(new ForegroundColorSpan(ContextCompat.getColor(this, statusColorRes)),
                statusStart, statusEnd, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            sb.setSpan(new StyleSpan(Typeface.BOLD), statusStart, statusEnd, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            sb.setSpan(new UnderlineSpan(), statusStart, statusEnd, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

            // Only worth a line when it is actually set: an absent optional
            // setting does not need to occupy space on the screen.
            final String basePath = getBaseGeneralsPath();
            if (basePath != null) {
                sb.append(getString(R.string.setup_status_base_generals_line, basePath));
            }
        }
        // GeneralsX @feature Android port launcher-ui-2026 08/09/2026 The
        // "your logs live in private storage" paragraph used to be glued to
        // the end of this verdict, burying it. It now labels the View Logs
        // row on the Tools page (buildLogsSection()), where it is actually
        // about to be acted on.
        int end = sb.length();
        while (end > 0 && Character.isWhitespace(sb.charAt(end - 1))) {
            end--;
        }
        statusText.setText(sb.subSequence(0, end));
        updateGameLanguageStatusView();
    }

    // String.join() is API 26; this launcher's UI code deliberately stays
    // inside the API 24 surface (see the minSdk note in android/app/build.gradle),
    // so the one place that needed it gets a three-line equivalent instead of
    // a version gate.
    private static String joinLines(java.util.List<String> lines) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < lines.size(); i++) {
            if (i > 0) {
                sb.append('\n');
            }
            sb.append(lines.get(i));
        }
        return sb.toString();
    }

    private void showFolderProblemDialog(String message) {
        showFolderProblemDialog(message, false);
    }

    // GeneralsX @feature Android port 06/09/2026 When what is missing is the
    // base game specifically, listing the filenames is only half an answer:
    // the other half is that they do not have to be moved at all, since Setup
    // can be pointed at wherever they already live. Offer that here rather
    // than leaving it to be discovered among the buttons further down.
    private void showFolderProblemDialog(String message, boolean offerBasePicker) {
        android.app.AlertDialog.Builder b = new android.app.AlertDialog.Builder(this)
            .setTitle(R.string.setup_dialog_folder_problem_title)
            .setMessage(message)
            .setPositiveButton(android.R.string.ok, null);
        if (offerBasePicker) {
            b.setNeutralButton(R.string.setup_button_select_base_generals,
                (d, which) -> onSelectBaseGeneralsFolder());
        }
        b.show();
    }


    // GeneralsX @feature Android port 09/09/2026 A language's text can live in a .big
    // archive, not just as a loose file, and this used to look only for the loose one.
    //
    // The engine never cared: g_csfFile is "data/%s/generals.csf" (SDL3Main.cpp) with %s
    // being whatever GetRegistryLanguage() returns, and it is opened through TheFileSystem,
    // which spans loose files AND every mounted .big equally. There is no list of supported
    // languages anywhere in the engine -- any token works if the CSF resolves. So the
    // launcher was the only thing insisting on a loose file, and it told players with a
    // perfectly good language pack that their game folder had nothing.
    //
    // Community translations ship exactly that way. The Russian pack, for instance, is a
    // single 00RussianZH.big whose entries are Data\English\generals.csf plus Russian menu
    // .wnd layouts and textures -- it REPLACES English rather than adding a token, and wins
    // because archives are mounted in name order, first one wins, and "00..." sorts first.
    //
    // Both shapes are now recognised: a loose data/<token>/generals.csf, or that path inside
    // any .big in the game folder (or one level down -- ZH_Generals/ and friends).
    private static boolean gameHasTextFor(String gamePath, String token) {
        if (gamePath == null || token == null) {
            return false;
        }
        // GeneralsX @feature Android port 09/09/2026 Either format counts. generals.str is
        // the plain-text one the engine now prefers and the one a language pack ships as
        // (languages/README.md); generals.csf is the compiled form the original SKUs and the
        // old .big translations carry. Asking only about the .csf would have told a player
        // who just downloaded a pack that their game folder had nothing in it.
        for (String leaf : new String[] { "generals.str", "generals.csf" }) {
            if (new File(gamePath, "data/" + token + "/" + leaf).isFile()) {
                return true;
            }
        }
        File root = new File(gamePath);
        File[] children = root.listFiles();
        for (String leaf : new String[] { "generals.str", "generals.csf" }) {
            String wanted = ("data/" + token + "/" + leaf).toLowerCase(java.util.Locale.ROOT);
            if (bigContainsEntry(root, wanted)) {
                return true;
            }
            if (children != null) {
                for (File child : children) {
                    if (child.isDirectory() && bigContainsEntry(child, wanted)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    private static boolean bigContainsEntry(File dir, String wantedLowerSlash) {
        File[] bigs = dir.listFiles((d, name) -> name.toLowerCase(java.util.Locale.ROOT).endsWith(".big"));
        if (bigs == null) {
            return false;
        }
        for (File big : bigs) {
            if (bigContainsEntry(big, wantedLowerSlash, 0)) {
                return true;
            }
        }
        return false;
    }

    // BIGF layout: "BIGF", archive size (little-endian), then file count and the offset of
    // the first file, both BIG-endian, then one record per file -- offset, size, and a
    // NUL-terminated name. Only the names are read here; nothing is decompressed, so this
    // costs a few kilobytes per archive however large the archive is.
    private static boolean bigContainsEntry(File big, String wantedLowerSlash, int unusedDepth) {
        final boolean[] hit = { false };
        forEachBigEntry(big, name -> {
            if (name.equals(wantedLowerSlash)) {
                hit[0] = true;
            }
        });
        return hit[0];
    }

    // BIGF layout: "BIGF", archive size (little-endian), then file count and the offset of
    // the first file, both BIG-endian, then one record per file -- offset, size, and a
    // NUL-terminated name. Only the names are read here; nothing is decompressed, so this
    // costs a few kilobytes per archive however large the archive is. Names are handed over
    // lowercased with backslashes turned into forward slashes, which is how every caller
    // wants to compare them.
    private static void forEachBigEntry(File big, java.util.function.Consumer<String> visit) {
        try (java.io.DataInputStream in =
                 new java.io.DataInputStream(new java.io.BufferedInputStream(new java.io.FileInputStream(big), 1 << 16))) {
            byte[] magic = new byte[4];
            in.readFully(magic);
            if (magic[0] != 'B' || magic[1] != 'I' || magic[2] != 'G' || magic[3] != 'F') {
                return;
            }
            in.skipBytes(4);                 // archive size, little-endian, not needed
            int count = in.readInt();        // big-endian
            in.readInt();                    // offset of the first file, not needed
            if (count <= 0 || count > 200000) {
                return;                      // not a shape we recognise; do not chew memory over it
            }
            StringBuilder name = new StringBuilder(64);
            for (int i = 0; i < count; i++) {
                in.readInt();                // entry offset
                in.readInt();                // entry size
                name.setLength(0);
                int c;
                while ((c = in.read()) > 0) {
                    name.append((char) (c == '\\' ? '/' : c));
                }
                if (c < 0) {
                    return;                  // truncated archive
                }
                visit.accept(name.toString().toLowerCase(java.util.Locale.ROOT));
            }
        } catch (Exception e) {
            // An unreadable or unfamiliar archive is not an error worth surfacing: it just
            // does not answer the question the caller asked.
        }
    }


    // GeneralsX @feature Android port 09/09/2026 Fetch a language pack straight from the
    // project's own repository.
    //
    // A language is one plain-text file now (languages/<token>/generals.str -- see
    // languages/README.md), so installing one is downloading a file into
    // data/<token>/generals.str inside the game folder. No archive, no unpacking, no
    // asking a player to find a folder with a file manager. And because the pack is a text
    // file in the repository, a translator's pull request reaches players as soon as it is
    // merged.
    private static final String LANGUAGE_PACK_INDEX =
        "https://api.github.com/repos/MYSOREZ/GeneralsZH-Android-Port/contents/languages?ref=main";
    private static final String LANGUAGE_PACK_BASE =
        "https://raw.githubusercontent.com/MYSOREZ/GeneralsZH-Android-Port/main/languages/";

    // GeneralsX @feature Android port 09/09/2026 Fetch every pack there is, and do not ask
    // which one first.
    //
    // Asking was the wrong question twice over. The list it asked from was hardcoded, so a
    // language nobody had thought of when it was written -- Ukrainian, the very next one
    // contributed -- could not be chosen and therefore could not be downloaded. And the
    // question is premature anyway: choosing belongs to the picker below, which lists what is
    // really in the game folder, and there is nothing to choose between until something has
    // been downloaded. So this downloads all of them, the picker then shows what arrived, and
    // the set of languages comes from the repository rather than from this file.
    private void onDownloadLanguagePack() {
        final String gamePath = getSavedGamePath();
        if (gamePath == null) {
            toast(getString(R.string.setup_langpack_no_folder));
            return;
        }
        if (languagePackButton != null) {
            languagePackButton.setEnabled(false);
        }
        toast(getString(R.string.setup_langpack_downloading));

        new Thread(() -> {
            final java.util.List<String> tokens = new java.util.ArrayList<>();
            final String indexError = fetchLanguagePackIndex(tokens);
            final java.util.List<String> installed = new java.util.ArrayList<>();
            final java.util.List<String> failed = new java.util.ArrayList<>();
            if (indexError == null) {
                for (String token : tokens) {
                    if (downloadLanguagePack(token, gamePath) == null) {
                        installed.add(token);
                    } else {
                        failed.add(token);
                    }
                }
            }
            runOnUiThread(() -> {
                if (languagePackButton != null) {
                    languagePackButton.setEnabled(true);
                }
                if (indexError != null) {
                    toast(getString(R.string.setup_langpack_failed, indexError));
                    return;
                }
                if (installed.isEmpty()) {
                    toast(getString(R.string.setup_langpack_failed,
                        failed.isEmpty() ? getString(R.string.setup_langpack_err_none_yet)
                                         : android.text.TextUtils.join(", ", failed)));
                    return;
                }
                updateGameLanguageStatusView();
                StringBuilder names = new StringBuilder();
                for (String token : installed) {
                    if (names.length() > 0) {
                        names.append(", ");
                    }
                    names.append(LocaleHelper.gameTextDisplayName(token));
                }
                toast(getString(R.string.setup_langpack_done_n, installed.size(), names.toString()));
            });
        }, "gx-langpack").start();
    }

    /**
     * Ask the repository which languages exist. Returns null on success, else a reason.
     *
     * The directory listing IS the list -- add languages/<name>/generals.str to the project
     * and it appears here, with nothing in this app to update.
     */
    private String fetchLanguagePackIndex(java.util.List<String> out) {
        HttpURLConnection conn = null;
        try {
            URL url = new URL(LANGUAGE_PACK_INDEX);
            conn = (HttpURLConnection) url.openConnection();
            conn.setConnectTimeout(15000);
            conn.setReadTimeout(30000);
            conn.setRequestProperty("Accept", "application/vnd.github+json");
            final int status = conn.getResponseCode();
            if (status < 200 || status >= 300) {
                return "HTTP " + status;
            }
            StringBuilder body = new StringBuilder();
            try (java.io.BufferedReader r = new java.io.BufferedReader(
                     new java.io.InputStreamReader(conn.getInputStream(), "UTF-8"))) {
                String line;
                while ((line = r.readLine()) != null) {
                    body.append(line);
                }
            }
            org.json.JSONArray entries = new org.json.JSONArray(body.toString());
            for (int i = 0; i < entries.length(); i++) {
                org.json.JSONObject entry = entries.getJSONObject(i);
                if ("dir".equals(entry.optString("type"))) {
                    out.add(entry.optString("name"));
                }
            }
            return out.isEmpty() ? getString(R.string.setup_langpack_err_none_yet) : null;
        } catch (Exception e) {
            String msg = e.getMessage();
            return (msg == null || msg.isEmpty()) ? e.getClass().getSimpleName() : msg;
        } finally {
            if (conn != null) {
                conn.disconnect();
            }
        }
    }

    /** @return null on success, else a short reason to put in front of the user. */
    private String downloadLanguagePack(String token, String gamePath) {
        HttpURLConnection conn = null;
        // Written beside the real file and renamed at the end: a half-downloaded
        // generals.str in place would leave the game with a truncated string table, which
        // fails in a far more confusing way than not having the file at all.
        File dir = new File(gamePath, "data/" + token);
        File tmp = new File(dir, "generals.str.part");
        File dest = new File(dir, "generals.str");
        try {
            if (!dir.isDirectory() && !dir.mkdirs()) {
                return getString(R.string.setup_langpack_err_mkdir);
            }
            URL url = new URL(LANGUAGE_PACK_BASE + token + "/generals.str");
            conn = (HttpURLConnection) url.openConnection();
            conn.setConnectTimeout(15000);
            conn.setReadTimeout(30000);
            conn.setRequestProperty("Accept", "text/plain");
            final int status = conn.getResponseCode();
            if (status == 404) {
                return getString(R.string.setup_langpack_err_none_yet);
            }
            if (status < 200 || status >= 300) {
                return "HTTP " + status;
            }
            byte[] buf = new byte[16 * 1024];
            long total = 0;
            try (java.io.InputStream in = conn.getInputStream();
                 java.io.FileOutputStream out = new java.io.FileOutputStream(tmp)) {
                int n;
                while ((n = in.read(buf)) > 0) {
                    out.write(buf, 0, n);
                    total += n;
                    if (total > 8L * 1024 * 1024) {
                        return "> 8 MB";   // a string table is a few hundred KB; this is not one
                    }
                }
            }
            if (total == 0) {
                return getString(R.string.setup_langpack_err_empty);
            }
            if (dest.exists() && !dest.delete()) {
                return getString(R.string.setup_langpack_err_replace);
            }
            if (!tmp.renameTo(dest)) {
                return getString(R.string.setup_langpack_err_replace);
            }
            return null;
        } catch (Exception e) {
            String msg = e.getMessage();
            return (msg == null || msg.isEmpty()) ? e.getClass().getSimpleName() : msg;
        } finally {
            if (conn != null) {
                conn.disconnect();
            }
            if (tmp.exists()) {
                tmp.delete();
            }
        }
    }

    private void toast(CharSequence text) {
        android.widget.Toast.makeText(this, text, android.widget.Toast.LENGTH_LONG).show();
    }

    private com.google.android.material.button.MaterialButton languagePackButton;

    private String getSavedGamePath() {
        return getSavedGamePath(this);
    }

    // Public + static so GeneralsZHActivity uses the exact same recovery
    // logic instead of its own copy that only ever checked SharedPreferences.
    static String getSavedGamePath(android.content.Context ctx) {
        SharedPreferences prefs = ctx.getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String path = prefs.getString(PREF_GAME_PATH, null);
        if (path != null) {
            return path;
        }

        // Not in this install's private prefs (fresh install after an
        // uninstall, most likely) -- check the external marker left by a
        // previous install and self-heal by restoring it into prefs.
        String recovered = readExternalMarker();
        if (recovered != null && isValidGameFolder(new File(recovered))) {
            prefs.edit().putString(PREF_GAME_PATH, recovered).apply();
            File nativeMarker = new File(ctx.getFilesDir(), "gamedata_path.txt");
            try (java.io.FileWriter w = new java.io.FileWriter(nativeMarker, false)) {
                w.write(recovered);
                w.write("\n");
            } catch (java.io.IOException e) {
                // Not fatal: native code just won't see the recovered path
                // until the user re-saves it once via Setup.
            }
            return recovered;
        }
        return null;
    }

    private static File externalMarkerFile() {
        File root = Environment.getExternalStorageDirectory();
        return root != null ? new File(root, EXTERNAL_MARKER_NAME) : null;
    }

    static String readExternalMarker() {
        File marker = externalMarkerFile();
        if (marker == null || !marker.isFile()) {
            return null;
        }
        try (java.io.BufferedReader r = new java.io.BufferedReader(new java.io.FileReader(marker))) {
            String line = r.readLine();
            return (line != null && !line.isEmpty()) ? line.trim() : null;
        } catch (java.io.IOException e) {
            return null;
        }
    }

    static boolean isValidGameFolder(File dir) {
        if (dir == null || !dir.isDirectory()) {
            return false;
        }
        for (String name : REQUIRED_GAME_FILES) {
            if (GameDataValidator.named(dir, name) != null) {
                return true;
            }
        }
        return false;
    }

    // GeneralsX @feature Android port game-folder-integrity-check 07/09/2026
    // isValidGameFolder() above only checks that INIZH.big/INI.big EXIST --
    // a truncated/incomplete copy (interrupted transfer, bad extraction,
    // etc.) can pass that check while still crashing the engine deep in
    // INI::loadFileDirectory() the first time it hits a directory whose
    // data never made it into the archive: a real user report hit
    // "[INI] ERROR: No files read from directory 'Data\INI\Default\Weather'"
    // followed by an uncaught C++ exception during GameEngine::init() --
    // well before any graphics/Vulkan code runs, so it isn't a driver bug.
    // The same broken copy also failed the same way in Winlator (a
    // completely different Wine/DXVK compatibility layer), confirming it's
    // the user's own file set, not this port's code.
    //
    // These two checks stay cheap by only reading each .big's small BIGF
    // directory table (a header + a null-terminated path per entry), never
    // the actual multi-hundred-MB file payloads the table points at:
    //   1. Every *.big already present must start with the "BIGF" magic
    //      and be non-zero size (catches an empty/corrupted download).
    //   2. Whichever ini archive is present (INI.big or INIZH.big) must
    //      contain Data/INI/Default/Weather.ini specifically -- the exact
    //      file the real crash above needed, and a reasonable proxy for
    //      "this archive's directory table wasn't truncated partway
    //      through", since an interrupted copy would most likely lose
    //      entries somewhere in the middle of the table, not leave out
    //      exactly and only this one file.
    //
    // BIG format read from this project's own parser --
    // Core/GameEngineDevice/Source/StdDevice/Common/StdBIGFileSystem.cpp,
    // openArchiveFile(): "BIGF" magic at offset 0, file count (big-endian)
    // at offset 8, directory table starting at offset 0x10, each entry is
    // [4-byte offset, big-endian][4-byte size, big-endian][null-terminated
    // backslash-separated path].
    private static final String BIG_CRITICAL_ENTRY = "data\\ini\\default\\weather.ini";

    // GeneralsX @bugfix Android port game-folder-integrity-check 08/30/2026
    // Some retail/Deluxe layouts don't put the base Generals archives (incl.
    // INI.big) next to the ZH ones -- they nest an entire base-game copy in
    // a subfolder instead (confirmed via real "Generals Deluxe" install
    // screenshots: root has INIZH.big + *ZH.big, and a "ZH_Generals"
    // subfolder has a plain INI.big alongside unsuffixed archives). The
    // engine itself already expects layouts like this -- see
    // loadBaseGeneralsAssetsForZH() in StdBIGFileSystem.cpp, which tries a
    // handful of base-game locations including a "ZH_Generals" sibling --
    // but that folder name isn't the only one real installers have used
    // over the years, so rather than hardcode it, scan every immediate
    // subfolder of the selected directory for ini archives. One level deep
    // only: cheap (just a directory listing per subfolder), and matches
    // what the engine itself is willing to look for automatically.
    private java.util.List<String> findGameFolderIntegrityIssues(File dir) {
        java.util.List<String> issues = new java.util.ArrayList<>();
        m_lastCheckWantedBaseGenerals = false;
        if (dir == null || !dir.isDirectory()) {
            return issues;
        }
        java.util.List<File> scanRoots = new java.util.ArrayList<>();
        scanRoots.add(dir);
        File[] subdirs = dir.listFiles(File::isDirectory);
        if (subdirs != null) {
            for (File sub : subdirs) {
                scanRoots.add(sub);
            }
        }

        // GeneralsX @bugfix Android port game-folder-integrity-check 07/09/2026
        // INI.big (base Generals) and INIZH.big (the Zero Hour expansion)
        // both get loaded and merged into one virtual file list by the
        // engine's ArchiveFileSystem -- they don't replace each other, ZH
        // only overrides/adds specific entries on top of the base game's.
        // Weather.ini is base-game content, not something Zero Hour
        // replaces, so on an install with both archives it can legitimately
        // live in INI.big while INIZH.big itself is missing/corrupted --
        // checking only whichever one was found last would have produced a
        // false "missing data" report. Collect every present ini archive
        // (root and any subfolder alike) and only flag a problem if NONE of
        // them have the entry, matching how the actual merged filesystem
        // resolves it.
        java.util.List<File> iniArchives = new java.util.ArrayList<>();
        java.util.Set<String> presentArchives = new java.util.HashSet<>();
        for (File root : scanRoots) {
            File[] bigFiles = root.listFiles((d, name) -> name.toLowerCase(java.util.Locale.ROOT).endsWith(".big"));
            if (bigFiles == null) {
                continue;
            }
            String prefix = (root == dir) ? "" : (root.getName() + "\\");
            for (File f : bigFiles) {
                if (f.length() == 0) {
                    issues.add(getString(R.string.setup_folder_issue_empty_file, prefix + f.getName()));
                    continue;
                }
                if (!hasBigFHeader(f)) {
                    issues.add(getString(R.string.setup_folder_issue_bad_header, prefix + f.getName()));
                    continue;
                }
                String lower = f.getName().toLowerCase(java.util.Locale.ROOT);
                if (lower.equals("ini.big") || lower.equals("inizh.big")) {
                    iniArchives.add(f);
                }
                presentArchives.add(lower);
            }
        }
        if (!iniArchives.isEmpty()) {
            boolean found = false;
            for (File f : iniArchives) {
                if (bigArchiveHasEntry(f, BIG_CRITICAL_ENTRY)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                StringBuilder names = new StringBuilder();
                for (File f : iniArchives) {
                    if (names.length() > 0) {
                        names.append(", ");
                    }
                    File parent = f.getParentFile();
                    boolean nested = parent != null && !parent.equals(dir);
                    names.append(nested ? (parent.getName() + "\\" + f.getName()) : f.getName());
                }
                issues.add(getString(R.string.setup_folder_issue_missing_data, names.toString()));
            }
        }

        // GeneralsX @bugfix Android port game-folder-integrity-check 06/09/2026
        // A copy holding only *ZH.big archives passes every check above -- the
        // archives are all present, valid and complete -- and then renders its
        // terrain and half its units in solid magenta, because Zero Hour is an
        // expansion: its archives carry only what it added or changed, and the
        // original game's artwork lives in the base game's Terrain.big /
        // Textures.big / W3D.big. Traced from a real report: every archive in
        // the folder mounted successfully, yet trdirtroad, trsidewalk,
        // trtwolane, trtraintrack and their neighbours were nowhere in the
        // merged filesystem, so the engine substituted the magenta
        // missing-texture placeholder for each of them.
        //
        // Only flag it when ALL of the base asset archives are absent from
        // every scanned root: a copy missing one of the three is unusual but
        // could still be deliberate, whereas none of the three is
        // unambiguously a Zero-Hour-only folder. Deliberately keyed on the
        // asset archives rather than "any archive without a ZH suffix" --
        // Music.big is commonly present on its own and would mask the problem.
        // A folder chosen as the base-Generals location supplies these instead, so
        // do not go on demanding them here -- the warning would be permanent and
        // wrong for exactly the people who already did the right thing.
        final String basePath = getBaseGeneralsPath();
        final boolean baseFolderCovers =
            basePath != null && missingBaseGeneralsArchives(new File(basePath)).isEmpty();

        StringBuilder missing = new StringBuilder();
        if (!baseFolderCovers) {
            for (String name : BASE_GAME_REQUIRED_ARCHIVES) {
                if (!presentArchives.contains(name.toLowerCase(java.util.Locale.ROOT))) {
                    missing.append("\n  \u2022 ").append(name);
                }
            }
        }
        if (missing.length() > 0) {
            issues.add(getString(R.string.setup_folder_issue_no_base_game, missing.toString()));
            m_lastCheckWantedBaseGenerals = true;
        }

        // GeneralsX @bugfix Android port game-folder-integrity-check 06/09/2026
        // A check for a base-game language archive used to live here, matching
        // English.big / German.big / ... against the list registry.cpp probes.
        // Removed: it fired on a copy that plays perfectly well. Localisations
        // are routinely shipped as mods rather than as the archive a retail
        // installer would have written -- a real one here is named
        // 00RussianZH.big, its leading zeroes chosen so it sorts first and wins
        // the load order -- and no list of expected names can cover that. A
        // check that calls a working setup broken is worse than no check, and
        // what it guarded against costs some text and voices, not a playable
        // game.
        return issues;
    }

    // GeneralsX @bugfix Android port game-folder-integrity-check 06/09/2026
    // The base Generals archives, named as they ship. Zero Hour's own archives
    // all carry a ZH suffix and hold only what the expansion added or changed,
    // so a copy with nothing but *ZH.big passes every other check here -- all
    // archives valid, none truncated -- and then renders the original game's
    // terrain and units as solid magenta. Reported as the exact list of names
    // that are absent, because "your copy is incomplete" leaves the user with
    // nothing to act on.
    //
    // Deliberately limited to archives whose base-game names are certain.
    // Language-specific ones (English.big and the AudioEnglish/SpeechEnglish/
    // W3DEnglish family) are left out: which of them a given release ships
    // varies by SKU and language, and naming one that a perfectly good copy
    // never had would send people hunting for a file that does not exist.
    // A missing language archive also degrades far more gracefully than a
    // missing Textures.big.
    // GeneralsX @bugfix Android port 06/09/2026 TOP LEVEL ONLY, deliberately.
    // This also looked one folder deep, which made Setup more permissive than
    // the engine and produced the worst possible answer: pick the root of a
    // Steam copy, be told the folder is fine because the archives sit in its
    // ZH_Generals child, then watch the game render magenta anyway.
    //
    // The engine cannot see that child. StdLocalFileSystem::getFileListInDirectory()
    // recurses by passing the subfolder's NAME with an empty originalDirectory,
    // so the recursive call resolves it against the working directory instead of
    // the folder being scanned -- fine for the relative scan of the game folder,
    // useless for the absolute path CNC_GENERALS_PATH carries. Until that is
    // fixed in the engine, Setup has to judge a folder by what the engine will
    // really find in it; resolveBaseGeneralsFolder() descends into the child on
    // the user's behalf instead of pretending the engine would.
    java.util.List<String> missingBaseGeneralsArchives(File dir) {
        java.util.Set<String> present = new java.util.HashSet<>();
        if (dir != null && dir.isDirectory()) {
            File[] bigs = dir.listFiles((d, name) ->
                name.toLowerCase(java.util.Locale.ROOT).endsWith(".big"));
            if (bigs != null) {
                for (File f : bigs) {
                    present.add(f.getName().toLowerCase(java.util.Locale.ROOT));
                }
            }
        }
        java.util.List<String> missing = new java.util.ArrayList<>();
        for (String name : BASE_GAME_REQUIRED_ARCHIVES) {
            if (!present.contains(name.toLowerCase(java.util.Locale.ROOT))) {
                missing.add(name);
            }
        }
        return missing;
    }

    // The three archives Zero Hour cannot supply for itself and that a magenta
    // battlefield depends on. A folder without a single one of them is not a
    // base Generals folder, whatever else it happens to contain -- one stray
    // Music.big was enough to fool an earlier version of this check.
    private static final String[] BASE_GAME_ASSET_ARCHIVES = { "terrain.big", "textures.big", "w3d.big" };

    private boolean hasAnyBaseGeneralsAssets(File dir) {
        java.util.List<String> missing = missingBaseGeneralsArchives(dir);
        for (String name : BASE_GAME_ASSET_ARCHIVES) {
            boolean absent = false;
            for (String m : missing) {
                if (m.toLowerCase(java.util.Locale.ROOT).equals(name)) {
                    absent = true;
                    break;
                }
            }
            if (!absent) {
                return true;
            }
        }
        return false;
    }

    private File resolveBaseGeneralsFolder(File picked) {
        if (picked == null || !picked.isDirectory()) {
            return null;
        }
        if (hasAnyBaseGeneralsAssets(picked)) {
            return picked;
        }
        File[] subdirs = picked.listFiles(File::isDirectory);
        if (subdirs != null) {
            java.util.Arrays.sort(subdirs, (a, b) -> a.getName().compareToIgnoreCase(b.getName()));
            for (File sub : subdirs) {
                if (hasAnyBaseGeneralsAssets(sub)) {
                    return sub;
                }
            }
        }
        return null;
    }

    // Set by findGameFolderIntegrityIssues(): whether the problems it just
    // found are ones that pointing Setup at a base-Generals folder would fix.
    private boolean m_lastCheckWantedBaseGenerals = false;

    private static final String[] BASE_GAME_REQUIRED_ARCHIVES = {
        "INI.big", "Terrain.big", "Textures.big", "W3D.big", "Window.big",
        "Shaders.big", "Audio.big", "Speech.big", "Maps.big", "Music.big"
    };

    private static boolean hasBigFHeader(File f) {
        try (java.io.RandomAccessFile raf = new java.io.RandomAccessFile(f, "r")) {
            byte[] magic = new byte[4];
            if (raf.read(magic) != 4) {
                return false;
            }
            return magic[0] == 'B' && magic[1] == 'I' && magic[2] == 'G' && magic[3] == 'F';
        } catch (java.io.IOException e) {
            return false;
        }
    }

    private static int readBigEndianInt(java.io.RandomAccessFile raf) throws java.io.IOException {
        int b0 = raf.read(), b1 = raf.read(), b2 = raf.read(), b3 = raf.read();
        if ((b0 | b1 | b2 | b3) < 0) {
            throw new java.io.EOFException();
        }
        return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
    }

    private static boolean bigArchiveHasEntry(File bigFile, String targetLowerPath) {
        if (!bigFile.isFile() || bigFile.length() < 0x10) {
            return false;
        }
        try (java.io.RandomAccessFile raf = new java.io.RandomAccessFile(bigFile, "r")) {
            if (!hasBigFHeader(bigFile)) {
                return false;
            }
            raf.seek(8);
            int numFiles = readBigEndianInt(raf);
            if (numFiles < 0 || numFiles > 500000) {
                return false;  // sanity guard against a corrupted/garbage header
            }
            raf.seek(0x10);
            byte[] nameBuf = new byte[512];
            for (int i = 0; i < numFiles; i++) {
                raf.skipBytes(8);  // per-entry offset + size, not needed for this check
                int len = 0;
                int b;
                while ((b = raf.read()) > 0) {
                    if (len < nameBuf.length - 1) {
                        nameBuf[len++] = (byte) b;
                    }
                }
                if (b < 0) {
                    return false;  // ran off the end of the file mid-entry -- truncated archive
                }
                String path = new String(nameBuf, 0, len, java.nio.charset.StandardCharsets.US_ASCII)
                    .toLowerCase(java.util.Locale.ROOT);
                if (path.equals(targetLowerPath)) {
                    return true;
                }
            }
        } catch (java.io.IOException e) {
            return false;
        }
        return false;
    }

    private static final int REQUEST_LEGACY_STORAGE_PERMISSION = 1003;

    private void onSelectGameFolder() {
        // GeneralsX @feature Codex 14/09/2026 All folder changes use the guided validator.
        startActivity(new Intent(this, GameDataSetupActivity.class));
    }

    // GeneralsX @feature Android port 06/09/2026 Second picker, same browser,
    // different destination. Deliberately not merged with the game-folder flow:
    // that one validates what it is given as a Zero Hour folder, and this one
    // must accept the opposite -- a folder with base archives and no *ZH.big.
    private void onSelectBaseGeneralsFolder() {
        // GeneralsX @feature Codex 14/09/2026 All folder changes use the guided validator.
        startActivity(new Intent(this, GameDataSetupActivity.class));
    }

    private void onClearBaseGeneralsFolder() {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit().remove(PREF_BASE_GENERALS_PATH).apply();
        new File(getFilesDir(), "generals_base_path.txt").delete();
        Toast.makeText(this, R.string.setup_toast_base_generals_cleared, Toast.LENGTH_LONG).show();
        // Only the Home card changes shape (the "clear" button disappears) --
        // rebuilding that one page is enough, and keeps the user where they
        // are instead of restarting the whole Activity.
        showTab(TAB_HOME);
    }

    String getBaseGeneralsPath() {
        return getSharedPreferences(PREFS_NAME, MODE_PRIVATE).getString(PREF_BASE_GENERALS_PATH, null);
    }

    private void saveBaseGeneralsPath(String path) {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit()
            .putString(PREF_BASE_GENERALS_PATH, path)
            .apply();
        // SDL3Main.cpp reads this plain-text marker on the next launch and turns
        // it into CNC_GENERALS_PATH, which is the first location the engine's
        // base-game search consults. Native code cannot read SharedPreferences,
        // hence the file -- same arrangement as gamedata_path.txt.
        File marker = new File(getFilesDir(), "generals_base_path.txt");
        try (java.io.FileWriter w = new java.io.FileWriter(marker, false)) {
            w.write(path);
            w.write("\n");
        } catch (java.io.IOException e) {
            Toast.makeText(this, getString(R.string.setup_toast_marker_save_failed, e.getMessage()), Toast.LENGTH_LONG).show();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_LEGACY_STORAGE_PERMISSION) {
            boolean granted = grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED;
            if (granted) {
                startActivityForResult(new Intent(this, FolderPickerActivity.class), 1001);
            } else {
                Toast.makeText(this, R.string.setup_toast_storage_permission_denied, Toast.LENGTH_LONG).show();
            }
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 1001 && resultCode == Activity.RESULT_OK && data != null) {
            String path = data.getStringExtra(FolderPickerActivity.EXTRA_SELECTED_PATH);
            if (path != null) {
                saveGamePath(path);
                refreshStatus();
                File dir = new File(path);
                boolean valid = isValidGameFolder(dir);
                // GeneralsX @feature Android port game-folder-integrity-check
                // 07/09/2026 - a Toast auto-dismisses in a couple of seconds
                // and is easy to miss entirely; a real problem here means
                // the game WILL crash on launch, so it gets a blocking
                // dialog the user has to acknowledge instead. The colored
                // status line in refreshStatus() (visible right below) still
                // covers "I want to re-check the status later" without
                // re-triggering the dialog every time the screen redraws.
                if (!valid) {
                    showFolderProblemDialog(getString(R.string.setup_status_folder_invalid).trim());
                } else {
                    java.util.List<String> issues = findGameFolderIntegrityIssues(dir);
                    if (!issues.isEmpty()) {
                        showFolderProblemDialog(getString(R.string.setup_status_folder_incomplete,
                            joinLines(issues)).trim(), m_lastCheckWantedBaseGenerals);
                    } else {
                        Toast.makeText(this, R.string.setup_toast_folder_saved, Toast.LENGTH_LONG).show();
                    }
                }
            }
        } else if (requestCode == REQUEST_PICK_BASE_GENERALS && resultCode == Activity.RESULT_OK && data != null) {
            String path = data.getStringExtra(FolderPickerActivity.EXTRA_SELECTED_PATH);
            if (path != null) {
                // GeneralsX @bugfix Android port 06/09/2026 Judged on the archives
                // that actually carry the original game's artwork -- an earlier
                // version accepted a folder unless ALL ten were absent, which one
                // stray Music.big was enough to defeat.
                //
                // Picking the root of a Steam install is the natural thing to do,
                // so resolve it to the child that really holds them rather than
                // saving a path the engine cannot make use of.
                File resolved = resolveBaseGeneralsFolder(new File(path));
                if (resolved == null) {
                    showFolderProblemDialog(getString(R.string.setup_base_generals_not_here, path));
                } else {
                    saveBaseGeneralsPath(resolved.getAbsolutePath());
                    Toast.makeText(this, getString(R.string.setup_toast_base_generals_saved,
                        resolved.getAbsolutePath()), Toast.LENGTH_LONG).show();
                    showTab(TAB_HOME);
                }
            }
        } else if (requestCode == REQUEST_IMPORT_DRIVER && resultCode == Activity.RESULT_OK && data != null) {
            Uri uri = data.getData();
            if (uri != null) {
                importCustomDriver(uri);
            }
        }
    }

    private void saveGamePath(String path) {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit()
            .putString(PREF_GAME_PATH, path)
            .apply();
        // GeneralsZHActivity/SDL3Main.cpp read this plain-text marker on the
        // NEXT launch (native code has no Android SharedPreferences access).
        File marker = new File(getFilesDir(), "gamedata_path.txt");
        try (java.io.FileWriter w = new java.io.FileWriter(marker, false)) {
            w.write(path);
            w.write("\n");
        } catch (java.io.IOException e) {
            Toast.makeText(this, getString(R.string.setup_toast_marker_save_failed, e.getMessage()), Toast.LENGTH_LONG).show();
        }
        File externalMarker = externalMarkerFile();
        if (externalMarker != null) {
            try (java.io.FileWriter w = new java.io.FileWriter(externalMarker, false)) {
                w.write(path);
                w.write("\n");
            } catch (java.io.IOException e) {
                // Not fatal: uninstall-survival just won't work for this
                // install; the private-prefs/marker-file path above still
                // covers normal in-place updates.
            }
        }
        File bundledRoot = getExternalFilesDir(null);
        if (bundledRoot != null) {
            copyBundledRuntimeIfMissing(bundledRoot, path);
        }
        applyGameLanguageOverride();
    }

    // GeneralsX @bugfix Android port 07/07/2026 dxvk.conf, DefaultOptions.ini,
    // AND the entire fonts/ directory are all read by the engine relative to
    // its CWD (see SDL3Main.cpp and render2dsentence.cpp's
    // Locate_Font_FontConfig, which does access("fonts/<name>.ttf", R_OK) with
    // no absolute-path fallback on Android/iOS). CWD is now whatever folder
    // the user picked — not the external-files-dir path package-android-zh.sh
    // originally extracted the APK's bundled copies into. Missing fonts/
    // specifically means EVERY W3DFont load fails ("load miss" for every
    // single font in the log) and every button in the UI renders with no
    // text at all — copy all three, not just dxvk.conf. Static + takes
    // bundledRoot explicitly so GeneralsZHActivity can also call this on
    // every launch (an already-configured install needs the fix applied
    // retroactively, not just at folder-selection time).
    static void copyBundledRuntimeIfMissing(File bundledRoot, String gameFolderPath) {
        copyFileIfMissing(new File(bundledRoot, "dxvk.conf"), new File(gameFolderPath, "dxvk.conf"));
        copyFileIfMissing(new File(bundledRoot, "DefaultOptions.ini"), new File(gameFolderPath, "DefaultOptions.ini"));
        copyDirIfMissing(new File(bundledRoot, "fonts"), new File(gameFolderPath, "fonts"));
        syncEngineWindowOverrides(bundledRoot, gameFolderPath);
    }

    // GeneralsX @bugfix Android port 02/08/2026 GroupPanel.wnd (the native
    // in-engine unit-group panel) is a loose Window\ override WE inject --
    // see GameWindowManagerScript.cpp's Window\ path resolution -- not a
    // user file, so unlike dxvk.conf/DefaultOptions.ini/fonts/ above it must
    // NOT use copy-if-missing: every edit to it (this exact feature has
    // already gone through several rounds of position/behavior/style fixes)
    // would otherwise silently never reach an existing install, since the
    // very first copy would win forever. Always overwrite this one
    // subdirectory; bundledRoot's own copy is kept fresh the same way, see
    // GeneralsZHActivity.copyAssetTree's ALWAYS_OVERWRITE_PREFIX.
    private static void syncEngineWindowOverrides(File bundledRoot, String gameFolderPath) {
        File srcDir = new File(bundledRoot, "Window");
        File[] children = srcDir.listFiles();
        if (children == null) {
            return;
        }
        File destDir = new File(gameFolderPath, "Window");
        if (!destDir.isDirectory() && !destDir.mkdirs()) {
            return;
        }
        for (File child : children) {
            if (child.isFile()) {
                copyFileOverwrite(child, new File(destDir, child.getName()));
            }
        }
    }

    private static void copyFileOverwrite(File src, File dest) {
        try (java.io.InputStream in = new java.io.FileInputStream(src);
             java.io.OutputStream out = new java.io.FileOutputStream(dest)) {
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
            }
        } catch (java.io.IOException e) {
            // Not fatal: worst case the engine keeps using a stale or
            // missing override, same as before this fix existed.
        }
    }

    private static void copyFileIfMissing(File bundled, File dest) {
        if (dest.exists() || !bundled.exists()) {
            return;
        }
        try (java.io.InputStream in = new java.io.FileInputStream(bundled);
             java.io.OutputStream out = new java.io.FileOutputStream(dest)) {
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
            }
        } catch (java.io.IOException e) {
            // Not fatal: caller falls back to its own defaults without this file.
        }
    }

    private static void copyDirIfMissing(File bundledDir, File destDir) {
        if (destDir.exists() || !bundledDir.isDirectory()) {
            return;
        }
        if (!destDir.mkdirs()) {
            return;
        }
        File[] children = bundledDir.listFiles();
        if (children == null) {
            return;
        }
        for (File child : children) {
            copyFileIfMissing(child, new File(destDir, child.getName()));
        }
    }

    private void onClearGameFolder() {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit().remove(PREF_GAME_PATH).apply();
        new File(getFilesDir(), "gamedata_path.txt").delete();
        new File(getFilesDir(), "game_language.cfg").delete();
        File externalMarker = externalMarkerFile();
        if (externalMarker != null) {
            externalMarker.delete();
        }
        refreshStatus();
        refreshDiagnosticsSwitches();
        Toast.makeText(this, R.string.setup_toast_folder_cleared, Toast.LENGTH_SHORT).show();
    }

    private void onViewLogs() {
        startActivity(new Intent(this, LogViewerActivity.class));
    }

    // GeneralsX @bugfix Android port 31/07/2026 Setup is portrait-first now
    // (see AndroidManifest.xml/onCreate() comments), so launching straight
    // into GeneralsZHActivity (locked landscape) can trigger a real
    // portrait->landscape rotation right as the game's native window-size
    // probe (WW3D::Init()) runs -- previously sidestepped entirely by never
    // letting Setup rotate. Force landscape here and wait for
    // onConfigurationChanged() to confirm the OS has actually applied it
    // before starting the game, instead of guessing with a fixed delay. If
    // we're already landscape (e.g. a tablet, or the user physically
    // rotated the phone), there's nothing to wait for.
    private boolean pendingLaunchAfterRotation = false;

    private void onLaunchGame() {
        if (getResources().getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE) {
            launchGameActivity();
            return;
        }
        pendingLaunchAfterRotation = true;
        setRequestedOrientation(android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (pendingLaunchAfterRotation && newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE) {
            pendingLaunchAfterRotation = false;
            launchGameActivity();
        }
    }

    // GeneralsX @feature XR port Phase 1.0 - the xr flavor stripped
    // GeneralsZHActivity from its manifest, so "Launch Game" there must
    // target the VR entry instead. Package-name check, no BuildConfig
    // dependency; the 2D flavor's target is unchanged.
    private void launchGameActivity() {
        Class<?> target = getPackageName().endsWith(".xr")
                ? XrHelloActivity.class
                : GameDataSetupActivity.class;
        startActivity(new Intent(this, target).putExtra("auto_launch", true));
    }

    private int dp(int value) {
        float density = getResources().getDisplayMetrics().density;
        return (int) (value * density + 0.5f);
    }
}
