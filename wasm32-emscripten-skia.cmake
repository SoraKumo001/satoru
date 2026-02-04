set(VCPKG_TARGET_ARCHITECTURE wasm32)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Emscripten)

# Emscripten toolchain path
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "C:/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")

# Force Skia to use "wasm" CPU type
set(VCPKG_GN_TARGET_CPU wasm)