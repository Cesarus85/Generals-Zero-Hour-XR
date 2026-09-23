# XR release preparation and checkpoints

**Updated:** 2026-09-23 — 1.2.33 command-console and native-keyboard release

**Publication state:** The non-debuggable 1.2.33 APK is the prepared public offline preview. Older releases, including debug-signed APKs, are retained as maintainer-only drafts. This leaves one public download without destroying historical artifacts. The maintainer chose to retain the product name after being informed that EA's source license grants no trademark rights; the project makes no EA affiliation claim.

## Current public release checkpoint: 1.2.33

PR #41 integrates PR #40's accepted military-console visual redesign and
transparent board-side shortcut plates, then replaces rejected custom/Android
IME experiments with Meta's runtime-owned OpenXR virtual keyboard. The native
keyboard covers every focused original entry gadget without changing original
field filtering or owner notifications. It does not enable or claim human
multiplayer compatibility.

The release-signed APK is 57,161,910 bytes, versionCode `10233`, versionName
`1.2.33-xr-preview`, package `com.generalsx.zerohour.xr`, ARM64 only and
nondebuggable. SHA-256 is
`0f940168d8bf376e0ac8211f9e089c6c86eb0b40b53db0cec9c1647e45deeed1`.
APK Signature Scheme v3 verifies with certificate SHA-256
`a3774568b341adc8abaa1e4200014020e6e2b80ca77c8a66e12bfa7a4498018f`.
Archive inspection found no retail `.big`, `.scb`, `.map` or `.w3d` files.

Local verification passes the complete ARM64 native build, release APK
assembly, 2,890 menu geometry/capture checks, 363 production routing checks
and 20,765 bilingual panel checks. The functionally identical versionCode
10233 test-label APK was installed in place on Quest 3. The maintainer accepted
the native keyboard's readable keys, controller ray, text confirmation and
safe return to gameplay, together with the PR #40 presentation. GitHub Actions
is skipped because the account quota is exhausted; local evidence is not
represented as hosted CI. Human multiplayer remains experimental because the
known Quest/PC simulation mismatch is separate from lobby text entry.

## Previous public release checkpoint: 1.2.28

PR #27 reduces XR board-mesh allocation churn; PR #33 exposes the existing
draw-source diagnostic over ADB; PR #34 batches up to ten unchanged Android
terrain patches per draw submission. The maintainer accepted the image and
reported the P26-2 build as much smoother. Maximum-coverage frame time improved
from 42.00 to 34.30 ms in the controlled Campaign measurement without reducing
terrain detail.

The release-signed APK is 57,100,470 bytes, versionCode `10228`, versionName
`1.2.28-xr-preview`, package `com.generalsx.zerohour.xr`, ARM64 only and
non-debuggable. SHA-256 is
`fc04349550027cab90964261e6eb221e4f4947c41ec57ff6def03529ca88ca20`.
APK Signature Scheme v3 verifies with certificate SHA-256
`a3774568b341adc8abaa1e4200014020e6e2b80ca77c8a66e12bfa7a4498018f`.
Archive inspection found no retail `.big`, `.scb`, `.map` or `.w3d` files.

The exact release bytes update-installed on Quest 3 `2G0YC5ZG9609PY`, retained
the original `2026-09-16 14:55:36` first-install time and report the expected
version. A pulled device `base.apk` matches the local release APK byte-for-byte.
Release tag `v1.2.28-xr-preview` points to merged source
`2b5150335c4f240253e61039b8bdfeac9322a9c8`. GitHub marks it as the public
non-prerelease Latest release. The uploaded APK digest, checksum sidecar and a
fresh independent GitHub download all match the local and device APK hash.
The former 1.2.25 public release is retained as a maintainer-only draft.

## Previous public release checkpoint: 1.2.25

