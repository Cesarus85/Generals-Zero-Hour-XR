# Multiplayer status — paused after the first paired Quest/PC trace

**Updated:** 2026-09-23

**State:** Experimental, paused for the first offline release preparation
**Do not ship:** Quest LAN diagnostic APKs or LAN-enabled gameplay as a supported feature

This document is the public-facing handoff summary. The detailed chronological audit, source and test commands live on the separate [`codex/quest-pc-lan-preflight`](https://github.com/Cesarus85/Generals-Zero-Hour-XR/tree/codex/quest-pc-lan-preflight) branch in `docs/WORKDIR/planning/PLAN-025_QUEST_PC_LAN_PREFLIGHT.md`. That branch is **not** part of the offline release candidate. The [current XR status](XR_CURRENT_STATUS.md) and Git history remain the other sources of truth.

## Goal and current boundary

The goal is human Quest ↔ PC play with the unmodified Steam *Zero Hour* game, plus Quest ↔ Quest LAN later. Internet multiplayer, reconnect and replay XR are separate future gates. Offline AI Skirmish and campaign do not validate human lockstep synchronization.

The accepted offline P23 prerelease is [`xr-preview-2026-09-16-p23`](https://github.com/Cesarus85/Generals-Zero-Hour-XR/releases/tag/xr-preview-2026-09-16-p23), package `com.generalsx.zerohour.xr`, versionCode `10209`, APK SHA-256 `978c627e276ab1627d014f68a8e1ad436ecaae215e8946ef900e5b999077273f`. It must stay distinct from the unpublished LAN diagnostic APK 10214. The same package ID allows update installs, but a higher versionCode would replace the offline candidate on a device; do not infer release readiness from what is currently installed on the development Quest.

## What has been demonstrated

- Quest can reach the LAN lobby, use Direct Connect, join/host a match and load the map. The focused controller-operated player-name/IP keyboard is now being integrated independently onto current offline `main`; it does not enable or claim multiplayer compatibility and contains no LAN diagnostic or simulation changes.
- The Quest battlefield enters the tabletop view in a human LAN match with the preview-only `GX_XR_LAN_PREVIEW` build option. This option defaults off and does not change simulation or wire format.
- Quest and an Omarchy laptop running Steam Zero Hour through Proton can start a Direct-Connect match. Automatic LAN discovery remains unreliable. Omarchy's `100.123.209.83` address was Tailscale, while its WLAN is `192.168.178.158`; the isolated diagnostic profile pins both LAN and Online IP to WLAN. This addressed interface selection but did **not** cure the later mismatch.
- The common Zero Hour and base Generals gameplay archives tested on both devices have matching SHA-256 hashes. This reduces, but does not eliminate, effective-data/mod/override differences.
- A native Linux GeneralsX diagnostic built from the same simulation source also starts a Quest-hosted match. Its build-compatibility changes do not intentionally change simulation logic. The original Steam/Proton installation and normal user data were not modified.

## Reproducible failure and evidence

Steam/Proton reported the game's synchronization mismatch dialog shortly after map start. The paired Quest/native-Linux diagnostic test then reproduced a **real different-CRC failure on both peers**, so the problem is not limited to the retail executable or to missing CRC packets.

The paired test used map CRC `DEA9E8E4`, game seed `4042777` and CRC interval 100. Both peers produced the same final CRC and all recorded rolling checkpoints at generation frame 0: `3263A8D7`. At generation frame 100, the first observed unequal checkpoint is the object list: Quest `6AE75FBB`, native PC `03972538`. The logic RNG seed checksum still agrees (`A82FF014`). At validation frame 105 both peers received Quest CRC `E2E3DF5F` and PC CRC `A6E913D4` and classified `different_crc`.

The existing paired comparator, `scripts/qa/lan-crc-compare.py` on the LAN branch, confirms the frame-100 `objects` result. The later RNG/partition/player/AI checkpoint values are *rolling* CRCs, so their differences are downstream of the object checkpoint; they do not independently prove those subsystems differ. The exact object, first divergent simulation tick and root cause are **not yet known**. Cross-CPU floating-point behavior, object ordering/creation and inherited simulation changes are hypotheses, not conclusions. Raw device logs are kept private and are not part of this repository.

## Concrete resumption plan

1. On the separate LAN branch, add opt-in, bounded per-object observations to the **existing** `GameLogic::getCRC` traversal: stable object ID, traversal order and rolling CRC after each object, plus count/truncation. Avoid a second scan and do not change the production CRC contents/cadence, network packets, commands or match rules. Test disabled behavior and trace bounds.
2. Incrementally rebuild the Quest diagnostic APK and the native Omarchy diagnostic from matching source. Verify hashes, package/version, and that the ordinary offline build still excludes the preview path.
3. Repeat one controlled Quest-hosted Direct-Connect match on a fixed stock map, fixed factions/slots, no AI and no early player orders. Compare the first unequal object or missing/differently ordered ID. If necessary, add a *narrow* field-level or earlier-tick probe for that object rather than broad logging.
4. Fix only a demonstrated cause. Pass an idle match, an interactive match with orders from both players, and a 15-minute headset test against the same-source native PC build. Then repeat against **unmodified Steam/Proton** and **Windows Steam**; only those tests can establish the requested retail-PC compatibility. Quest ↔ Quest is an additional separate gate.
5. Keep automatic discovery, leave/rejoin, keyboard/chat UX, headset performance and regressions of offline campaign/Skirmish on the acceptance list. Do not suppress the CRC detector, assume a lobby/map load proves compatibility, or merge the LAN preview into the first offline release.

This pause is intentional; no multiplayer support is claimed by the P23 offline prerelease.
