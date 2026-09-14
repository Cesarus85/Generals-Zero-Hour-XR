#!/usr/bin/env bash
# GeneralsX @test Codex 14/09/2026 Production cache and source-release regression.
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_dir"
test_dir="${1:?Supply a test output directory}"
mkdir -p "$test_dir"
src=Core/GameEngineDevice/Source/OpenALAudioDevice/OpenALAudioCache.cpp
{
 for fn in 'ALuint OpenALAudioFileCache::getBufferForFile' 'void OpenALAudioFileCache::closeBuffer' 'void OpenALAudioFileCache::releaseOpenAudioFile' 'Bool OpenALAudioFileCache::freeEnoughSpaceForSample'; do
  sed -n "/^${fn}(/,/^}/p" "$src"
 done
 sed -n '/^void OpenALAudioManager::releaseSampleBuffer(/,/^}/p' Core/GameEngineDevice/Source/OpenALAudioDevice/OpenALAudioManager.cpp
} > "$test_dir/audio-production.inc"
sed -n '/^void OpenALAudioManager::releaseSampleBuffer(/,/^}/p' Core/GameEngineDevice/Source/OpenALAudioDevice/OpenALAudioManager.cpp > "$test_dir/audio-release.inc"
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined -I"$test_dir" \
 scripts/qa/audio-cache-test.cpp -o "$test_dir/audio-cache-test"
"$test_dir/audio-cache-test"
