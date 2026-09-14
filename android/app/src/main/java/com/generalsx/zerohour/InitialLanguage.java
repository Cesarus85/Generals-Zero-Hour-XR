// GeneralsX @feature Codex 14/09/2026 First-run language policy, shared by
// Quest workspace and original-game text setup. Explicit choices win later.
package com.generalsx.zerohour;

import java.util.Locale;

final class InitialLanguage {
    private InitialLanguage() {}

    static int xrCode(String systemTag) {
        String language = systemTag == null ? "" :
                Locale.forLanguageTag(systemTag.replace('_', '-')).getLanguage();
        return "de".equals(language) ? 0 : 1; // XrLanguage: German, English
    }

    static String gameTextToken(String systemTag, boolean hasGermanText) {
        return xrCode(systemTag) == 0 && hasGermanText ? "german" : "english";
    }
}
