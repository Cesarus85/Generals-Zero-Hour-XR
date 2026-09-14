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

// GeneralsX @feature Android port 13/07/2026 GitHub issue #4: an in-app
// language override for the launcher UI (Setup, folder picker, log viewer),
// independent of the game itself (whose text comes from the user's own
// .big files and isn't something this app controls).
//
// Deliberately NOT using AppCompatDelegate.setApplicationLocales() -- this
// app doesn't otherwise depend on androidx.appcompat, and pulling it in
// just for this would be a heavier dependency than the manual
// attachBaseContext() override below, which works identically on every API
// level this project targets (including pre-33 devices, the actual
// low-end/old-phone audience this port cares about).
//
// "System Default" (no saved preference) intentionally does nothing here:
// the Activity's base context is already configured with the system
// locale, and resource resolution already falls back to the un-suffixed
// values/ (English) when no values-<lang>/ matches -- both halves of "system
// locale, or English if unsupported" are automatic, no override needed.

package com.generalsx.zerohour;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Build;

import java.util.Locale;

final class LocaleHelper {

    static final String PREFS_NAME = "generalszh_setup";
    static final String PREF_LANGUAGE_TAG = "language_tag";

    // GeneralsX @feature Android port 13/07/2026 Keep this in the same order
    // shown in the Setup UI's language picker.
    static final String[] SUPPORTED_TAGS = { "", "en", "ru", "de", "uk", "es", "zh", "fr", "ko", "pl", "pt-BR", "isv", "ar", "fa" };

    private LocaleHelper() {}

    // GeneralsX @feature Codex 14/09/2026 Read the OS configuration, not
    // Locale.getDefault(), which the independent launcher override changes.
    static String systemLanguage() {
        Configuration config = android.content.res.Resources.getSystem().getConfiguration();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            return config.getLocales().isEmpty() ? "en" : config.getLocales().get(0).getLanguage();
        }
        return config.locale == null ? "en" : config.locale.getLanguage();
    }

    static String getSavedLanguageTag(Context ctx) {
        SharedPreferences prefs = ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        return prefs.getString(PREF_LANGUAGE_TAG, "");
    }

    static void setSavedLanguageTag(Context ctx, String tag) {
        ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putString(PREF_LANGUAGE_TAG, tag)
            .apply();
    }

    // Call from every Activity's attachBaseContext(Context) that has
    // localized resources: super.attachBaseContext(LocaleHelper.wrap(base)).
    static Context wrap(Context base) {
        String tag = getSavedLanguageTag(base);
        if (tag == null || tag.isEmpty()) {
            return base;  // "System Default" -- leave the base context alone.
        }
        Locale locale = Locale.forLanguageTag(tag);
        Locale.setDefault(locale);

        Configuration config = new Configuration(base.getResources().getConfiguration());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            config.setLocale(locale);
        } else {
            config.locale = locale;
        }
        return base.createConfigurationContext(config);
    }

    // GeneralsX @feature Android port 13/07/2026 GitHub issue #4 follow-up:
    // maps a launcher UI language tag to the engine's own language-folder
    // token (data/<token>/generals.csf, registry.cpp's GetRegistryLanguage())
    // -- entirely separate namespaces. Tokens for german/french/spanish/
    // chinese/korean/polish/brazilian match tryAutoDetectLanguage()'s own
    // candidate list in registry.cpp (the official Zero Hour SKUs); russian
    // is not an official EA SKU (Zero Hour never shipped one) but the same
    // data/<token>/generals.csf lookup works for a user-supplied fan/licensed
    // translation, e.g. TheSuperHackers/GeneralsRussianLoca's ReleaseUnpacked
    // "Data\Russian\" folder copied in as data/russian/ (lowercase, since
    // this engine build's data/ lookup is case-sensitive). Everything else
    // returns null, meaning "don't touch the game's own language, only this
    // app's UI changed." SetupActivity still requires data/<token>/
    // generals.csf to actually exist in the user's own game folder before
    // ever writing the override -- this mapping alone never forces a
    // language whose data isn't present.
    // GeneralsX @feature Android port 09/09/2026 The game's text language is now a setting of
    // its own, not a shadow of the launcher's UI language.
    //
    // They were one control, and that was wrong in both directions: picking Russian for the
    // launcher silently changed the game's text, and once changed there was no way back to
    // English short of picking English for the launcher too. They are separate questions --
    // someone can want a Russian launcher over an English game, or the reverse -- and with
    // downloadable packs the game side is a list of what is actually installed rather than a
    // fixed mapping.
    static final String PREF_GAME_TEXT_TOKEN = "game_text_token";

    /** Has the player ever answered the question? Absent is not the same as "default". */
    static boolean hasGameTextToken(Context ctx) {
        return ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                  .contains(PREF_GAME_TEXT_TOKEN);
    }

    static String getGameTextToken(Context ctx) {
        return ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                  .getString(PREF_GAME_TEXT_TOKEN, "");
    }

    static void setGameTextToken(Context ctx, String token) {
        ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE).edit()
           .putString(PREF_GAME_TEXT_TOKEN, token == null ? "" : token).apply();
    }

    /** Human name for an engine language token, in that language where it is obvious. */
    static String gameTextDisplayName(String token) {
        if (token == null) {
            return "";
        }
        switch (token) {
            case "english":   return "English";
            case "german":    return "Deutsch";
            case "french":    return "Français";
            case "spanish":   return "Español";
            case "italian":   return "Italiano";
            case "polish":    return "Polski";
            case "brazilian": return "Português (BR)";
            case "russian":   return "Русский";
            case "korean":    return "한국어";
            case "chinese":   return "中文";
            case "ukrainian": return "Українська";
            default:
                // GeneralsX @feature Android port 09/09/2026 A language nobody listed is still
                // a language. Show the folder name with a capital rather than nothing, so a
                // pack contributed after this file was written is still pickable.
                if (token.isEmpty()) {
                    return token;
                }
                return Character.toUpperCase(token.charAt(0)) + token.substring(1);
        }
    }

    static String gameDataLanguageFor(String launcherTag) {
        if (launcherTag == null) {
            return null;
        }
        switch (launcherTag) {
            case "de":    return "german";
            case "es":    return "spanish";
            case "zh":    return "chinese";
            case "ru":    return "russian";  // not an official EA SKU; licensed/fan RU data
            case "fr":    return "french";
            case "ko":    return "korean";
            case "pl":    return "polish";
            case "pt-BR": return "brazilian";
            default:      return null;
        }
    }

    static String displayNameFor(Context ctx, String tag) {
        if (tag == null || tag.isEmpty()) {
            return ctx.getString(R.string.setup_language_system_default);
        }
        switch (tag) {
            case "en": return "English";
            case "ru": return "Русский";
            case "de": return "Deutsch";
            case "uk": return "Українська";
            case "es": return "Español";
            case "zh": return "中文 (简体)";
            case "fr": return "Français";
            case "ko": return "한국어";
            case "pl": return "Polski";
            case "pt-BR": return "Português (Brasil)";
            case "isv": return "Medžuslovjansky";
            case "ar": return "العربية";
            case "fa": return "فارسی";
            default:   return tag;
        }
    }
}
