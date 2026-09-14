#!/bin/bash
# Fetch the prebuilt Khronos Vulkan Validation Layer (arm64-v8a) and stage it
# for bundling into the APK.
#
# Opt-in diagnostic tool: SDL3Main.cpp only sets DXVK_DEBUG=validation when a
# tester drops dxvk_validation.txt into the game data folder. Off by default,
# it's pure dead weight in a normal launch -- but for a crash whose PC/LR
# resolve inside the vendor driver itself (e.g. issue #9's libGLES_mali.so
# SIGSEGV) it turns "the driver died" into an actual Vulkan API-misuse
# message, since Android's loader auto-discovers a layer bundled in a
# debuggable app's own jniLibs, and DXVK's debug-callback output already
# goes to stderr on non-Windows (log.cpp) -- i.e. straight into the
# generals-stderr.log the tester already knows how to export.
set -euo pipefail

VVL_VERSION="1.4.357.0"
VVL_TAG="vulkan-sdk-${VVL_VERSION}"
VVL_URL="https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases/download/${VVL_TAG}/android-binaries-${VVL_VERSION}.zip"
DEST="${GX_VULKAN_VALIDATION:-${HOME}/GeneralsX/android-staging/vulkan_validation}"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

if [[ -f "${DEST}/libVkLayer_khronos_validation.so" ]]; then
    echo "Vulkan validation layer already staged at ${DEST}"
    exit 0
fi

echo "==> Downloading Vulkan Validation Layers ${VVL_VERSION} (Android binaries)"
curl -fL -o "${TMP}/android-binaries.zip" "${VVL_URL}"
unzip -q "${TMP}/android-binaries.zip" -d "${TMP}/extracted"

LIB="$(find "${TMP}/extracted" -path "*/arm64-v8a/libVkLayer_khronos_validation.so" | head -1)"
if [[ -z "${LIB}" || ! -f "${LIB}" ]]; then
    echo "ERROR: libVkLayer_khronos_validation.so not found for arm64-v8a in ${VVL_URL}"
    exit 1
fi

if command -v file >/dev/null 2>&1; then
    ARCH="$(file -b "${LIB}")"
    if [[ "${ARCH}" != *"ARM aarch64"* && "${ARCH}" != *"AArch64"* ]]; then
        echo "ERROR: libVkLayer_khronos_validation.so is not an AArch64 shared object (got: ${ARCH})"
        exit 1
    fi
fi

mkdir -p "${DEST}"
cp "${LIB}" "${DEST}/libVkLayer_khronos_validation.so"
echo "==> Staged Vulkan Validation Layers ${VVL_VERSION} (arm64-v8a) at ${DEST}"
