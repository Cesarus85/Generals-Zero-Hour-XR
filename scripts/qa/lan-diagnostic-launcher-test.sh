#!/usr/bin/env bash
# GeneralsX @build Codex 16/09/2026 Exercise the isolated PC launcher without retail data or a game process.
set -euo pipefail

if [[ "${GX_FAKE_BINARY:-0}" == 1 ]]; then
    printf 'FAKE args:'
    printf ' <%s>' "$@"
    printf '\n'
    for name in HOME XDG_CONFIG_HOME XDG_DATA_HOME XDG_CACHE_HOME CNC_GENERALS_ZH_PATH CNC_GENERALS_PATH CNC_ZH_INSTALLPATH CNC_GENERALS_INSTALLPATH DXVK_LOG_PATH DXVK_STATE_CACHE_PATH DXVK_CONFIG_FILE GX_LAN_CRC SAGE_PATCH_DISABLED LD_LIBRARY_PATH LD_PRELOAD; do
        printf 'FAKE %s=%s\n' "$name" "${!name-}"
    done
    exit "${GX_FAKE_EXIT:-0}"
fi

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
launcher="$repo_root/scripts/build/linux/run-lan-diagnostic-zh.sh"
test_root="$(mktemp -d)"
test_root="$(cd "$test_root" && pwd -P)"
trap 'rm -r -- "$test_root"' EXIT
lab="$test_root/lab with spaces"
mkdir -p "$lab/runtime" "$lab/game/ZH_Generals" "$lab/game/Data/Scripts"
cp -- "$0" "$lab/runtime/GeneralsXZH"
chmod +x "$lab/runtime/GeneralsXZH"
touch "$lab/runtime/libdxvk_d3d8.so" "$lab/runtime/libSDL3.so" "$lab/runtime/libSDL3_image.so" "$lab/runtime/libgamespy.so"
touch "$lab/game/INIZH.big" "$lab/game/MapsZH.big" "$lab/game/PatchZH.big"
touch "$lab/game/Data/Scripts/MultiplayerScripts.scb" "$lab/game/Data/Scripts/SkirmishScripts.scb"
touch "$lab/game/ZH_Generals/INI.big" "$lab/game/ZH_Generals/Patch.big" "$lab/game/ZH_Generals/maps.big"

export GX_FAKE_BINARY=1
export DISPLAY=:99
set +e
GX_FAKE_EXIT=17 LD_PRELOAD="$test_root/inherited-preload.so" bash "$launcher" "$lab" '-test argument with spaces' >"$test_root/first.out" 2>&1
first_status=$?
set -e
[[ "$first_status" -eq 17 ]] || { printf 'Expected child exit 17, got %s\n' "$first_status" >&2; exit 1; }
rg -Fq 'FAKE args: <-win> <-quickstart> <-test argument with spaces>' "$test_root/first.out"
rg -Fq "FAKE HOME=$HOME" "$test_root/first.out"
rg -Fq "FAKE XDG_CONFIG_HOME=$lab/config" "$test_root/first.out"
rg -Fq "FAKE XDG_DATA_HOME=$lab/data" "$test_root/first.out"
rg -Fq "FAKE XDG_CACHE_HOME=$lab/cache" "$test_root/first.out"
rg -Fq "FAKE CNC_GENERALS_ZH_PATH=$lab/game/" "$test_root/first.out"
rg -Fq "FAKE CNC_GENERALS_PATH=$lab/game/ZH_Generals/" "$test_root/first.out"
rg -Fq "FAKE CNC_ZH_INSTALLPATH=$lab/game/" "$test_root/first.out"
rg -Fq "FAKE CNC_GENERALS_INSTALLPATH=$lab/game/ZH_Generals/" "$test_root/first.out"
rg -Fq "FAKE DXVK_LOG_PATH=$lab/logs" "$test_root/first.out"
rg -Fq "FAKE DXVK_STATE_CACHE_PATH=$lab/cache" "$test_root/first.out"
rg -Fq "FAKE DXVK_CONFIG_FILE=$lab/config/dxvk.conf" "$test_root/first.out"
rg -Fq 'FAKE GX_LAN_CRC=1' "$test_root/first.out"
rg -Fq 'FAKE SAGE_PATCH_DISABLED=1' "$test_root/first.out"
rg -Fq "FAKE LD_LIBRARY_PATH=$lab/runtime" "$test_root/first.out"
rg -q '^FAKE LD_PRELOAD=$' "$test_root/first.out"

if ! GX_FAKE_EXIT=0 bash "$launcher" "$lab" >"$test_root/second.out" 2>&1; then
    printf 'Second launch unexpectedly failed:\n' >&2
    sed -n '1,80p' "$test_root/second.out" >&2
    exit 1
fi
[[ "$(find "$lab/logs" -maxdepth 1 -type f -name 'native-zh-*' | wc -l)" -eq 2 ]]

mv "$lab/game/ZH_Generals/Patch.big" "$test_root/Patch.big"
if bash "$launcher" "$lab" >"$test_root/missing.out" 2>&1; then
    printf 'Missing asset unexpectedly passed\n' >&2
    exit 1
fi
rg -Fq 'Copied base Generals asset missing' "$test_root/missing.out"
mv "$test_root/Patch.big" "$lab/game/ZH_Generals/Patch.big"

if env -u DISPLAY -u WAYLAND_DISPLAY bash "$launcher" "$lab" >"$test_root/display.out" 2>&1; then
    printf 'Headless launch unexpectedly passed\n' >&2
    exit 1
fi
rg -Fq 'Open a terminal on the PC desktop' "$test_root/display.out"

mv "$lab/game" "$lab/game-copy"
ln -s "$lab/game-copy" "$lab/game"
if bash "$launcher" "$lab" >"$test_root/symlink.out" 2>&1; then
    printf 'Symlinked game directory unexpectedly passed\n' >&2
    exit 1
fi
rg -Fq 'non-symlink copied game directory' "$test_root/symlink.out"
unlink "$lab/game"
mv "$lab/game-copy" "$lab/game"

mv "$lab/config" "$lab/config-copy"
ln -s "$lab/config-copy" "$lab/config"
if bash "$launcher" "$lab" >"$test_root/config-symlink.out" 2>&1; then
    printf 'Symlinked config destination unexpectedly passed\n' >&2
    exit 1
fi
rg -Fq 'Refusing symlinked writable destination' "$test_root/config-symlink.out"
unlink "$lab/config"
mv "$lab/config-copy" "$lab/config"

mkdir -p "$test_root/steamapps"
mv "$lab" "$test_root/steamapps/lab"
ln -s "$test_root/steamapps" "$test_root/alias"
if bash "$launcher" "$test_root/alias/lab" >"$test_root/steam-alias.out" 2>&1; then
    printf 'Steam-path alias unexpectedly passed\n' >&2
    exit 1
fi
rg -Fq 'Refusing to run inside a Steam library' "$test_root/steam-alias.out"

printf 'LAN diagnostic launcher: isolated paths, quoted arguments, exit status, unique logs and safety preflight passed\n'
