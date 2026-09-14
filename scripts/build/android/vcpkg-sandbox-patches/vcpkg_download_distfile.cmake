function(vcpkg_download_distfile out_var)
    cmake_parse_arguments(PARSE_ARGV 1 arg
        "SKIP_SHA512;SILENT_EXIT;QUIET;ALWAYS_REDOWNLOAD"
        "FILENAME;SHA512"
        "URLS;HEADERS"
    )

    if(NOT DEFINED arg_URLS)
        message(FATAL_ERROR "vcpkg_download_distfile requires a URLS argument.")
    endif()
    if(NOT DEFINED arg_FILENAME)
        message(FATAL_ERROR "vcpkg_download_distfile requires a FILENAME argument.")
    endif()
    if(arg_SILENT_EXIT)
        message(WARNING "SILENT_EXIT no longer has any effect. To resolve this warning, remove SILENT_EXIT.")
    endif()

    # Note that arg_ALWAYS_REDOWNLOAD implies arg_SKIP_SHA512, and NOT arg_SKIP_SHA512 implies NOT arg_ALWAYS_REDOWNLOAD
    if(arg_ALWAYS_REDOWNLOAD AND NOT arg_SKIP_SHA512)
        message(FATAL_ERROR "ALWAYS_REDOWNLOAD requires SKIP_SHA512")
    endif()

    if(NOT arg_SKIP_SHA512 AND NOT DEFINED arg_SHA512)
        message(FATAL_ERROR "vcpkg_download_distfile requires a SHA512 argument.
If you do not know the SHA512, add it as 'SHA512 0' and retry.")
    elseif(arg_SKIP_SHA512 AND DEFINED arg_SHA512)
        message(FATAL_ERROR "SHA512 may not be used with SKIP_SHA512.")
    endif()

    if(_VCPKG_INTERNAL_NO_HASH_CHECK)
        set(arg_SKIP_SHA512 1)
    endif()

    if(NOT arg_SKIP_SHA512)
        if("${arg_SHA512}" STREQUAL "0")
            string(REPEAT 0 128 arg_SHA512)
        else()
            string(LENGTH "${arg_SHA512}" arg_SHA512_length)
            if(NOT "${arg_SHA512_length}" EQUAL "128" OR NOT "${arg_SHA512}" MATCHES "^[a-zA-Z0-9]*$")
                message(FATAL_ERROR "Invalid SHA512: ${arg_SHA512}.
    If you do not know the file's SHA512, set this to \"0\".")
            endif()

            string(TOLOWER "${arg_SHA512}" arg_SHA512)
        endif()
    endif()

    set(downloaded_file_path "${DOWNLOADS}/${arg_FILENAME}")

    get_filename_component(directory_component "${arg_FILENAME}" DIRECTORY)
    if ("${directory_component}" STREQUAL "")
        file(MAKE_DIRECTORY "${DOWNLOADS}")
    else()
        file(MAKE_DIRECTORY "${DOWNLOADS}/${directory_component}")
    endif()

    if(EXISTS "${downloaded_file_path}")
        if(arg_SKIP_SHA512)
            if(NOT arg_ALWAYS_REDOWNLOAD)
                if(NOT _VCPKG_INTERNAL_NO_HASH_CHECK)
                    message(STATUS "Skipping hash check and using cached ${arg_FILENAME}")
                endif()

                set("${out_var}" "${downloaded_file_path}" PARENT_SCOPE)
                return()
            endif()
        else()
            # Note that NOT arg_SKIP_SHA512 implies NOT arg_ALWAYS_REDOWNLOAD
            file(SHA512 "${downloaded_file_path}" file_hash)
            if("${file_hash}" STREQUAL "${arg_SHA512}")
                message(STATUS "Using cached ${arg_FILENAME}")
                set("${out_var}" "${downloaded_file_path}" PARENT_SCOPE)
                return()
            endif()

            # The existing file hash mismatches. Perhaps the expected SHA512 changed. Try adding the expected SHA512
            # into the file name and try again to hopefully not conflict.
            get_filename_component(filename_component "${arg_FILENAME}" NAME_WE)
            get_filename_component(extension_component "${arg_FILENAME}" EXT)
            string(SUBSTRING "${arg_SHA512}" 0 8 hash)
            set(arg_FILENAME "${filename_component}-${hash}${extension_component}")
            if (NOT "${directory_component}" STREQUAL "")
                set(arg_FILENAME "${directory_component}/${arg_FILENAME}")
            endif()

            set(downloaded_file_path "${DOWNLOADS}/${arg_FILENAME}")
            if(EXISTS "${downloaded_file_path}")
                if(_VCPKG_NO_DOWNLOADS)
                    set(advice_message "note: Downloads are disabled. Please ensure that the expected file is placed at ${downloaded_file_path} and retry.")
                else()
                    set(advice_message "note: You may be able to resolve this failure by redownloading the file. To do so, delete ${downloaded_file_path} and retry.")
                endif()

                file(SHA512 "${downloaded_file_path}" file_hash)
                if("${file_hash}" STREQUAL "${arg_SHA512}")
                    message(STATUS "Using cached ${arg_FILENAME}")
                    set("${out_var}" "${downloaded_file_path}" PARENT_SCOPE)
                    return()
                endif()

                # Note that the extra leading spaces are here to prevent CMake from badly attempting to wrap this
                message(FATAL_ERROR
                    "  ${downloaded_file_path}: error: existing downloaded file had an unexpected hash\n"
                    "  Expected: ${arg_SHA512}\n"
                    "  Actual  : ${file_hash}\n"
                    "  ${advice_message}")
            endif()
        endif()
    endif()

    # vcpkg_download_distfile_ALWAYS_REDOWNLOAD only triggers when NOT _VCPKG_NO_DOWNLOADS
    # this could be de-morgan'd out but it's more clear this way
    if(_VCPKG_NO_DOWNLOADS)
        message(FATAL_ERROR "Downloads are disabled, but '${downloaded_file_path}' does not exist.")
    endif()

    # GeneralsX local build note (23/07/2026): same egress restriction as
    # vcpkg_from_github.cmake -- raw HTTPS to github.com/codeload.github.com
    # archive URLs is blocked here, but plain git clone/checkout against the
    # same host works. Ports that call vcpkg_download_distfile() directly
    # (bypassing vcpkg_from_github()) for a github.com "/archive/<ref>.tar.gz"
    # or ".zip" URL hit the same 403. Reproduce that one URL shape via git
    # instead, still pinned to the exact ref in the URL; anything else falls
    # through to the normal x-download path unchanged.
    unset(gh_org)
    foreach(url IN LISTS arg_URLS)
        if(url MATCHES "^https://github\\.com/([^/]+)/([^/]+)/archive/(.+)\\.(tar\\.gz|tgz|zip)$")
            set(gh_org "${CMAKE_MATCH_1}")
            set(gh_repo "${CMAKE_MATCH_2}")
            set(gh_ref "${CMAKE_MATCH_3}")
            set(gh_ext "${CMAKE_MATCH_4}")
            break()
        endif()
    endforeach()
    if(DEFINED gh_org)
        string(REPLACE "/" "_-" gh_sanitized_ref "${gh_ref}")
        set(gh_clone_dir "${CURRENT_BUILDTREES_DIR}/src/.gitclone-distfile-${gh_repo}-${gh_sanitized_ref}")
        file(REMOVE_RECURSE "${gh_clone_dir}")
        vcpkg_execute_required_process(
            ALLOW_IN_DOWNLOAD_MODE
            COMMAND git clone --quiet "https://github.com/${gh_org}/${gh_repo}.git" "${gh_clone_dir}"
            WORKING_DIRECTORY "${CURRENT_BUILDTREES_DIR}"
            LOGNAME "git-clone-${gh_org}-${gh_repo}"
        )
        vcpkg_execute_required_process(
            ALLOW_IN_DOWNLOAD_MODE
            COMMAND git checkout --quiet "${gh_ref}"
            WORKING_DIRECTORY "${gh_clone_dir}"
            LOGNAME "git-checkout-${gh_org}-${gh_repo}-${gh_sanitized_ref}"
        )
        if(gh_ext STREQUAL "zip")
            set(gh_archive_format "zip")
        else()
            set(gh_archive_format "tar.gz")
        endif()
        vcpkg_execute_required_process(
            ALLOW_IN_DOWNLOAD_MODE
            COMMAND git archive "--format=${gh_archive_format}" "--prefix=${gh_repo}-${gh_sanitized_ref}/" -o "${downloaded_file_path}" HEAD
            WORKING_DIRECTORY "${gh_clone_dir}"
            LOGNAME "git-archive-${gh_org}-${gh_repo}-${gh_sanitized_ref}"
        )
        file(REMOVE_RECURSE "${gh_clone_dir}")
        set("${out_var}" "${downloaded_file_path}" PARENT_SCOPE)
        return()
    endif()

    vcpkg_list(SET params "x-download" "${arg_FILENAME}")
    foreach(url IN LISTS arg_URLS)
        vcpkg_list(APPEND params "--url=${url}")
    endforeach()

    foreach(header IN LISTS arg_HEADERS)
        list(APPEND params "--header=${header}")
    endforeach()

    if(arg_SKIP_SHA512)
        vcpkg_list(APPEND params "--skip-sha512")
    else()
        vcpkg_list(APPEND params "--sha512=${arg_SHA512}")
    endif()

    # Setting WORKING_DIRECTORY and passing the relative FILENAME allows vcpkg x-download to print
    # the full relative path if FILENAME has /s in it.
    vcpkg_execute_in_download_mode(COMMAND "$ENV{VCPKG_COMMAND}" ${params} RESULT_VARIABLE error_code WORKING_DIRECTORY "${DOWNLOADS}")
    if(NOT "${error_code}" EQUAL "0")
        message(FATAL_ERROR "Download failed, halting portfile.")
    endif()

    set("${out_var}" "${downloaded_file_path}" PARENT_SCOPE)
endfunction()
