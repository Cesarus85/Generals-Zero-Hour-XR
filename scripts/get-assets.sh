#!/bin/bash
# Download C&C Generals Zero Hour game files from your own Steam account.
# Usage: ./get-assets.sh <your_steam_username>
# Steam Guard: you'll be prompted for the code on first login.
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <steam_username>"
    exit 1
fi

STEAM_USER="$1"
DEST="$HOME/GeneralsX/GeneralsZH"
TMP_DIR="$HOME/GeneralsX/.steamcmd_zh"

mkdir -p "$TMP_DIR" "$DEST"

# App 2732960 = C&C Generals Zero Hour (Windows depot; assets are platform-independent)
steamcmd \
    +@sSteamCmdForcePlatformType windows \
    +force_install_dir "$TMP_DIR" \
    +login "$STEAM_USER" \
    +app_update 2732960 validate \
    +quit

echo "Moving game files into $DEST ..."
# Copy data files only; keep the deployed GeneralsX runtime (run.sh, dylibs) intact.
rsync -a --exclude="*.exe" --exclude="*.dll" "$TMP_DIR/" "$DEST/"

echo "Done. Assets in place:"
ls "$DEST"/*.big 2>/dev/null | head
echo
echo "Launch with: cd ~/GeneralsX/GeneralsZH && ./run.sh -win"
