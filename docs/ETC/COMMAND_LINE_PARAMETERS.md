# Command Line Parameters

Common command line parameters for `GeneralsX` (Generals) and `GeneralsXZH` (Zero Hour).

## Window & Display

| Parameter | Description | Example |
|-----------|-------------|---------|
| `-win` | Forces windowed mode | `./GeneralsXZH -win` |
| `-fullscreen` | Forces fullscreen mode | `./GeneralsXZH -fullscreen` |
| `-xres <width>` | Sets horizontal resolution | `./GeneralsXZH -xres 1920` |
| `-yres <height>` | Sets vertical resolution | `./GeneralsXZH -yres 1080` |

## Development & Testing

| Parameter | Description | Example |
|-----------|-------------|---------|
| `-noshellmap` | Disables the shell map (skip intro) | `./GeneralsXZH -noshellmap` |
| `-quickstart` | Quick launch (skip movies + shell) | `./GeneralsXZH -quickstart` |
| `-debug` | Enable debug mode | `./GeneralsXZH -debug` |
| `-logToCon` | Enables legacy debug-log console routing (`DEBUG_LOG`). **Debug builds only** (`ALLOW_DEBUG_UTILS` / `RTS_BUILD_OPTION_DEBUG=ON`); ignored in release builds. | `./GeneralsXZH -logToCon` |

### Opt-in LAN synchronization trace (Zero Hour)

This is a diagnostic option, not a multiplayer compatibility fix. Set
`GX_LAN_CRC=1` in the process environment, or create an empty `gx_lan_crc.txt`
in the selected game-data/working directory. On Android/Quest, use
**Setup → Diagnostics → LAN sync checkpoints**, then restart the game.
The opt-in is checked at each live LAN match start; offline play and replay
remain silent. Remove the marker and unset the environment variable to disable
it. `GX_LAN_CRC=0` does not override an existing marker.

Release builds write `[GX-LAN-CRC]` records to stderr without `-logToCon`:
match metadata, the first eight scheduled CRC generations and validations,
and at most one additional local mismatch after the normal output budget.
The negotiated CRC interval, game messages and simulation rules are unchanged.
Generation frames and validation frames are distinct; the retail CRC message
does not carry its generation frame. Intermediate values after objects, RNG,
partition, players and AI are **rolling** CRCs, not independent subsystem hashes.
`detector_reason` is the game's actual decision; `reason` separately checks
whether every connected slot has a value and whether those values agree.
Received entries use `s<network-slot>/p<engine-player-index>:<CRC>`; do not
assume that those two indices are identical.

On Quest, **View Logs → Share** includes the full current and previous
`generals-xr-stderr.log` files. The on-screen preview may be truncated. Capture
the logs promptly after reproducing, before repeated restarts rotate them out.
No player names or IP addresses are added by this trace, but the complete
existing game logs can contain personal paths/network details: review before
sharing publicly. Stock Steam peers expose no matching subsystem trace, so a
same-source peer may still be required to isolate a divergent subsystem.

For an independently staged Linux comparison, run
`bash scripts/build/linux/run-lan-diagnostic-zh.sh /absolute/path/to/lab`
from the PC's graphical terminal. The launcher enables this observer, adds
`-win -quickstart`, and writes a unique `logs/native-zh-*` file on every run.
It requires a separate `game/` data copy and `runtime/` binary/library set;
it does not install assets or make an incompatible retail peer synchronize.
Do not use debug builds or add optional game patches for the comparison.

## Mods & Content

| Parameter | Description | Example |
|-----------|-------------|---------|
| `-mod <path>` | Loads a mod from directory or .big file | `./GeneralsXZH -mod /path/to/mod.big` |

## Replay & Multiplayer

| Parameter | Description | Example |
|-----------|-------------|---------|
| `-replay <file>` | Play a replay file | `./GeneralsXZH -replay match.rep` |
| `-jobs <count>` | Number of parallel replay jobs | `./GeneralsXZH -jobs 4 -replay *.rep` |
| `-headless` | Run without graphics (replay testing) | `./GeneralsXZH -headless -replay *.rep` |

## Common Combinations

### Quick Testing
```bash
./GeneralsXZH -win -noshellmap
```
Launch in windowed mode, skip intro.

### Replay Compatibility Testing
```bash
./GeneralsXZH -jobs 4 -headless -replay subfolder/*.rep
```
Test multiple replays in parallel without graphics (requires optimized VC6 build with `RTS_BUILD_OPTION_DEBUG=OFF`).

### High Resolution Testing
```bash
./GeneralsXZH -win -xres 2560 -yres 1440
```
Test in windowed mode at 1440p resolution.

## Platform-Specific Notes

### Windows
- Parameters can use `/` or `-` prefix (both work)
- Paths can use backslashes

### Linux
- Must use `-` prefix
- Paths must use forward slashes
- Some parameters may not work until Linux port is complete
- `-logToCon` sets a debug flag, but many Linux diagnostics still require explicit `fprintf(stderr, ...)` instrumentation because `OutputDebugString` paths are stubbed/non-visible on this platform.
- **`-logToCon` is only available in debug builds** (`RTS_BUILD_OPTION_DEBUG=ON` / `ALLOW_DEBUG_UTILS` defined). It is unrecognized and has no effect in release builds.

## Logging Diagnostics Recipe

Use this when investigating runtime behavior (example: skirmish startup flow):

```bash
cd ~/GeneralsX/GeneralsZH
./run.sh -win -logToCon 2>&1 | grep -v "D3DRS_PATCHSEGMENTS" | tee ~/Projects/GeneralsX/logs/manual_run.log
```

Legacy fallback during migration:

```bash
cd ~/GeneralsX/GeneralsMD
./run.sh -win -logToCon 2>&1 | grep -v "D3DRS_PATCHSEGMENTS" | tee ~/Projects/GeneralsX/logs/manual_run.log
```

Then filter the generated log for targeted markers:

```bash
grep -n "SKIRMISH_DIAG\|ScoreScreen\|SkirmishGameOptionsMenu" ~/Projects/GeneralsX/logs/manual_run.log
```

## Source Code Reference

Command line parsing is implemented in:
- `Core/GameEngine/Source/gameclient.cpp` - Client-side parameters
- `GeneralsMD/Code/Main/WinMain.cpp` - Entry point and initial parsing
