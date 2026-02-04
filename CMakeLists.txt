cmake_minimum_required(VERSION 3.10)

# Set vcpkg and Emscripten paths
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
set(VCPKG_TARGET_TRIPLET "wasm32-emscripten" CACHE STRING "")
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "C:/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" CACHE STRING "")

project(satoru_wasm)

set(CMAKE_CXX_STANDARD 17)

# --- Manual Paths ---
set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/wasm32-emscripten")

include_directories(
    "${VCPKG_INSTALLED_DIR}/include"
    "${VCPKG_INSTALLED_DIR}/include/freetype2"
    "src/cpp"
)

link_directories("${VCPKG_INSTALLED_DIR}/lib")

# --- Target Configuration ---

add_executable(main src/cpp/main.cpp)

set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist/wasm"
    OUTPUT_NAME "main"
    SUFFIX ".js"
)

# Link libraries
target_link_libraries(main PRIVATE
    freetype
    litehtml
    gumbo
    z
    png
    brotlidec
    brotlicommon
    bz2
)

target_link_options(main PRIVATE
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sSINGLE_FILE=1"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','getValue','setValue']"
    "-sEXPORTED_FUNCTIONS=['_html_to_svg','_load_font','_clear_fonts','_malloc','_free']"
    "-o${CMAKE_CURRENT_SOURCE_DIR}/dist/wasm/main.js"
    "-sALLOW_MEMORY_GROWTH=1"
)
