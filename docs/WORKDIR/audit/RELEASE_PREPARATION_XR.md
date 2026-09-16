# First XR release preparation

**Updated:** 2026-09-16 — release-signing and Quest update test
**Publication state:** A non-debuggable, privately signed local candidate exists. No new GitHub Release or repository visibility change has been made.

## Candidate and provenance

| Item | Verified value |
|---|---|
| Candidate | Existing P23 offline prerelease `xr-preview-2026-09-16-p23` |
| Source tag | `ba9169d5c81604aa9fac00508cf2e5dcbf9a4939` |
| Current `main` before this branch | `b515e7517ab338b04d390dd9a80fcc27ef8c8286`; changes after the P23 source tag are documentation and screenshots only |
| APK asset | `Generals-Zero-Hour-XR.apk`, 138,815,473 bytes |
| APK SHA-256 | `978c627e276ab1627d014f68a8e1ad436ecaae215e8946ef900e5b999077273f` (GitHub asset digest and fresh download agree) |
| Package/version | `com.generalsx.zerohour.xr`, versionCode `10209`, inherited versionName `1.2.9-p20.1-thin-underbody` |
| ABI and signature | `arm64-v8a`; APK Signature Scheme v2 verifies, one signer; certificate subject is `CN=GeneralsX Android Debug` |
| App label | `Generals: Zero Hour XR` |
| Debuggability | Manifest has `android:debuggable=true` |
| Contents | Engine/runtime libraries are present; no retail `.big`, `.scb`, `.map` or `.w3d` game data was found in the APK |

The current GitHub “Latest” pointer still resolves to older `v1.2.8-xr-preview` because P23 is a prerelease. README and Quest installation instructions therefore use an **explicit P23 tag/asset URL**, not `releases/latest`. Do not silently move the pointer or substitute the unpublished LAN 10214 diagnostic APK. The P23 versionName is misleading; correcting it would create a new APK that needs installation and physical validation. For this first candidate, document the discrepancy instead of pretending the binary says P23 internally.

The P23 release notes report local native/Android build and focused host-test success, plus user-played Quest feedback for commands, waypoints and visual readability. The exact uploaded artifact's device-side hash and a sustained Ultra+ busy-campaign performance capture were **not** supplied. Campaign coverage and device compatibility are incremental rather than universal. The APK has been inspected here, not newly built or freshly worn-headset-tested by this documentation pass.

The original P23 prerelease remains **debuggable** and signed with the tracked
development key. Do not re-label or promote that APK as a security-hardened
public build. The new release path separates `assembleXrRelease` from the
debug signer, signs only with a private certificate and an Android v3
proof-of-rotation, and gives the old certificate installed-data and
signature-permission continuity but **not** rollback authority. Permission
continuity is required by the app's AndroidX dynamic-receiver permission;
without it, Quest installation fails with `INSTALL_FAILED_DUPLICATE_PERMISSION`.
The development CI workflow can no longer publish a debug artifact as a
GitHub Release.

## Local release-signed candidate (not published)

| Item | Verified value |
|---|---|
| Version/package | `1.2.15-xr-preview` (`10215`), `com.generalsx.zerohour.xr` |
| APK | `build/apk/Generals-Zero-Hour-XR-release.apk` in the release branch |
| SHA-256 | `bafff443d77e7b9e925a73fcf16b5bf7234c54f256e2cb7fa0f95d1aad008d2b` |
| Signing certificate SHA-256 | `a3774568b341adc8abaa1e4200014020e6e2b80ca77c8a66e12bfa7a4498018f` |
| Verification | Android v3 signature valid; release manifest has no `debuggable=true`; ARM64 only; no retail `.big`, `.scb`, `.map` or `.w3d` |
| Native provenance | Clean ARM64/DXVK source build from this `main`-based branch; packaged `libmain.so` SHA-256 `28ac8c653c24b361c2eadf4f802dde2a6d565abf76f9b6e7bc106773380df382` equals the newly built native library |
| Quest update | `adb install -r` succeeded over diagnostic `10214` on Quest 3/API 34 without uninstalling, then the exact source-built APK update also succeeded; `firstInstallTime` remained `2026-09-16 14:55:36` and `run-as` is rejected because the package is not debuggable |
| Device-side artifact | Installed `/data/app/.../base.apk` SHA-256 `bafff443d77e7b9e925a73fcf16b5bf7234c54f256e2cb7fa0f95d1aad008d2b`, matching the local file |

