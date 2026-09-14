package com.generalsx.zerohour;

import java.io.*;
import java.nio.file.*;

public final class GameDataValidatorTest {
    private static int checks;
    private static void expect(boolean ok, String name) {
        if (!ok) throw new AssertionError(name); checks++;
    }
    private static void archive(File root, String filename, String entry) throws Exception {
        root.mkdirs();
        byte[] name = entry.getBytes("US-ASCII"); int end = 16 + 8 + name.length + 1;
        try (DataOutputStream out = new DataOutputStream(new FileOutputStream(new File(root, filename)))) {
            out.writeInt(0x42494746); out.writeInt(Integer.reverseBytes(end + 1));
            out.writeInt(1); out.writeInt(end); out.writeInt(end); out.writeInt(1);
            out.write(name); out.write(0); out.write(1);
        }
    }
    private static void base(File dir) throws Exception {
        for (String n : GameDataValidator.BASE)
            archive(dir, n, n.equals("INI.big") ? "Data\\INI\\Default\\Weather.ini" : "sample");
    }
    private static void zh(File dir) throws Exception {
        for (String n : GameDataValidator.ZH) archive(dir, n, "sample");
        archive(dir, "EnglishZH.big", "Data\\English\\generals.csf");
    }
    public static void main(String[] args) throws Exception {
        if (args.length > 0) {
            GameDataValidator.Result r = GameDataValidator.check(new File(args[0]), args.length > 1 ? new File(args[1]) : null);
            System.out.println("game=" + r.game + " base=" + r.base + " ready=" + r.ready()
                + " weather=" + r.weather + " text=" + r.language + " missing=" + r.missing + " damaged=" + r.damaged);
            if (!r.ready()) throw new AssertionError("Real game data rejected");
            return;
        }
        File temp = Files.createTempDirectory("generals-data-test").toFile();
        File steam = new File(temp, "Steam/Zero Hour"); zh(steam); base(new File(steam, "ZH_Generals"));
        expect(GameDataValidator.check(steam, null).ready(), "Steam nested base");
        archive(steam, "PatchZH.big", "Data\\*");
        try (RandomAccessFile placeholder = new RandomAccessFile(new File(steam, "PatchZH.big"), "rw")) {
            placeholder.seek(16); placeholder.writeInt(0); placeholder.writeInt(0);
        }
        expect(GameDataValidator.check(steam, null).ready(), "Retail patch empty placeholder");
        expect(GameDataValidator.check(steam.getParentFile(), null).ready(), "Steam parent");
        File retail = new File(temp, "Retail/Zero Hour"); zh(retail);
        File retailBase = new File(temp, "Retail/Generals"); base(retailBase);
        expect(GameDataValidator.check(retail, null).ready(), "CD/ISO installed sibling base");
        expect(GameDataValidator.check(retail, retailBase).ready(), "Explicit split base");
        expect(!GameDataValidator.check(retailBase, null).ready(), "Base-only rejected");
        File merged = new File(temp, "Merged"); zh(merged); base(merged);
        expect(GameDataValidator.check(merged, null).ready(), "Merged install");
        File upper = new File(merged, "Terrain.big");
        expect(upper.renameTo(new File(merged, "TERRAIN.BIG")), "Rename fixture");
        expect(GameDataValidator.check(merged, null).ready(), "Archive case variants");
        File other = new File(temp, "Steam/Another"); zh(other);
        expect(GameDataValidator.check(steam.getParentFile(), null).choices.size() == 2, "Ambiguous installs");
        File disc = new File(temp, "Disc"); disc.mkdirs(); new File(disc, "data1.cab").createNewFile();
        expect(GameDataValidator.check(disc, null).installer, "Uninstalled disc explanation");
        File missing = new File(merged, "MapsZH.big"); expect(missing.delete(), "Remove fixture");
        expect(!GameDataValidator.check(merged, null).ready(), "Missing ZH archive");
        archive(merged, "MapsZH.big", "sample");
        try (RandomAccessFile bad = new RandomAccessFile(missing, "rw")) { bad.setLength(bad.length() - 1); }
        expect(GameDataValidator.check(merged, null).damaged.size() == 1, "Truncated payload with intact table");
        archive(merged, "MapsZH.big", "sample");
        try (RandomAccessFile bad = new RandomAccessFile(missing, "rw")) { bad.seek(8); bad.writeInt(999999); }
        expect(!GameDataValidator.check(merged, null).ready(), "Malformed table count");
        expect(!GameDataValidator.check(steam, new File(temp, "MissingBase")).ready(), "Explicit missing base never silently replaced");
        expect(GameDataValidator.check(steam, null).ready(), "Failed candidates never modify working files");
        File noText = new File(temp, "NoText"); zh(noText); base(new File(noText, "ZH_Generals"));
        archive(new File(noText, "ZH_Generals"), "English.big", "Data/English/generals.csf");
        expect(new File(noText, "EnglishZH.big").delete(), "Remove expansion text fixture");
        expect(!GameDataValidator.check(noText, null).ready(), "Base strings cannot replace expansion text");
        archive(noText, "EnglishZH.big", "Data/English/generals.csf");
        expect(new File(noText, "AudioZH.big").delete(), "Remove expansion audio fixture");
        expect(GameDataValidator.check(noText, null).missing.contains("AudioZH.big"), "Missing expansion sound");
        System.out.println(checks + " game-data checks passed");
    }
}
