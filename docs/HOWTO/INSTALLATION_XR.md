# Generals: Zero Hour XR – Quest-3-Anleitung

Diese Anleitung gilt für den XR-Build **Generals: Zero Hour XR** auf Meta Quest
3. Das Repository enthält den portierten Quelltext und den Startcode, aber aus
Lizenzgründen keine Originalkarten, Texturen, Videos oder `.big`-Archive.
Benötigt wird eine eigene, rechtmäßig erworbene Installation von *Command &
Conquer: Generals – Zero Hour* inklusive der Basisdaten von *Generals*.

## Voraussetzungen

- Meta Quest 3 mit aktuellem Horizon OS und zwei Touch-Controllern
- aktivierter Entwicklermodus und ein USB-C-Datenkabel für ADB/SideQuest
- eigene Zero-Hour-Dateien mit mindestens `INIZH.big` sowie den Basisarchiven
  `Terrain.big`, `Textures.big` und `W3D.big`
- ungefähr 250 MB freier Speicher für das Preview-APK plus den eigenen
  Spieldaten

Die Originaldateien können von einem Windows-/Steam-PC auf die Quest kopiert
werden. Eine konkrete Dateibeschaffung ist nicht Teil dieses Projekts; siehe
[`GETTING_THE_GAME_FILES.md`](GETTING_THE_GAME_FILES.md).

## APK installieren

1. Lade die aktuelle Datei
   [`Generals-Zero-Hour-XR.apk`](../../releases/latest/download/Generals-Zero-Hour-XR.apk)
   von GitHub Releases.
2. Aktiviere in der Meta-Quest-App den Entwicklermodus, verbinde das Headset
   und bestätige den USB-Debugging-Dialog im Headset.
3. Prüfe am Rechner die Verbindung und installiere die APK:

   ```sh
   adb devices
   adb -s <quest-serial> install -r Generals-Zero-Hour-XR.apk
   ```

   `-r` ist wichtig: Die XR-Paket-ID bleibt bewusst
   `com.generalsx.zerohour.xr`, damit ein Update die bisherigen Einstellungen
   und Fensterpositionen behalten kann. Für ein Update die alte App nicht
   deinstallieren. SideQuest kann dieselbe APK ebenfalls installieren.
4. Starte **Generals: Zero Hour XR** aus der Quest-App-Bibliothek. Beim ersten
   Start öffnet sich automatisch der Setup-Fenster. Wähle dort den Ordner, in
   dem die Spieldaten liegen, und erteile die abgefragten Datei-/Raumrechte.

## Spieldaten auswählen

Der Setup-Fenster öffnet den Android-Dateibrowser. Wähle den Ordner, der direkt
`INIZH.big` und die Datenordner enthält; nicht den übergeordneten Download-
oder PC-Ordner. Die Prüfung meldet fehlende Basisarchive, bevor der native
Spielstart versucht wird. Bei einer fehlerhaften Auswahl erneut `Select Game
Folder` wählen. Die Raumfreigabe ist optional: Bei Ablehnung bleibt die freie
oder manuelle Brettplatzierung verfügbar.

## Spielbrett einrichten

Das Spiel startet im Tabletop-Modus. `UI → Spielplatz einrichten` öffnet den
Wizard auch im Hauptmenü und während eines geeigneten Skirmish-Spiels.

1. **Freies Brett** legt ein Brett mit dem Laser frei im Raum ab.
2. **Reale Fläche** lädt nach ausdrücklicher Bestätigung Raumdaten. Cyan
   markiert erkannte Flächen, Orange einen passenden Brett-Footprint und Rot
   eine zu kleine Fläche.
3. **Manuelle Höhe** merkt die Controllerhöhe mit Trigger vor, danach wird die
   gewünschte Position angezielt und erneut bestätigt.

Das ist eine optionale, sitzungsbezogene Platzierung und kein dauerhaftes
Möbel-Tracking. Das aktuelle Preview kann eine bestätigte letzte Anordnung
wiederherstellen. Für einen neuen Raum ist als nächster XR-Schritt ein sicherer
Fallback direkt vor dem Spieler vorgesehen; bis dahin `Spielplatz einrichten`
verwenden, wenn die alte Anordnung unpraktisch ist.

