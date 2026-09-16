# PLAN-025 — Quest ↔ Windows PC LAN preflight

**Status:** Opt-in Quest candidate reaches a Steam/Proton lobby by Direct Connect; map availability blocks the first match.
**Scope:** One Quest 3 against a Windows PC on the same LAN. Start with the user's installed Steam Zero Hour (record its displayed version); if incompatible, isolate whether a same-source GeneralsX Windows build solves it. Internet services, public matchmaking, replay and reconnect are later gates.

## Decision and evidence

- The Quest port contains the native LAN lobby, direct-IP join, game-start transport and synchronized command path. LAN uses UDP 8086 for lobby traffic and 8088 for gameplay in the inspected source.
- `RETAIL_COMPATIBLE_NETWORKING` and `RETAIL_COMPATIBLE_CRC` default to 1. This makes retail interoperability a worthwhile **test**, not a verified feature. Source changes, platform timing, installed game data, maps or mods may still diverge.
- Zero Hour's LAN join CRC rejection is currently compiled out (`#if !RTS_ZEROHOUR`). Therefore lobby success alone is weak evidence: only a sustained match without CRC mismatch or desynchronization can pass the compatibility gate. Do not remove or alter that legacy check speculatively.
- The accepted Quest release deliberately allows tabletop stereo only for offline Skirmish and campaign. The new `GX_XR_LAN_PREVIEW` Android CMake option defaults OFF and admits `GAME_LAN` only when explicitly enabled. It is a **presentation-only** experiment; it changes no game messages, rules or wire format. Internet and replay stay excluded.

## First physical test: Steam Zero Hour on Windows

1. Record the exact Quest APK/source commit and Steam Zero Hour version. Use matching unmodified Zero Hour game data and a stock multiplayer map; disable gameplay-changing mods on both ends. Do not copy retail assets into the source repository.
2. Connect Quest and PC to the same ordinary IPv4 LAN. Disable guest Wi-Fi/client isolation. Permit the game through the Windows firewall, including UDP 8086 and 8088. Record each endpoint's IPv4 address. LAN broadcast discovery is tested first; direct IP is the fallback, not proof that discovery works.
3. Open the native LAN lobby in the Quest's upright shell. Verify controller pointing, text entry where needed, host/join, lobby selection and chat. Test PC-host/Quest-join first, then reverse the roles if possible.
4. Start a two-human match. Verify Quest tabletop placement, build window and commands; select, build, move, attack and use one control group on both sides. Play at least 15 minutes with visible interaction, compare synchronized events, watch for CRC mismatch/stall and measure Quest frame pacing. Then leave and rejoin safely.
5. Repeat one deliberately mismatched-map/content case only after the clean match; capture its observable result without modifying the networking rules to force a join.

If Steam discovery/join or in-match synchronization fails, reproduce with a Windows GeneralsX executable from the **same source commit and game data**. This separates retail compatibility from Quest networking/XR defects. Do not describe the Steam version as supported until the clean Steam match passes. A same-source pass is not evidence that retail Steam works.

## Acceptance gates

| Gate | Evidence needed | State |
|---|---|---|
| Build safety | Default-off and preview-on mode tests; Android native/XR APK build | 10210 lobby candidate tested; 10212 keyboard/map candidate built and installed, headset test pending |
| Lobby | Discovery and direct-IP outcomes, both endpoint IPs, host/join/leave, chat/input | Direct Connect to Steam/Proton reaches the game lobby; automatic discovery still fails |
| Simulation | 15-minute Quest ↔ PC human match, orders from both players, no desync/CRC/stall | Open |
| XR usability | Tabletop and upright shell transitions, controller menu/text entry, headset pause/resume and performance | Open; controller-operated Direct Connect keyboard built in 10212, worn-headset test pending |
| Retail compatibility | Above gates against the user's unmodified Steam Zero Hour | Open |
| Regression | Offline Skirmish/campaign retain accepted tabletop behavior | Host eligibility test passes; device regression open |

