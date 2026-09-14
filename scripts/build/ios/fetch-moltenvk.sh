#!/bin/bash
# Fetch the dynamic MoltenVK.framework for iOS (DXVK dlopens it from the app
# bundle at runtime). Pinned to a specific release so the packaged driver is
# reproducible; bump MVK_VERSION + MVK_SHA256 together.
set -euo pipefail

MVK_VERSION="v1.4.1"
MVK_SHA256="54336b90212c390ed5935c96460aed3bf651ad7d3c0f0e956586ce18e9c0b701"
DEST="${GX_MOLTENVK_ROOT:-${HOME}/GeneralsX/MoltenVK}"

mkdir -p "${DEST}"
cd "${DEST}"
if [[ -d "MoltenVK/MoltenVK/dynamic/MoltenVK.xcframework/ios-arm64/MoltenVK.framework" ]]; then
    echo "MoltenVK already staged at ${DEST}"
    exit 0
fi
echo "==> Downloading MoltenVK ${MVK_VERSION} (iOS)"
curl -fL -o MoltenVK-ios.tar \
    "https://github.com/KhronosGroup/MoltenVK/releases/download/${MVK_VERSION}/MoltenVK-ios.tar"
echo "${MVK_SHA256}  MoltenVK-ios.tar" | shasum -a 256 -c -
tar -xf MoltenVK-ios.tar
echo "==> Staged: ${DEST}/MoltenVK/MoltenVK/dynamic/MoltenVK.xcframework/ios-arm64/MoltenVK.framework"
