#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Public identity and update compatibility.
# Usage: AAPT2=/path/to/aapt2 bash scripts/qa/xr-branding-test.sh [XR_APK] [ZH_APK]
# Requires: xmllint, rg, aapt2; read-only APK/source inspection, no device access.
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
xr_apk="${1:-android/app/build/outputs/apk/xr/debug/app-xr-debug.apk}"
zh_apk="${2:-android/app/build/outputs/apk/zh/debug/app-zh-debug.apk}"
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-branding-test.XXXXXX")"
"${AAPT2:?Set AAPT2 to the Android SDK aapt2}" dump badging "$xr_apk" > "$task_dir/xr.txt"
"$AAPT2" dump badging "$zh_apk" > "$task_dir/zh.txt"
rg -q "^package: name='com.generalsx.zerohour.xr'" "$task_dir/xr.txt"
rg -q "^package: name='com.generalsx.zerohour'" "$task_dir/zh.txt"
rg -q "^application-label:'Generals Zero Hour'$" "$task_dir/zh.txt"
count=0
while IFS= read -r line; do
 test "${line#*:}" = "'Generals: Zero Hour XR'"
 count=$((count+1))
done < <(rg '^application-label' "$task_dir/xr.txt")
test "$count" -ge 13
test "$(rg -c '^launchable-activity:' "$task_dir/xr.txt")" = 1
rg '^launchable-activity:' "$task_dir/xr.txt" | rg -q "name='com.generalsx.zerohour.XrHelloActivity'[[:space:]]+label='Generals: Zero Hour XR'"
locales=0
for source in android/app/src/main/res/values*/strings.xml; do
 target="android/app/src/xr/res/${source#android/app/src/main/res/}"
 for key in app_name setup_title; do
  test "$(xmllint --xpath "string(/resources/string[@name='$key'])" "$target")" = 'Generals: Zero Hour XR'
 done
 xmllint --xpath "string(/resources/string[@name='setup_window_title'])" "$target" | rg -q 'Generals: Zero Hour XR'
 locales=$((locales+1))
done
rg -q 'applicationName, "Generals: Zero Hour XR"' GeneralsMD/Code/Main/XrHello.cpp
rg -q 'engineName, "GeneralsX"' GeneralsMD/Code/Main/XrHello.cpp
rg -q 'localizedActionSetName,"Generals: Zero Hour XR"' GeneralsMD/Code/Main/XrControls.h
cmp "$xr_apk" build/apk/Generals-Zero-Hour-XR.apk
echo "PASS $count compiled app labels, $locales source locale overlays, launcher, runtime identity, unchanged package IDs and product APK"
