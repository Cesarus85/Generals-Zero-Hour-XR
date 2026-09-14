#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 First-run DE/EN policy without Android runtime.
# Usage: JAVA_HOME=/path/to/jdk bash scripts/qa/initial-language-test.sh
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-language-test.XXXXXX")"
java_bin="${JAVA_HOME:+$JAVA_HOME/bin/}"
"${java_bin}javac" --release 8 -d "$test_dir" \
 android/app/src/main/java/com/generalsx/zerohour/InitialLanguage.java \
 scripts/qa/InitialLanguageTest.java
"${java_bin}java" -cp "$test_dir" com.generalsx.zerohour.InitialLanguageTest
