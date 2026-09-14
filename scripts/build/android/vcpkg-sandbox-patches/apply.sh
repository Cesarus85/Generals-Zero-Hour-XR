#!/usr/bin/env bash
# Apply the sandboxed-dev-environment vcpkg patches (see
# docs/BUILD/ANDROID_SANDBOXED_LOCAL.md for the full story). Only needed when
# building inside an environment whose egress policy blocks raw HTTPS to
# github.com/codeload.github.com but allows plain `git clone`/`fetch` to the
# same host (e.g. Claude Code's web/cloud sandbox) -- vcpkg's own
# hash-checked HTTPS download of GitHub source archives hits a 403 there.
# On a normal machine with unrestricted GitHub access, skip this entirely;
# the patched files behave identically to stock vcpkg when nothing is
# missing from the downloads cache (they only take the git-clone path as a
# fallback for the exact "github.com/.../archive/<ref>.tar.gz|zip" URL
# shape, and only when the target file isn't already downloaded).
#
# Usage: VCPKG_ROOT=/opt/vcpkg ./apply.sh
set -euo pipefail

VCPKG_ROOT="${VCPKG_ROOT:?Set VCPKG_ROOT to the vcpkg checkout to patch}"
PATCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for f in vcpkg_from_github.cmake vcpkg_download_distfile.cmake; do
    dest="${VCPKG_ROOT}/scripts/cmake/${f}"
    if [[ ! -f "${dest}" ]]; then
        echo "ERROR: ${dest} not found -- is VCPKG_ROOT a real vcpkg checkout?"
        exit 1
    fi
    if cmp -s "${PATCH_DIR}/${f}" "${dest}"; then
        echo "==> ${f} already patched, skipping"
        continue
    fi
    cp "${PATCH_DIR}/${f}" "${dest}"
    echo "==> Patched ${dest}"
done