PR #19 merged Campaign Ground View to `main` at `652f46f`; PR #21 merged
release-only native debug-section stripping at `5938257`. The release APK
was rebuilt from that merged source without version overrides. It is
57,100,470 bytes, versionCode `10225`, versionName `1.2.25-xr-preview`,
package `com.generalsx.zerohour.xr`, SHA-256
`dcd79e4d6be706f0ddd01d01416d321abd977a37aad768d750232bd1fc65e009`.
The packaged, debug-section-stripped `libmain.so` hashes to
`581a9772be3cce940d68ec3393e7b201ce246ceb0def8ae1709624964b796dfe`;
its NDK ELF build ID and `DT_NEEDED` dependencies match the unstripped native
build. Both APK and native library hashes match the staged release artifacts.
APK Signature Scheme v3 verifies with the existing release certificate
`a3774568b341adc8abaa1e4200014020e6e2b80ca77c8a66e12bfa7a4498018f`.
The manifest is non-debuggable, the ZIP is intact, only ARM64 native libraries
are packaged and no retail `.big`, `.scb`, `.map` or `.w3d` files are included.

Local tests pass: observer 64, loading 152, scene 1280, workspace 781,
bilingual panel 20,765, trigger 28 and endgame 33. Native ARM64,
`assembleXrRelease`, `assembleXrDebug` and `assembleZhDebug` pass. GitHub
Actions was not used because the account quota is exhausted. The Quest was
not ADB-connected during packaging. Worn-headset Campaign transitions,
long-session locomotion, comfort and mission performance remain open. The
public preview notes disclose these limits. Public access does not change the
open worn-headset Campaign Ground View or multiplayer validation gates.

Release tag `v1.2.25-xr-preview` points to `6b50f747e08f5e5d0a641e5f3ef9c90752f3b3f1`.
GitHub reports this private, non-prerelease release as `latest`, with the APK
asset size and SHA-256 matching the local final file. An independent fresh
download passes the uploaded `.sha256` sidecar and matches the local APK
byte-for-byte. The earlier 125-MB candidate was never published.

After publication, the exact 1.2.25 APK update-installed successfully over
10224 on Quest 3 `2G0YC5ZG9609PY`. Android reports versionCode 10225 and the
original `firstInstallTime` of 2026-09-16 14:55:36. The device-side
`base.apk` SHA-256 exactly matches the GitHub release asset. No worn-headset
Campaign Ground View play test was performed by this installation check.

## Historical private release checkpoints

The sections below record the state at the time of each older private release.
Their APK releases are now maintainer-only drafts; older references to private
visibility, `latest` or publication gates are historical, not current advice.

### Previous private release checkpoint: 1.2.24

PR #15 merged the optional offline-Skirmish Ground View and accepted board-side
shortcut layout to `main` at `9792d947`. The 1.2.23 test APK was installed over
the previous app on Quest 3, and the maintainer accepted the final shortcut
appearance. The release-preparation branch raises the committed Android
defaults to versionCode `10224` / `1.2.24-xr-preview`; package ID, ARM64 ABI,
private certificate and disabled debug cheats are retained. The final APK was
rebuilt from merged `main` commit `d72a7969`, with no version override. It is
125,028,534 bytes and hashes to
`bb685046bcfcdcd5222b648ac3ebe3a008dbe19bfc5a863b18b4ba749c17195e`.
The APK's `libmain.so` SHA-256 is
`a22343a882592fcbec2d2c9e37780326c9e78d46b89ff63b51d92a4b383450eb`,
matching the native ARM64 build. Android v3 verifies with the established
certificate `a3774568b341adc8abaa1e4200014020e6e2b80ca77c8a66e12bfa7a4498018f`;
the package is non-debuggable, ARM64-only and contains no retail `.big`,
`.scb`, `.map` or `.w3d` files. The earlier pre-merge candidate had a
different APK hash and is not the release asset.

Local checks on the merged P25 source pass: observer 56, interaction 148,
menu/ray 363, loading presenter 152, panel text 20,765, workspace 781,
scene 1,280 and endgame 33. A world-copy/MRT test ran on the Quest 3 Adreno
740 and passed 3,321 checks. The final 10224 signed APK installed in place on
Quest 3 `2G0YC5ZG9609PY`; Android reports the earlier `firstInstallTime`, and
its device-side `base.apk` SHA-256 exactly matches the final file. GitHub
Actions was skipped because the account quota is exhausted; local evidence
is not represented as hosted CI. Ground
View movement, collision, comfort and longer recovery sequences are still
experimental, not accepted by the button-layout report. The private offline
preview can disclose that limit without expanding campaign or multiplayer
support.