Unter `UI → Fenster` wird **Tisch** oder **Baufenster** als Bearbeitungsziel
gewählt. Ein orangefarbener Rahmen und ein Häkchen zeigen das Ziel. Ein Grip
bewegt/neigt das Ziel, beide Grips skalieren gleichmäßig. Ohne Grip verändert
der Zeigehand-Stick die Größe und der andere Stick den Abstand. Mit dem
Zeigehand-Stick zwischen den beiden Fenstern wechseln; `Reset` setzt das
ausgewählte Fenster wieder vor den Spieler. `Fertig`, Befehle oder Back
beenden die Bearbeitung.

## Controller-Bedienung

Die Standardbelegung ist rechtshändig. Unter `UI → Ansicht` kann jederzeit auf
linkshändig gewechselt werden; Zeigehand, Nebenhand und A/B/X/Y werden dann
logisch getauscht.

| Eingabe | Funktion |
|---|---|
| Rechter Trigger kurz | Einheit wählen oder Kontextbefehl am Laserziel |
| Rechter Trigger halten + Laser ziehen | Auswahlrahmen, beim Loslassen übernehmen |
| Linker Grip beim Drücken | Auswahl ergänzen bzw. einzelne Einheit abwählen |
| Linker Stick | Karte verschieben |
| Rechter Stick links/rechts | Karte/Kamera drehen |
| Rechter Stick hoch/runter | hinein-/herauszoomen |
| Rechter Grip | Zielmodus abbrechen, sonst Auswahl aufheben |
| A | Befehle-Fenster ein/aus |
| B oder System-Menü | Spielmenü öffnen/schließen |
| Linker Grip + rechter Stick beim Bau | Gebäudevorschau drehen, vor Trigger-Klick loslassen |

Einheiten werden zuerst markiert. Danach im Befehle-Fenster `Bewegen`,
`Angriffsmarsch` oder `Bewachen` wählen und auf das Ziel zeigen. `STOPP` und
`Auseinanderlaufen` wirken direkt. Gruppe speichern: Auswahl → `Speichern` →
Zahl. Die Zahl allein ruft die Gruppe auf. Gruppe erweitern: weitere Einheiten
→ `Hinzufügen` → Zahl → erneut `Speichern`. `Befehle → Hilfe` zeigt die
vierseitige Erklärung im Spiel.

## Sprache und Grafik

Beim ersten Start folgt die XR-Oberfläche der Quest-Systemsprache: Deutsch bei
deutschem System, sonst Englisch. Die Sprache der Spieldaten ist davon getrennt
und wird nur auf Deutsch gesetzt, wenn ein deutscher Sprachdatensatz vorhanden
ist. Die öffentliche Produktbezeichnung bleibt in jeder Sprache
**Generals: Zero Hour XR**.

Für Quest 3 sind standardmäßig **Balanced-Auflösung**, **leichte Schatten** und
**Multiview** aktiv; die Messanzeige bleibt für normales Spielen aus. Campaign
und Offline-AI-Skirmish sind die unterstützten XR-Spielpfade. Menschliches
LAN/Internet-Multiplayer, Replay-XR sowie Tastatur-/Mausunterstützung bleiben
separate Roadmap-Themen.

## Fehlerbehebung

- **Schwarzer/fehlender Start:** Setup erneut öffnen und den Ordner mit
  `INIZH.big` auswählen; danach die Basisarchive prüfen.
- **Brett außerhalb des Sichtfelds:** `UI → Spielplatz einrichten → Freies
  Brett` wählen oder `UI → Fenster → Reset` verwenden.
- **Keine Raumflächen:** Raumfreigabe in Horizon erlauben und `Load room data`
  erneut ausführen. Alternativ freies Brett oder manuelle Höhe verwenden.
- **Update setzt nichts zurück:** Prüfen, dass mit `install -r` aktualisiert und
  nicht deinstalliert wurde. Die Paket-ID ist unverändert.
- **Diagnose:** In Setup nur die angeforderte Diagnose aktivieren, Problem
  reproduzieren und unter `View Logs → Share` das Protokoll teilen.

## Aus Source bauen

```sh
git clone https://github.com/Cesarus85/Generals-Zero-Hour-XR.git
cd Generals-Zero-Hour-XR
git submodule update --init --recursive
./scripts/build/android/build-android-zh.sh
./scripts/build/android/package-android-zh.sh
```

Die XR-Datei liegt danach unter
`build/apk/Generals-Zero-Hour-XR.apk`. Voraussetzungen, reproduzierbare Builds
und Android-Architektur stehen in [`docs/port/ANDROID_PORT.md`](../port/ANDROID_PORT.md).
Die Buildskripte verteilen keine Spieldaten.
