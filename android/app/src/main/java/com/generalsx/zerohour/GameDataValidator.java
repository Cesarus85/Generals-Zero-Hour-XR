// GeneralsX @feature Codex 14/09/2026 Shared, read-only Steam/retail data validation.
package com.generalsx.zerohour;

import java.io.*;
import java.util.*;

final class GameDataValidator {
    static final String[] BASE = {"INI.big", "Terrain.big", "Textures.big", "W3D.big",
        "Window.big", "Shaders.big", "Audio.big", "Speech.big", "Maps.big", "Music.big"};
    static final String[] ZH = {"INIZH.big", "TerrainZH.big", "TexturesZH.big", "W3DZH.big",
        "WindowZH.big", "MapsZH.big", "AudioZH.big", "SpeechZH.big", "MusicZH.big", "ShadersZH.big"};

    static final class Result {
        File game, base;
        final List<String> missing = new ArrayList<>();
        final List<String> damaged = new ArrayList<>();
        final List<File> choices = new ArrayList<>();
        boolean installer, unreadable, weather, language;
        boolean ready() { return game != null && base != null && missing.isEmpty() && damaged.isEmpty()
            && !unreadable && weather && language; }
    }

    static File named(File dir, String name) {
        if (dir == null) return null;
        File[] files = dir.listFiles();
        if (files != null) for (File f : files)
            if (f.getName().equalsIgnoreCase(name) && f.isFile()) return f;
        return null;
    }

    // A bounded local search, never a recursive scan of shared storage. Selecting
    // a Steam/common parent is supported; multiple installs need an explicit choice.
    static List<File> candidates(File root, String marker) {
        List<File> out = new ArrayList<>();
        discover(root, marker, 0, out, new HashSet<String>(), new int[]{0});
        out.sort(Comparator.comparing(File::getAbsolutePath));
        return out;
    }

    private static void discover(File dir, String marker, int depth, List<File> out,
            Set<String> seen, int[] visited) {
        if (dir == null || !dir.isDirectory() || visited[0]++ >= 128) return;
        try { if (!seen.add(dir.getCanonicalPath())) return; }
        catch (IOException e) { return; }
        if (named(dir, marker) != null) { out.add(dir); return; }
        if (depth >= 2) return;
        File[] children = dir.listFiles(File::isDirectory);
        if (children != null) {
            Arrays.sort(children, Comparator.comparing(File::getName));
            for (File child : children) if (!child.isHidden())
                discover(child, marker, depth + 1, out, seen, visited);
        }
    }

    static Result check(File selected, File explicitBase) {
        Result r = new Result();
        if (selected == null) return r;
        if (!selected.isDirectory() || selected.listFiles() == null) {
            r.unreadable = true;
            r.installer = selected.getName().toLowerCase(Locale.ROOT).endsWith(".iso");
            return r;
        }
        r.choices.addAll(candidates(selected, "INIZH.big"));
        if (r.choices.size() != 1) {
            File[] files = selected.listFiles();
            if (files != null) for (File f : files) {
                String n = f.getName().toLowerCase(Locale.ROOT);
                if (n.endsWith(".iso") || n.endsWith(".cab") || n.endsWith(".msi") || n.equals("setup.exe"))
                    r.installer = true;
            }
            return r;
        }
        r.game = r.choices.get(0);
        for (String n : ZH) if (named(r.game, n) == null) r.missing.add(n);

        if (explicitBase != null) {
            List<File> bases = candidates(explicitBase, "Terrain.big");
            if (bases.size() == 1) r.base = bases.get(0);
        } else if (named(r.game, "Terrain.big") != null) {
            r.base = r.game;
        } else {
            // Store the resolved base path explicitly: Java must not promise a
            // directory the engine's native mount search would never visit.
            List<File> bases = candidates(r.game, "Terrain.big");
            if (bases.size() == 1) r.base = bases.get(0);
            if (r.base == null && r.game.getParentFile() != null) {
                File[] siblings = r.game.getParentFile().listFiles(File::isDirectory);
                if (siblings != null) for (File sibling : siblings) {
                    if (named(sibling, "Terrain.big") != null) {
                        if (r.base != null) { r.base = null; break; }
                        r.base = sibling;
                    }
                }
            }
        }
        for (String n : BASE) if (named(r.base, n) == null) r.missing.add("Generals: " + n);
        Set<String> seen = new HashSet<>();
        inspect(r.game, r, seen);
        boolean expansionLanguage = r.language || looseLanguage(r.game);
        if (r.base != null && !r.base.equals(r.game)) inspect(r.base, r, seen);
        r.weather |= loose(r.game, "Data/INI/Default/Weather.ini") || loose(r.base, "Data/INI/Default/Weather.ini");
        // A base-only string table cannot supply Zero Hour's additional UI.
        r.language = expansionLanguage;
        return r;
    }

