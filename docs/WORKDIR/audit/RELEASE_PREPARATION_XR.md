# First XR release preparation

**Updated:** 2026-09-16 — repository and artifact audit
**Publication state:** Prepared for review; no new public release or repository visibility change was made by this documentation pass.

## Candidate and provenance

| Item | Verified value |
|---|---|
| Candidate | Existing P23 offline prerelease `xr-preview-2026-09-16-p23` |
| Source tag | `ba9169d5c81604aa9fac00508cf2e5dcbf9a4939` |
| Current `main` at audit | `a5cb2d0393d9778bc0fcbb013020b29d1e750325`; its only change after the P23 source tag is XR status documentation |
| APK asset | `Generals-Zero-Hour-XR.apk`, 138,815,473 bytes |
| APK SHA-256 | `978c627e276ab1627d014f68a8e1ad436ecaae215e8946ef900e5b999077273f` (GitHub asset digest and fresh download agree) |
| Package/version | `com.generalsx.zerohour.xr`, versionCode `10209`, inherited versionName `1.2.9-p20.1-thin-underbody` |
| ABI and signature | `arm64-v8a`; APK Signature Scheme v2 verifies, one signer; certificate subject is `CN=GeneralsX Android Debug` |
| App label | `Generals: Zero Hour XR` |
| Debuggability | Manifest has `android:debuggable=true` |
| Contents | Engine/runtime libraries are present; no retail `.big`, `.scb`, `.map` or `.w3d` game data was found in the APK |

The current GitHub “Latest” pointer still resolves to older `v1.2.8-xr-preview` because P23 is a prerelease. README and Quest installation instructions therefore use an **explicit P23 tag/asset URL**, not `releases/latest`. Do not silently move the pointer or substitute the unpublished LAN 10214 diagnostic APK. The P23 versionName is misleading; correcting it would create a new APK that needs installation and physical validation. For this first candidate, document the discrepancy instead of pretending the binary says P23 internally.

The P23 release notes report local native/Android build and focused host-test success, plus user-played Quest feedback for commands, waypoints and visual readability. The exact uploaded artifact's device-side hash and a sustained Ultra+ busy-campaign performance capture were **not** supplied. Campaign coverage and device compatibility are incremental rather than universal. The APK has been inspected here, not newly built or freshly worn-headset-tested by this documentation pass.

**Public-signing decision required:** `android/app/debug.keystore` is tracked in
Git, and both Gradle `debug` and `release` build types currently use that
fixed, known-password debug certificate. Reusing it for a public package would
let anyone with the repository sign an APK with the same update identity.
The existing P23 APK is also debuggable. Do not present the current binary as
a security-hardened public release. A private signing key outside the repo and
a non-debuggable build are the normal public path, but changing the signer can
affect in-place updates for people already using `com.generalsx.zerohour.xr`.
Choose and test a migration path before publication; do not delete the shared
debug key or change package/signature in this documentation-only pass.

## Release-facing content

- README now centers Quest tabletop features, requirements, controls, honest limits and the exact P23 download.
- Four maintainer-provided Quest captures illustrate separate maps, the build window and Commands/group UI. Original JPEG pixels and color profiles were retained; EXIF/TIFF/GPS metadata was removed from the repository copies. These captures can show the user's physical room in passthrough: review the *visible image content* before making the repository public.
- [Quest installation and controls](../../HOWTO/INSTALLATION_XR.md) and [game-file sourcing](../../HOWTO/GETTING_THE_GAME_FILES.md) are the user guides. No original game files are distributed.
- [Multiplayer status](../planning/MULTIPLAYER_STATUS.md) explicitly pauses LAN and records the paired CRC evidence and resumption plan. Experimental LAN code remains on its own branch, outside this candidate.

## Publication gates still open

1. Decide whether this first release will be public; the repository is currently **private**. Review the four passthrough screenshots for visible personal surroundings before changing visibility.
2. Decide the signing/update strategy above. The development Quest last had
   diagnostic versionCode 10214 installed, **higher** than P23's 10209; a
   normal `install -r` is not a valid rollback procedure. Back up relevant app
   data or use a separate device, and test any supported downgrade/migration
   path explicitly before changing that Quest.
3. On a Quest 3, install the **exact candidate APK** after resolving the
   version/signature path, confirm package/version and ideally read back its
   SHA-256. Run first-time game-data import with legitimate Steam files,
   returning launch, one Skirmish, a campaign intro/mission and the
   Commands/group/waypoint controls. Keep the original data outside Git.
4. Confirm README/guide links and screenshot rendering on the GitHub default branch after the documentation change is merged. Recheck the source tag, APK asset digest and release notes at publication time.
5. Choose a user-facing release tag/name and whether to retain the prerelease classification. If publishing a new APK instead of reusing P23, build, sign, install and retest that **new** artifact. Do not claim the currently experimental LAN feature in release notes.

No new APK, GitHub Release or public visibility change is implied by this checklist.
