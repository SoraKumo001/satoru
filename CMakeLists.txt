cmake_minimum_required(VERSION 3.10)

project(satoru_wasm)

set(CMAKE_CXX_STANDARD 17)

# Emscripten specific settings
set(WASM_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dist/wasm")

# --- Manual Paths (Since find_package is failing for wasm triplet) ---
set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/wasm32-emscripten")

include_directories(
    "${VCPKG_INSTALLED_DIR}/include"
    "${VCPKG_INSTALLED_DIR}/include/freetype2"
)

link_directories("${VCPKG_INSTALLED_DIR}/lib")

# --- Target Configuration ---

add_executable(main src/cpp/main.cpp)

set_target_properties(main PROPERTIES
    OUTPUT_NAME "main"
    SUFFIX ".js"
)

# Link libraries (Using actual filenames found in lib directory)
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
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    "-sEXPORTED_FUNCTIONS=['_add']"
    "-o${WASM_OUTPUT_DIR}/main.js"
)
