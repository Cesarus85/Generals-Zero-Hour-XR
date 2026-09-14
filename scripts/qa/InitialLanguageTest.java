// GeneralsX @test Codex 14/09/2026 Actual shared first-run language policy.
package com.generalsx.zerohour;

public final class InitialLanguageTest {
    private static int checks;
    private static void check(boolean value) {
        ++checks;
        if (!value) throw new AssertionError("language check " + checks);
    }
    public static void main(String[] args) {
        for (String tag : new String[]{"de", "de-DE", "de-AT", "de-CH", "DE", "de_LU", "de-Latn-DE"}) {
            check(InitialLanguage.xrCode(tag) == 0);
            check(InitialLanguage.gameTextToken(tag, true).equals("german"));
            check(InitialLanguage.gameTextToken(tag, false).equals("english"));
        }
        for (String tag : new String[]{"en", "en-US", "fr", "ja", "ar", "zh-CN", "ru", "", null, "invalid"}) {
            check(InitialLanguage.xrCode(tag) == 1);
            check(InitialLanguage.gameTextToken(tag, true).equals("english"));
            check(InitialLanguage.gameTextToken(tag, false).equals("english"));
        }
        System.out.println("PASS " + checks + " first-run language policy checks");
    }
}