The signing key lives outside Git in the maintainer's private user directory;
its password is in macOS Keychain service `Generals Zero Hour XR Release
Signing`. **Back up the private keystore and its password separately before
public distribution.** Losing either prevents future updates under the same
identity. The tracked debug keystore is intentionally retained only for old
preview lineage and development builds.

To rebuild from native source, run the normal Android native build, then:

```sh
export GX_XR_RELEASE_KEYSTORE='/path/to/private/release-signing.p12'
export GX_XR_RELEASE_KEY_ALIAS='generals-zero-hour-xr'
export GX_XR_RELEASE_KEYSTORE_PASSWORD="$(security find-generic-password -a "$USER" -s 'Generals Zero Hour XR Release Signing' -w)"
GX_FLAVORS=xr ./scripts/build/android/package-android-zh.sh --release
```

`--release --install` additionally requires `GX_ADB_SERIAL`; never use a
blind first-device install for a release candidate. A Gradle-only
`assembleXrRelease` produces an **unsigned** APK, not a releasable asset.
Android 9+ supports v3 rotation; this candidate was actually update-tested on
Quest API 34. Other OS versions and already-modified third-party signing
lineages have not been device-tested. [Android's update requirements](https://developer.android.com/google/play/app-updates)
and [v3 rotation behavior](https://source.android.com/docs/security/features/apksigning/v3)
are the platform basis, not a promise of universal migration.

## Release-facing content

- README now centers Quest tabletop features, requirements, controls, honest limits and the exact P23 download.
- Four maintainer-provided Quest captures illustrate separate maps, the build window and Commands/group UI. Original JPEG pixels and color profiles were retained; EXIF/TIFF/GPS metadata was removed from the repository copies. These captures can show the user's physical room in passthrough: review the *visible image content* before making the repository public.
- [Quest installation and controls](../../HOWTO/INSTALLATION_XR.md) and [game-file sourcing](../../HOWTO/GETTING_THE_GAME_FILES.md) are the user guides. No original game files are distributed.
- [Multiplayer status](../planning/MULTIPLAYER_STATUS.md) explicitly pauses LAN and records the paired CRC evidence and resumption plan. Experimental LAN code remains on its own branch, outside this candidate.

## Publication gates still open

1. Decide whether this first release will be public; the repository is currently **private**. Review the four passthrough screenshots for visible personal surroundings and audit **all existing debug-signed release assets** before changing visibility. Making the repository public would also expose those older downloads; do not present them as supported public builds.
2. Preserve a secure backup of the release key **and** Keychain password.
   The 10215 update was installed over 10214 without uninstalling and a
   private pre-update app-data backup was captured. The first release-signed
   repack was playable according to the user; its session ended when the exact
   source-built replacement was installed while the app was running. That
   interruption was caused by our ADB update, not evidence of a game crash.
   Do not install again during an active play session without asking first.
3. The clean native source rebuild and exact-device APK hash now pass. Run a
   worn-headset test of **this final hash**: returning launch with retained
   game-data selection, one Skirmish, a campaign intro/mission and the
   Commands/group/waypoint controls. Separately test first-time import with
   legitimate Steam files on a clean profile/device; do not erase this user's
   saved data merely to perform that test. Keep all retail data outside Git.
4. Confirm README/guide links and screenshot rendering on the GitHub default branch after the documentation change is merged. Recheck the source tag, APK asset digest and release notes at publication time.
5. Choose a user-facing release tag/name and whether to retain the prerelease classification. Publish only the exact verified, privately signed APK and matching source checkpoint. Do not claim the experimental LAN feature in release notes.

The 10215 APK is local only. The P23 GitHub asset and README download link remain unchanged until the final artifact passes the remaining gates.
