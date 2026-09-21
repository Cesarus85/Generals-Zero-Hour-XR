# GeneralsX @feature fbraz 03/05/2026
# Deterministic cross-platform math library integration (Phase 4)
# GameMath: fdlibm-based deterministic math functions for bit-exact replay validation
#
# Strategy:
# - Enable deterministic math via FetchContent (not on VC6, which uses x87 asm)
# - Define USE_DETERMINISTIC_MATH compile flag when enabled
# - Wrappers in wwmath.h conditionally dispatch to GameMath (deterministic) or CRT (fast fallback)
#
# Reference: Okladnoj et al., PR #2670, TheSuperHackers/GeneralsGameCode
# https://github.com/TheSuperHackers/GeneralsGameCode/pull/2670
#
# Note: GameMath source location is configurable via SAGE_GAMEMATH_GIT_REPO
# Default: pinned GameMath release used by the upstream deterministic-math work
#
# Upstream reference: fdlibm (Berkeley math library) provides platform-independent
# implementations of standard math functions (sin, cos, sqrt, atan2, etc.) that produce
# identical results on all architectures when compiled with the same precision flags.

# Enable deterministic math only for non-VC6 builds (VC6 uses native x87 asm)
# Note: Currently defaults to OFF until GameMath is available as a proper library/submodule
if(NOT IS_VS6_BUILD)
    option(SAGE_USE_DETERMINISTIC_MATH "Use fdlibm-based deterministic math for cross-platform replay validation" OFF)
else()
    # VC6 uses native x87 inline asm; deterministic mode not applicable
    set(SAGE_USE_DETERMINISTIC_MATH OFF)
endif()

if(SAGE_USE_DETERMINISTIC_MATH)
    message(STATUS "Configuring GameMath (fdlibm-based deterministic math)...")

    include(FetchContent)

    # FetchContent declaration for GameMath library
    # GeneralsX @bugfix Codex 21/09/2026 Fetch the actual GameMath project rather than
    # recursively fetching GeneralsGameCode, whose placeholder SOURCE_SUBDIR never
    # provided a gamemath target.
    # Can be overridden via cmake -DSAGE_GAMEMATH_GIT_REPO=<url> -DSAGE_GAMEMATH_GIT_TAG=<tag>
    if(NOT SAGE_GAMEMATH_GIT_REPO)
        set(SAGE_GAMEMATH_GIT_REPO "https://github.com/TheSuperHackers/GameMath.git")
    endif()
    
    if(NOT SAGE_GAMEMATH_GIT_TAG)
        # Pinned SHA: tracking "main" let the deterministic-math source drift silently.
        set(SAGE_GAMEMATH_GIT_TAG "59f7ccd494f7e7c916a784ac26ef266f9f09d78d")
    endif()

    FetchContent_Declare(
        gamemath
        GIT_REPOSITORY ${SAGE_GAMEMATH_GIT_REPO}
        GIT_TAG ${SAGE_GAMEMATH_GIT_TAG}
    )

    # Minimal GameMath configuration
    set(GM_ENABLE_TESTS OFF CACHE BOOL "Disable GameMath tests" FORCE)
    set(GM_ENABLE_INTRINSICS OFF CACHE BOOL "Use the same software path on every architecture" FORCE)
    set(gamemath_SHARED_LIBS OFF CACHE BOOL "Link deterministic math into the game binary" FORCE)

    # Make GameMath available (FetchContent_MakeAvailable is idempotent)
    FetchContent_MakeAvailable(gamemath)

    # wwmath.h is included by simulation targets outside core_wwmath. Keep the
    # deterministic API visible wherever the globally selected wrapper is used.
    include_directories(${gamemath_SOURCE_DIR}/include)

    # Add USE_DETERMINISTIC_MATH to all compile definitions for this project
    # This enables conditional compilation in wwmath.h and trig wrappers
    add_compile_definitions(USE_DETERMINISTIC_MATH)

    message(STATUS "GameMath deterministic math enabled (fdlibm backend)")
    message(STATUS "  Math operations will be bit-exact across platforms")
    message(STATUS "  Performance: Slightly slower than CRT but guarantees replay determinism")

else()
    message(STATUS "Deterministic math disabled (SAGE_USE_DETERMINISTIC_MATH=OFF)")
    message(STATUS "  Math operations will use platform-native CRT/x87")
    message(STATUS "  Note: Replays may differ between platforms due to FMA/rounding differences")
endif()