The local, **not published**, opt-in APK is
`build/apk/Generals-Zero-Hour-XR.apk`, package
`com.generalsx.zerohour.xr`, version 10210 (`1.2.10-lan-preflight`), SHA-256
`5cc4f84a04eb7b44bab906f8acff3414b1e5d9b808fafc04b557d8fcfb374a4b`.
The native ARM64 target and XR APK build passed; v2 signing and staged native
library equality were verified. Its source is the unmerged
`codex/quest-pc-lan-preflight` branch. It was installed in place on Quest 3
`2G0YC5ZG9609PY` using `adb install -r`, without clearing app data. The APK
read back from the device has the same SHA-256. The user reached the LAN lobby;
tabletop human gameplay and broader headset UX remain untested. Preserve the
prior release APK for rollback.

First user feedback after installation: both machines run explicitly labelled
Zero Hour and share the same Wi-Fi, but neither LAN lobby discovers the other.
Their animated shell backgrounds differ; that does not identify the network
failure or prove an SKU mismatch. Quest has only one active non-loopback IPv4
interface (`wlan0`), and no fixed `IPAddress` entry was found in its options.
The diagnostic next test was native Direct Connect to the peer's IPv4 address.
Avoid changing the simulation or relaxing compatibility checks on discovery
failure alone.
The user subsequently tried an Omarchy laptop at `192.168.178.158` as an
alternate peer, running Steam Zero Hour through Proton (the Windows game,
not a native GeneralsX build). The Quest routes to it via `wlan0` and
received all three ICMP replies (0% loss). This rules out total IP/subnet
isolation for that peer, not a UDP firewall, LAN socket or game protocol
failure. Direct Connect subsequently reached the shared game lobby, proving
that at least the initial Quest ↔ Steam/Proton LAN exchange works. The Omarchy
side then reported that it did not have the selected map. The latest Quest
`Network.ini` names `Maps\\Alpine Assault\\Alpine Assault.map`; the Quest
hosted, and the user believes this was the failed lobby map. SHA-256 of the
Quest and Omarchy Steam/Proton `MapsZH.big` and base Generals `maps.big`
archives matches on both systems. This rules out different bytes in those
archives, not a local map override, map-cache discrepancy or LAN map-path/CRC
mismatch. `hasMap`
is false both when the map is absent from `MapCache` and when its file CRC
differs, so do not treat the warning as proof that the file is missing. Check
the exact lobby map, effective map metadata/CRC and serialized map path before
changing transfer rules or CRC checks. The Quest also needs an XR controller
text-entry path for Direct Connect IP fields; for now the Quest-host direction
avoids typing on the headset.

The source inspection identified a concrete retail-compatibility candidate:
`GameInfoToAsciiString` percent-encoded spaces in every map directory, including
LAN announcements and game options. A stock `Alpine Assault` path therefore
sent `Alpine%20Assault` to the Steam peer. LAN call sites now serialize the
literal legacy directory while replay/save callers retain percent encoding;
the parser still accepts both. This explanation is strongly consistent with
the warning but remains unconfirmed until a Steam/Proton lobby retest.

The same candidate adds a controller-operated XR keyboard for the Direct
Connect player-name and remote-IPv4 fields. The keyboard is a native spatial
panel, not an Android 2D IME; numeric/IP and alphabetic layouts share their
paint/hit geometry. Host regressions pass: 3848 menu geometry/capture, 329
menu action/routing, and 22429 bilingual panel-payload checks (including both
keyboard layouts).
The Android ARM64 native build and XR APK package pass. Test version 10212
(`1.2.12-lan-keyboard`), SHA-256
`e8f05cc2c77a3d125117ed7a836e36fdcd159f59d4807d37a89006954fd166fc`,
is at `build/apk/Generals-Zero-Hour-XR.apk` and was installed on Quest 3
`2G0YC5ZG9609PY` with `adb install -r` (data retained). APK v2 signature,
package/version and bundled native-library hash were verified. This is a
test candidate only; keyboard visibility/typing and the same-map LAN join
still require worn-headset testing before merge or publication.

Do not enable LAN tabletop by default, merge a network-eligibility expansion into a release, or claim multiplayer support while these physical gates remain open. Keep replay and internet as separate later work. Keyboard/mouse remains secondary to the controller path.
