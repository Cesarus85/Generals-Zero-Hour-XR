#!/usr/bin/env bash
# GeneralsX @test Codex 22/09/2026 Production ring-buffer tests plus comparator fixtures; no game assets needed.
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -r -- "$scratch"' EXIT
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
    -I"$repo/Core/Libraries/Include" "$repo/scripts/qa/lan-snapshot-test.cpp" -o "$scratch/test"
for variant in bounds equal different late; do
    if [[ "$variant" == bounds ]]; then
        (cd "$scratch" && ./test 2>bounds.log)
    else
        (cd "$scratch" && ./test "$variant" 2>"$variant.log")
    fi
done
python3 "$repo/scripts/qa/lan-snapshot-compare-test.py" "$scratch"
mkdir "$scratch/Common"
# Reuse the actual argument declarations, with only message ownership replaced by a test double.
python3 - "$repo" "$scratch" <<'PY'
from pathlib import Path
import sys
repo, scratch = map(Path, sys.argv[1:])
source = (repo / 'GeneralsMD/Code/GameEngine/Include/Common/MessageStream.h').read_text()
arguments = source[source.index('union GameMessageArgumentType'):source.index('class GameMessageArgument :')]
(scratch / 'Common/MessageStream.h').write_text('''#pragma once
#define CPP_11(x) x
using Int=int; using Real=float; using Bool=bool; using UnsignedInt=unsigned int;
using ObjectID=int; using DrawableID=int; using WideChar=unsigned int;
struct Coord3D { float x,y,z; }; struct ICoord2D { int x,y; }; struct IRegion2D { ICoord2D lo,hi; };
''' + arguments + '''
struct GameMessage {
    enum { MSG_BEGIN_NETWORK_MESSAGES=1000, MSG_END_NETWORK_MESSAGES=1999, MSG_LOGIC_CRC=1800 };
    int type=0,player=0,argc=0;
    GameMessageArgumentType args[32]; GameMessageArgumentDataType types[32];
    int getType() const {return type;} int getPlayerIndex() const {return player;}
    int getArgumentCount() const {return argc;}
    const char *getCommandAsString() const {return "test";}
    const GameMessageArgumentType *getArgument(int i) const {return &args[i];}
    GameMessageArgumentDataType getArgumentDataType(int i) const {return types[i];}
};
''')
PY
"${CXX:-clang++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined \
    -I"$scratch" -I"$repo/Core/Libraries/Include" -I"$repo/GeneralsMD/Code/GameEngine/Include" \
    "$repo/scripts/qa/lan-snapshot-command-test.cpp" -o "$scratch/commands"
(cd "$scratch" && ./commands 2>commands.log)
if rg -q '\[GX-LAN-CRC\]' "$scratch/equal.log"; then
    echo 'Legacy probes must be suppressed in universal mode' >&2; exit 1
fi
echo 'LAN snapshot: production lifecycle, late mismatch, bounds and comparator tests passed'
