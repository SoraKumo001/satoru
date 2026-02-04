cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
set(VCPKG_TARGET_TRIPLET "wasm32-emscripten" CACHE STRING "")
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "C:/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" CACHE STRING "")

project(satoru_wasm)

set(CMAKE_CXX_STANDARD 17)

set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/wasm32-emscripten")

include_directories(
    "${VCPKG_INSTALLED_DIR}/include"
    "${VCPKG_INSTALLED_DIR}/include/freetype2"
    "src/cpp"
    "external/skia"
    "external/skia/include/core"
    "external/skia/include/config"
    "external/skia/include/utils"
    "external/skia/include/private"
    "external/skia/include/ports"
    "external/skia/include/effects"
    "external/skia/include/svg"
    "external/skia/include/encode"
    "external/skia/modules/skcms"
    "external/skia/src/core"
    "external/skia/src/base"
    "external/skia/src/text"
    "external/skia/src/utils"
)

link_directories("${VCPKG_INSTALLED_DIR}/lib")

# --- Skia Source Selection ---
file(GLOB SKIA_CORE_SRC 
    "external/skia/src/core/*.cpp"
    "external/skia/src/base/*.cpp"
    "external/skia/src/utils/*.cpp"
    "external/skia/src/ports/SkFontHost_FreeType.cpp"
    "external/skia/src/ports/SkFontHost_FreeType_common.cpp"
    "external/skia/src/ports/SkOSFile_posix.cpp"
    "external/skia/src/ports/SkMemory_malloc.cpp"
    "external/skia/src/ports/SkDiscardableMemory_none.cpp"
    "external/skia/src/ports/SkFontMgr_custom.cpp"
    "external/skia/src/ports/SkFontMgr_custom_empty.cpp"
    "external/skia/src/ports/SkDebug_stdio.cpp"
    "external/skia/src/image/*.cpp"
    "external/skia/src/shaders/*.cpp"
    "external/skia/src/effects/*.cpp"
    "external/skia/src/svg/*.cpp"
    "external/skia/src/xml/*.cpp"
    "external/skia/src/text/*.cpp"
    "external/skia/modules/skcms/skcms.cc"
    "external/skia/src/encode/SkPngEncoder.cpp"
)
list(FILTER SKIA_CORE_SRC EXCLUDE REGEX ".*_win.cpp|.*_mac.cpp|.*_linux.cpp|.*_android.cpp|.*_ios.cpp")
list(FILTER SKIA_CORE_SRC EXCLUDE REGEX ".*SkGetExecutablePath.*|.*SkThread.*|.*SkTime.*|.*SkSemaphore.*|.*SkRuntimeEffect.*|.*SkPDF.*|.*SkGPU.*")

add_executable(main src/cpp/main.cpp src/cpp/skia_stubs.cpp ${SKIA_CORE_SRC})

# Compiler flags: Standard exceptions (JS based)
target_compile_options(main PRIVATE 
    -Wno-switch 
    -Wno-macro-redefined 
    -Wno-unused-function 
    -fexceptions
)

target_compile_definitions(main PRIVATE 
    SK_RELEASE 
    SK_GAMMA_SRGB 
    SK_HAS_FREETYPE_SUPPORT 
    SK_SUPPORT_PDF=0 
    SK_SUPPORT_GPU=0 
    SK_ENABLE_SVG=1
)

set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/public"
    OUTPUT_NAME "satoru"
    SUFFIX ".js"
)

# Link libraries
target_link_libraries(main PRIVATE 
    freetype 
    litehtml 
    gumbo 
    z 
    png 
    bz2 
    brotlidec 
    brotlienc 
    brotlicommon 
    expat
)

target_link_options(main PRIVATE
    "-fexceptions"
    "-sSUPPORT_LONGJMP=emscripten"
    "-sDISABLE_EXCEPTION_CATCHING=0"
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=0"
    "-sEXPORT_NAME='createSatoruModule'"
    "-sERROR_ON_UNDEFINED_SYMBOLS=0"
    "-sWARN_ON_UNDEFINED_SYMBOLS=0"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','getValue','setValue','HEAPU8','stringToUTF8','lengthBytesUTF8']"
    "-sEXPORTED_FUNCTIONS=['_init_engine','_html_to_svg','_malloc','_free','_load_font','_clear_fonts']"
    "-o${CMAKE_CURRENT_SOURCE_DIR}/public/satoru.js"
    "-sALLOW_MEMORY_GROWTH=1"
)
