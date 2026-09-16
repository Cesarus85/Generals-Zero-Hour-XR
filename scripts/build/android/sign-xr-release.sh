#!/usr/bin/env bash
# Sign an unsigned XR release APK with a private key outside this repository.
# The committed debug key is used only to attest a one-way update lineage.
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <unsigned-xr-apk> <signed-xr-apk>" >&2
    exit 2
fi

unsigned_apk="$1"
signed_apk="$2"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd "${script_dir}/../../.." && pwd)"
debug_keystore="${project_root}/android/app/debug.keystore"
release_keystore="${GX_XR_RELEASE_KEYSTORE:-}"
release_alias="${GX_XR_RELEASE_KEY_ALIAS:-}"
release_password="${GX_XR_RELEASE_KEYSTORE_PASSWORD:-}"
expected_release_cert='a3774568b341adc8abaa1e4200014020e6e2b80ca77c8a66e12bfa7a4498018f'

if [[ ! -f "${unsigned_apk}" ]]; then
    echo "ERROR: unsigned XR APK not found: ${unsigned_apk}" >&2
    exit 1
fi
if [[ -z "${release_keystore}" || ! -f "${release_keystore}" || -z "${release_alias}" || -z "${release_password}" ]]; then
    echo "ERROR: set GX_XR_RELEASE_KEYSTORE, GX_XR_RELEASE_KEY_ALIAS, and GX_XR_RELEASE_KEYSTORE_PASSWORD; the private keystore must exist outside the repository." >&2
    exit 1
fi
release_keystore="$(cd "$(dirname "${release_keystore}")" && pwd)/$(basename "${release_keystore}")"
case "${release_keystore}" in
    "${project_root}"/*)
        echo "ERROR: release signing key must be stored outside the repository." >&2
        exit 1 ;;
esac
if [[ "${unsigned_apk}" == "${signed_apk}" ]]; then
    echo "ERROR: input and output APK paths must differ." >&2
    exit 1
fi

android_sdk="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-${HOME}/Library/Android/sdk}}"
apksigner="${GX_APKSIGNER:-$(find "${android_sdk}/build-tools" -maxdepth 2 -name apksigner -type f 2>/dev/null | sort | tail -1)}"
aapt="${GX_AAPT:-$(find "${android_sdk}/build-tools" -maxdepth 2 -name aapt -type f 2>/dev/null | sort | tail -1)}"
if [[ ! -x "${apksigner}" || ! -x "${aapt}" ]]; then
    echo "ERROR: Android SDK apksigner and aapt are required." >&2
    exit 1
fi

# GeneralsX @build Codex 16/09/2026 Fail closed on a debug manifest or the
# wrong package before any release key operation.
manifest="$(${aapt} dump xmltree "${unsigned_apk}" AndroidManifest.xml)"
if [[ "${manifest}" != *'package="com.generalsx.zerohour.xr"'* ]] ||
   [[ "${manifest}" == *'android:debuggable('* ]]; then
    echo "ERROR: expected non-debuggable com.generalsx.zerohour.xr APK." >&2
    exit 1
fi
package_line="$(${aapt} dump badging "${unsigned_apk}" | grep '^package:' | head -1)"
if [[ "${package_line}" =~ versionCode=\'([0-9]+)\' ]]; then
    version_code="${BASH_REMATCH[1]}"
else
    echo "ERROR: cannot read XR versionCode." >&2
    exit 1
fi
if (( version_code < 10215 )); then
    echo "ERROR: release versionCode must exceed installed diagnostic 10214." >&2
    exit 1
fi
if unzip -Z1 "${unsigned_apk}" | grep -Eiq '\.(big|scb|map|w3d)$'; then
    echo "ERROR: APK contains retail game-data file types." >&2
    exit 1
fi
apk_entries="$(unzip -Z1 "${unsigned_apk}")"
if grep -Eq '^lib/(armeabi-v7a|x86|x86_64)/' <<< "${apk_entries}"; then
    echo "ERROR: XR release APK contains an unsupported native ABI." >&2
    exit 1
fi
for runtime_lib in libmain.so libSDL3.so libdxvk_d3d8.so libdxvk_d3d9.so libopenal.so libgamespy.so; do
    if [[ "${apk_entries}" != *"lib/arm64-v8a/${runtime_lib}"* ]]; then
        echo "ERROR: release APK is missing ARM64 runtime ${runtime_lib}." >&2
        exit 1
    fi
done

tmp_dir="$(mktemp -d)"
trap 'rm -rf "${tmp_dir}"' EXIT
lineage="${tmp_dir}/signing-lineage.bin"
signed_tmp="${tmp_dir}/signed.apk"

# The old public debug signer is granted installed-data and signature-permission
# continuity (needed by AndroidX's dynamic receiver permission during install).
# It cannot sign a rollback/update once the new key is active.
"${apksigner}" rotate --out "${lineage}" \
    --old-signer --ks "${debug_keystore}" --ks-key-alias androiddebugkey \
    --ks-pass pass:android --key-pass pass:android \
    --set-installed-data true --set-shared-uid false \
    --set-permission true --set-rollback false --set-auth false \
    --new-signer --ks "${release_keystore}" --ks-key-alias "${release_alias}" \
    --ks-pass env:GX_XR_RELEASE_KEYSTORE_PASSWORD \
    --key-pass env:GX_XR_RELEASE_KEYSTORE_PASSWORD

# API 28 is the APK's existing floor. Explicitly target v3 rotation there so
# all supported Android versions use the private signer, not the old debug key.
"${apksigner}" sign --ks "${debug_keystore}" \
    --ks-key-alias androiddebugkey --ks-pass pass:android \
    --key-pass pass:android --next-signer \
    --ks "${release_keystore}" --ks-key-alias "${release_alias}" \
    --ks-pass env:GX_XR_RELEASE_KEYSTORE_PASSWORD \
    --key-pass env:GX_XR_RELEASE_KEYSTORE_PASSWORD \
    --v1-signing-enabled false --v2-signing-enabled true \
    --lineage "${lineage}" --rotation-min-sdk-version 28 \
    --out "${signed_tmp}" "${unsigned_apk}"
"${apksigner}" verify --verbose --print-certs "${signed_tmp}" > "${tmp_dir}/verification.txt"

if ! grep -qi "Signer #1 certificate SHA-256 digest: ${expected_release_cert}" "${tmp_dir}/verification.txt"; then
    echo "ERROR: signed APK does not use the pinned private XR release certificate." >&2
    exit 1
fi
mkdir -p "$(dirname "${signed_apk}")"
cp "${signed_tmp}" "${signed_apk}"
echo "==> Release-signed XR APK: ${signed_apk}"
"${apksigner}" verify --verbose --print-certs "${signed_apk}" | grep -E 'Verified using v[23]|Signer #1 certificate SHA-256 digest'