    private static boolean loose(File root, String path) {
        if (root == null) return false;
        File current = root;
        for (String component : path.split("/")) {
            File[] files = current.listFiles(); File next = null;
            if (files != null) for (File f : files)
                if (f.getName().equalsIgnoreCase(component)) { next = f; break; }
            if (next == null) return false;
            current = next;
        }
        return current.isFile() && current.length() > 0;
    }

    private static boolean looseLanguage(File root) {
        if (root == null) return false;
        File[] dirs = root.listFiles(File::isDirectory);
        if (dirs != null) for (File d : dirs) if (d.getName().equalsIgnoreCase("data")) {
            File[] langs = d.listFiles(File::isDirectory);
            if (langs != null) for (File lang : langs)
                if (loose(lang, "generals.csf") || loose(lang, "generals.str")) return true;
        }
        return false;
    }

    private static void inspect(File root, Result r, Set<String> seen) {
        File[] files = root.listFiles(f -> f.isFile() && f.getName().toLowerCase(Locale.ROOT).endsWith(".big"));
        if (files == null) { r.unreadable = true; return; }
        Arrays.sort(files, Comparator.comparing(File::getName));
        for (File file : files) {
            try {
                if (!seen.add(file.getCanonicalPath())) continue;
                inspectArchive(file, r);
            } catch (IOException e) { r.damaged.add(file.getAbsolutePath()); }
        }
    }

    // Check every table entry against actual file length, without reading gigabytes
    // of payload. This detects truncated transfers even when the BIGF header survived.
    private static void inspectArchive(File file, Result r) throws IOException {
        long length = file.length();
        try (DataInputStream in = new DataInputStream(new BufferedInputStream(new FileInputStream(file)))) {
            if (length < 16 || in.readInt() != 0x42494746) throw new IOException("BIGF header");
            in.readInt(); // Retail total-size field is not used by the native loader.
            long count = Integer.toUnsignedLong(in.readInt());
            long tableEnd = Integer.toUnsignedLong(in.readInt());
            if (count > 500000 || tableEnd < 16 || tableEnd > length) throw new IOException("BIGF table");
            long position = 16;
            for (long i = 0; i < count; i++) {
                long offset = Integer.toUnsignedLong(in.readInt());
                long size = Integer.toUnsignedLong(in.readInt()); position += 8;
                StringBuilder name = new StringBuilder();
                int b;
                while ((b = in.readUnsignedByte()) != 0) {
                    position++;
                    if (name.length() >= 511 || position >= tableEnd) throw new IOException("BIGF name");
                    name.append((char)b);
                }
                position++;
                // Retail PatchZH.big contains a zero-size Data\* placeholder at
                // offset 0. Empty entries have no payload to overlap the table.
                if (position > tableEnd || (size > 0 && offset < tableEnd)
                        || offset > length || size > length - offset)
                    throw new IOException("BIGF payload bounds");
                String n = name.toString().replace('\\', '/').toLowerCase(Locale.ROOT);
                if (size > 0 && n.equals("data/ini/default/weather.ini")) r.weather = true;
                if (size > 0 && n.matches("data/[^/]+/generals\\.(csf|str)")) r.language = true;
            }
        }
    }
}
