# WW common libraries decended from earliest C&C games
add_subdirectory(Source/WWVegas)

# GeneralsX @build Android port GLES experiment - native D3D8->GLES3 backend,
# alternative to DXVK's D3D8->Vulkan translation (Android-only; the module's
# own CMakeLists.txt no-ops on every other platform).
add_subdirectory(Source/d3d8gles)

# profiling library
add_subdirectory(Source/profile)

# debugging library
add_subdirectory(Source/debug)

add_subdirectory(Source/EABrowserDispatch)
add_subdirectory(Source/EABrowserEngine)
add_subdirectory(Source/Compression)
