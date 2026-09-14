# pkg-config splits "-framework CoreFoundation" into two separate list items.
# On the PkgConfig::FFMPEG imported target this poisons both INTERFACE_LINK_LIBRARIES
# and INTERFACE_LINK_OPTIONS; CMake additionally de-duplicates interface link
# options token-wise, mangling the list into "-framework VideoToolbox CoreFoundation
# CoreMedia CoreVideo" which the linker reads as one flag plus bare filenames.
# Merge every "-framework;X" pair back into a single "-framework X" item, and move
# frameworks from link options into link libraries where ordering is preserved.
#
# NOTE: pkg_check_modules(IMPORTED_TARGET) without GLOBAL creates DIRECTORY-scoped
# imported targets, so this must be called after EVERY pkg_check_modules(FFMPEG ...)
# call site — a fixed target in one directory is invisible in sibling directories.
function(sage_fix_ffmpeg_framework_flags)
    if(NOT APPLE OR NOT TARGET PkgConfig::FFMPEG)
        return()
    endif()

    get_target_property(_libs PkgConfig::FFMPEG INTERFACE_LINK_LIBRARIES)
    if(_libs)
        set(_fixed "")
        set(_pending FALSE)
        foreach(_item IN LISTS _libs)
            if(_item STREQUAL "-framework")
                set(_pending TRUE)
            elseif(_pending)
                list(APPEND _fixed "-framework ${_item}")
                set(_pending FALSE)
            else()
                list(APPEND _fixed "${_item}")
            endif()
        endforeach()
        set_target_properties(PkgConfig::FFMPEG PROPERTIES
            INTERFACE_LINK_LIBRARIES "${_fixed}")
    endif()

    get_target_property(_opts PkgConfig::FFMPEG INTERFACE_LINK_OPTIONS)
    if(_opts)
        set(_fixed_opts "")
        set(_fw_libs "")
        set(_pending FALSE)
        foreach(_item IN LISTS _opts)
            if(_item STREQUAL "-framework")
                set(_pending TRUE)
            elseif(_pending)
                list(APPEND _fw_libs "-framework ${_item}")
                set(_pending FALSE)
            else()
                list(APPEND _fixed_opts "${_item}")
            endif()
        endforeach()
        set_target_properties(PkgConfig::FFMPEG PROPERTIES
            INTERFACE_LINK_OPTIONS "${_fixed_opts}")
        if(_fw_libs)
            target_link_libraries(PkgConfig::FFMPEG INTERFACE ${_fw_libs})
        endif()
    endif()
endfunction()
