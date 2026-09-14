#!/usr/bin/env bash
# Validate synthetic Steam/installed-retail layouts, optionally a real directory (read-only).
# Usage: JAVA_HOME=/path/to/jdk bash scripts/qa/game-data-validator-test.sh [ZERO_HOUR [GENERALS]]
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
qa_dir="$(mktemp -d "${TMPDIR:-/tmp}/generals-data-qa.XXXXXX")"
qa_java="${JAVA_HOME:+$JAVA_HOME/bin/}java"
qa_javac="${JAVA_HOME:+$JAVA_HOME/bin/}javac"
"$qa_javac" --release 8 -d "$qa_dir" \
    "$repo_dir/android/app/src/main/java/com/generalsx/zerohour/GameDataValidator.java" \
    "$repo_dir/scripts/qa/GameDataValidatorTest.java"
"$qa_java" -cp "$qa_dir" com.generalsx.zerohour.GameDataValidatorTest
if [[ $# -gt 0 ]]; then
    "$qa_java" -cp "$qa_dir" com.generalsx.zerohour.GameDataValidatorTest "$@"
fi
echo "Test classes and generated fixtures remain in temporary storage; classes: $qa_dir"
