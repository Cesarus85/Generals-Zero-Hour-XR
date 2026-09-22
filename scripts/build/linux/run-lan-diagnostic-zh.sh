#!/usr/bin/env bash
# GeneralsX @build Codex 16/09/2026 Keep the same-source LAN diagnostic out of Steam and the normal user profile.
set -euo pipefail

usage() {
    printf 'Usage: %s /absolute/path/to/lab [game arguments...]\n' "$0" >&2
    printf 'Run this from a graphical terminal on the PC, not a headless SSH session.\n' >&2
}

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 2
}

if (( $# < 1 )); then
    usage
    exit 2
fi

lab="$1"
shift
[[ "$lab" = /* ]] || fail 'Lab directory must be an absolute path.'
[[ "$lab" != / && ! -L "$lab" && -d "$lab" ]] || fail 'Lab directory must exist and must not be a symlink or filesystem root.'
[[ "$lab" != */steamapps/* ]] || fail 'Refusing to run inside a Steam library.'
lab="$(cd -- "$lab" && pwd -P)"
[[ "$lab" != */steamapps/* ]] || fail 'Refusing to run inside a Steam library.'

runtime="$lab/runtime"
game="$lab/game"
[[ -d "$runtime" && ! -L "$runtime" ]] || fail "Missing non-symlink runtime directory: $runtime"
[[ -d "$game" && ! -L "$game" ]] || fail "Missing non-symlink copied game directory: $game"
[[ -x "$runtime/GeneralsXZH" && ! -L "$runtime/GeneralsXZH" ]] || fail "Missing executable: $runtime/GeneralsXZH"
[[ -f "$runtime/libdxvk_d3d8.so" ]] || fail 'Missing libdxvk_d3d8.so in runtime.'
[[ -f "$runtime/libSDL3.so" ]] || fail 'Missing libSDL3.so in runtime.'
[[ -f "$runtime/libSDL3_image.so" ]] || fail 'Missing libSDL3_image.so in runtime.'
[[ -f "$runtime/libgamespy.so" ]] || fail 'Missing libgamespy.so in runtime.'

# The Steam standalone layout keeps the base Generals archives under ZH_Generals.
for asset in INIZH.big MapsZH.big PatchZH.big Data/Scripts/MultiplayerScripts.scb Data/Scripts/SkirmishScripts.scb; do
    [[ -f "$game/$asset" ]] || fail "Copied Zero Hour asset missing: $game/$asset"
done
for asset in INI.big Patch.big maps.big; do
    [[ -f "$game/ZH_Generals/$asset" ]] || fail "Copied base Generals asset missing: $game/ZH_Generals/$asset"
done

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    fail 'No graphical session detected. Open a terminal on the PC desktop and run this launcher there (DISPLAY or WAYLAND_DISPLAY must be set).'
fi

config="$lab/config"
data="$lab/data"
cache="$lab/cache"
logs="$lab/logs"
for destination in "$config" "$data" "$cache" "$logs"; do
    [[ ! -L "$destination" ]] || fail "Refusing symlinked writable destination: $destination"
done
mkdir -p -- "$config" "$data" "$cache" "$logs"
touch -- "$config/dxvk.conf"
log_file="$(mktemp "$logs/native-zh-$(date +%Y%m%d-%H%M%S).XXXXXX")"

# Keep HOME unchanged. The Linux engine uses XDG data/config roots; DXVK's
# otherwise-cwd log and state cache locations are explicitly redirected too.
export XDG_CONFIG_HOME="$config"
export XDG_DATA_HOME="$data"
export XDG_CACHE_HOME="$cache"
export CNC_GENERALS_ZH_PATH="$game/"
export CNC_GENERALS_PATH="$game/ZH_Generals/"
export CNC_ZH_INSTALLPATH="$game/"
export CNC_GENERALS_INSTALLPATH="$game/ZH_Generals/"
export DXVK_LOG_PATH="$logs"
export DXVK_STATE_CACHE_PATH="$cache"
export DXVK_CONFIG_FILE="$config/dxvk.conf"
export DXVK_WSI_DRIVER=SDL3
export DXVK_HUD=0
export GX_LAN_SNAPSHOT=1
export SAGE_PATCH_DISABLED=1
export LD_LIBRARY_PATH="$runtime"
unset LD_PRELOAD

printf 'LAN diagnostic log: %s\n' "$log_file"
cd -- "$game"

set +e
"$runtime/GeneralsXZH" -win -quickstart "$@" 2>&1 | tee -- "$log_file"
statuses=("${PIPESTATUS[@]}")
set -e

printf 'LAN diagnostic log: %s\n' "$log_file"
if (( statuses[0] != 0 )); then
    exit "${statuses[0]}"
fi
exit "${statuses[1]}"
