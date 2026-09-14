#pragma once

// GeneralsX @bugfix Android port 10/07/2026 stub. GeneralsOnline is a
// Zero Hour-only feature (GeneralsMD/Code/GameEngine/Include/GameNetwork/
// GeneralsOnline/NextGenMP_defines.h is the real one, and unconditionally
// #defines GENERALS_ONLINE). PersistentStorageThread.h (Core/GameEngine,
// shared) includes this same relative path for both games so that its
// "#if defined(GENERALS_ONLINE)" elo_rating/elo_num_matches fields compile
// for base Generals too -- this stub deliberately leaves GENERALS_ONLINE
// undefined, so those fields (and anything else gated the same way) simply
// don't exist for base Generals, same as before GeneralsOnline existed.