The release [v1.2.24-xr-preview](https://github.com/Cesarus85/Generals-Zero-Hour-XR/releases/tag/v1.2.24-xr-preview)
is private, non-prerelease and GitHub's current `latest`. Tag
`v1.2.24-xr-preview` resolves to `0403c5e485d18c42021b4a81e5d332bde1a49ea2`,
which adds verified-hash documentation only after the APK source merge
`d72a7969`. GitHub reports the uploaded APK asset at 125,028,534 bytes with
the exact SHA-256 above. A fresh independent download matches the local APK
byte-for-byte and passes the downloaded `.sha256` sidecar. The README and
installation guide point to this exact release tag and asset. The accepted
visual layout does not close the Ground View movement/comfort gate.

### Candidate and provenance

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

Before the 1.2.15 release, GitHub's “Latest” pointer resolved to older `v1.2.8-xr-preview` because P23 was a prerelease. README and Quest installation instructions therefore used an **explicit P23 tag/asset URL**, not `releases/latest`. The P23 versionName is misleading; this historical asset must not be substituted for the release-signed 10215 build. The LAN 10214 diagnostic APK was never a supported release candidate.

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

### Release-signed candidate

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
| Worn-headset acceptance | The maintainer confirmed that this installed final build starts Skirmish and Campaign and retains existing settings and game data. This is not a clean-profile first-import or all-missions test. |

### Previous private release checkpoint: 1.2.15

| Item | Verified value |
|---|---|
| Source | `main` merge `683997a89198ff418f0c7c2191836a2f62c25add` (PR #8); tag `v1.2.15-xr-preview` resolves to that exact commit |
| Release | [Generals: Zero Hour XR — Preview 1.2.15](https://github.com/Cesarus85/Generals-Zero-Hour-XR/releases/tag/v1.2.15-xr-preview), private repository, formerly “Latest” |
| APK asset | `Generals-Zero-Hour-XR.apk`, 124,995,766 bytes; GitHub asset digest `sha256:bafff443d77e7b9e925a73fcf16b5bf7234c54f256e2cb7fa0f95d1aad008d2b` |
| Independent download | Fresh GitHub release download hashes to the same value; its separately downloaded `.sha256` file verifies with `shasum -a 256 -c` |
| Historical releases | All four older debug-signed releases retain their assets and original notes, with a prominent superseded-build warning; all are marked prerelease |

The release asset is the **same bytes** as the locally built and Quest-installed
candidate. The original app data is not in the APK or this repository. The
repository has not been made public, and no LAN diagnostic APK was uploaded.

### Previous private release checkpoint: 1.2.17

| Item | Verified value |
|---|---|
| Source | PR #11 endgame fix and PR #12 version bump merged to `main`; tag `v1.2.17-xr-preview` resolves to `af439c387f91905c5df0d8d6345c9647b8521563` |
| Release | [Generals: Zero Hour XR — Preview 1.2.17](https://github.com/Cesarus85/Generals-Zero-Hour-XR/releases/tag/v1.2.17-xr-preview), private repository, formerly “Latest” |
| Package/version | `com.generalsx.zerohour.xr`, versionName `1.2.17-xr-preview`, versionCode `10217` |
| APK asset | `Generals-Zero-Hour-XR.apk`, 124,999,862 bytes; GitHub asset SHA-256 `8913648e9c8c124367ac812a4a3e9db0d0f296ec3a2c7e83a7d22627c2182869` |
| Independent download | Fresh GitHub download verifies against the separately downloaded `.sha256` asset and matches the local candidate |
| Signing and contents | Android v3 signature valid; signer certificate SHA-256 `a3774568b341adc8abaa1e4200014020e6e2b80ca77c8a66e12bfa7a4498018f`; ARM64; no manifest `debuggable` attribute; no retail `.big`, `.scb`, `.map` or `.w3d` |
| Native provenance | Complete local ARM64 build with `RTS_DEBUG_CHEATS=OFF`; packaged `libmain.so` SHA-256 `71195868054412b20af4e9ebad062839c4bb5dff10b6320a111eaeeb43e6beeb` matches the native build |
| Tests | Endgame 33, bilingual panel text 20,469, console bridge 1,467 and workspace 781 host checks pass; hosted Android CI is blocked by the account billing/spending limit |
| Quest installation | `adb install -r` succeeded over 10216 without uninstalling; versionCode 10217 and unchanged `firstInstallTime` confirmed; device-side `base.apk` SHA-256 matches the release asset |
| Headset scope | The maintainer accepted the short result-card test in separate 10216 APK; exact 10217 release bytes have not yet had a separate worn-headset play test |

The 10216 test APK contains debug-only retail end-action controller chords and
was **not** uploaded. The 1.2.17 APK is built without those chords. The release
does not include experimental LAN diagnostics. The GitHub repository remains
private. The original GPLv3 license and EA's additional conditions remain in
`LICENSE.md`; a separate public-distribution/trademark review is still open.

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

### Release-facing content

- For the previous 1.2.24 checkpoint, README centered Quest tabletop features,
  requirements, controls, honest limits and that release's exact download.
- Four maintainer-provided Quest captures illustrate separate maps, the build window and Commands/group UI. Original JPEG pixels and color profiles were retained; EXIF/TIFF/GPS metadata was removed from the repository copies. The maintainer reports that they visually reviewed the passthrough room content and approved it for publication.
- [Quest installation and controls](../../HOWTO/INSTALLATION_XR.md) and [game-file sourcing](../../HOWTO/GETTING_THE_GAME_FILES.md) are the user guides. No original game files are distributed.
- [Multiplayer status](../planning/MULTIPLAYER_STATUS.md) explicitly pauses LAN and records the paired CRC evidence and resumption plan. Experimental LAN code remains on its own branch, outside this candidate.

### Pre-publication gates and validation scope (historical)

1. The repository and 1.2.24 release are **private**. The maintainer reports that the four passthrough screenshots have been visually approved. Older debug-signed assets remain available but are prominently marked as historical development builds; making the repository public would expose them too. Before public distribution under the retained name, review the EA license's trademark restriction; a private release is not a decision to make the repository public.
2. The maintainer reports that the release key **and** Keychain password have been backed up; the backup was not independently verified and no secret belongs in Git.
   The 10215 update was installed over 10214 without uninstalling and a
   private pre-update app-data backup was captured. The first release-signed
   repack was playable according to the user; its session ended when the exact
   source-built replacement was installed while the app was running. That
   interruption was caused by our ADB update, not evidence of a game crash.
   Do not install again during an active play session without asking first.
3. The clean native source rebuild and exact-device 1.2.24 APK hash pass.
   The maintainer accepted the 1.2.23 shortcut layout, but the exact 1.2.24
   release bytes have not had a new worn-headset play session. Ground View
   locomotion/collision and long-session comfort are experimental. Earlier
   P23 play covered commands, groups and waypoints; this hash was not retested
   across that full matrix. Separately
   test first-time import with legitimate Steam files on a clean profile/device
   when available; do not erase this user's saved data merely to perform that
   test. Keep all retail data outside Git.
4. README/guide links, source tag, GitHub APK asset digest, independent download and 10223→10224 Quest update are verified for the 1.2.24 private release. Do not conflate the accepted 10223 shortcut-layout test with an exhaustive 1.2.24 headset test. Review screenshot presentation and release notes again before public visibility.
5. Keep the experimental LAN feature out of release claims. A fresh first-time data import and exhaustive mission/commands tests remain later validation work, not claims of this first preview.

The 10224 APK is the supported **private** offline preview. Older releases and
test builds remain historical; do not substitute a debug-signed or LAN
diagnostic asset for the current download.
