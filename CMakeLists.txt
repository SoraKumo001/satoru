cmake_minimum_required(VERSION 3.14)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
set(VCPKG_TARGET_TRIPLET "wasm32-emscripten-wasm-eh" CACHE STRING "")
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/triplets" CACHE STRING "")
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" CACHE STRING "")

project(satoru_wasm)

set(CMAKE_CXX_STANDARD 17)

set(LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/libs")
set(LITEHTML_DIR "${LIBS_DIR}/litehtml")
set(SKIA_CONFIG_DIR "${LIBS_DIR}/skia")

# --- Skia Clone Setup ---
include(FetchContent)

option(SKIA_UPDATE "Check for Skia updates" OFF)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/core/SkCanvas.h" AND NOT SKIA_UPDATE)
  set(FETCHCONTENT_SOURCE_DIR_SKIA "${CMAKE_CURRENT_SOURCE_DIR}/external/skia")
  message(STATUS "Using existing Skia source, skipping all FetchContent download steps.")
endif()

FetchContent_Declare(
  skia
  GIT_REPOSITORY https://github.com/google/skia.git
  GIT_TAG        main
  SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/external/skia
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(skia)

# Find dependencies
find_package(Freetype REQUIRED)
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(expat CONFIG REQUIRED)
find_package(unofficial-gumbo CONFIG REQUIRED)
find_package(unofficial-brotli CONFIG REQUIRED)
find_package(BZip2 REQUIRED)

add_executable(main
    src/cpp/main.cpp
    src/cpp/core/container_skia.cpp
    src/cpp/core/satoru_context.cpp
    src/cpp/core/resource_manager.cpp
    src/cpp/renderers/svg_renderer.cpp
    src/cpp/renderers/png_renderer.cpp
    src/cpp/utils/utils.cpp
    src/cpp/utils/skia_stubs.cpp
)

# --- Local litehtml Source ---
file(GLOB LITEHTML_SRC "${LITEHTML_DIR}/src/*.cpp")
target_sources(main PRIVATE ${LITEHTML_SRC})

# --- Skia Source Selection ---
file(GLOB SKIA_CORE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/base/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/utils/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/ports/SkFontHost_FreeType.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/ports/SkFontHost_FreeType_common.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/ports/SkOSFile_posix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/ports/SkMemory_malloc.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/ports/SkDiscardableMemory_none.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/ports/SkFontMgr_custom.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/ports/SkFontMgr_custom_empty.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/ports/SkDebug_stdio.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/image/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/shaders/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/shaders/gradients/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/effects/SkDashPathEffect.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/effects/SkCornerPathEffect.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/effects/imagefilters/SkBlurImageFilter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/svg/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/xml/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/text/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/modules/skcms/skcms.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/encode/SkEncoder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/encode/SkPngEncoderImpl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/encode/SkPngEncoderBase.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/encode/SkICC.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkCodec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkCodecColorProfile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkCodecImageGenerator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkImageGenerator_FromEncoded.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkEncodedInfo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkPngCodec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkPngCodecBase.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkPngCompositeChunkReader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkParseEncodedOrigin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkMaskSwizzler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkSwizzler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkSampler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkColorPalette.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkColorTable.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkPixmapUtils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkHdrMetadata.cpp"
)
list(FILTER SKIA_CORE_SRC EXCLUDE REGEX ".*_win.cpp|.*_mac.cpp|.*_linux.cpp|.*_android.cpp|.*_ios.cpp")
list(FILTER SKIA_CORE_SRC EXCLUDE REGEX ".*SkGetExecutablePath.*|.*SkThread.*|.*SkTime.*|.*SkSemaphore.*|.*SkRuntimeEffect.*|.*SkPDF.*|.*SkGPU.*")
target_sources(main PRIVATE ${SKIA_CORE_SRC})

# Includes
target_include_directories(main BEFORE PRIVATE
    "src/cpp"
    "src/cpp/core"
    "src/cpp/renderers"
    "src/cpp/utils"
    "${LITEHTML_DIR}/include"
    "${LITEHTML_DIR}/include/litehtml"
    "${LITEHTML_DIR}/src"
    "${SKIA_CONFIG_DIR}"
)


target_include_directories(main PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/config"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/utils"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/private"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/ports"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/effects"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/encode"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/codec"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/modules/skcms"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/base"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/text"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/utils"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/encode"
)

target_compile_options(main PRIVATE
    -O3
    -fexceptions
    -msimd128
)

target_compile_definitions(main PRIVATE
    SK_USER_CONFIG_HEADER="SkUserConfig.h"
)

set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packages/satoru/dist"
    OUTPUT_NAME "satoru"
    SUFFIX ".js"
)

# Link libraries
target_link_libraries(main PRIVATE
    Freetype::Freetype
    PNG::PNG
    ZLIB::ZLIB
    expat::expat
    unofficial::gumbo::gumbo
    unofficial::brotli::brotlidec
    unofficial::brotli::brotlienc
    unofficial::brotli::brotlicommon
    BZip2::BZip2
)

target_link_options(main PRIVATE
    "-O3"
    "-fexceptions"
    "-msimd128"
    "-sWASM_BIGINT=1"
    "-sSUPPORT_LONGJMP=emscripten"
    "-sDISABLE_EXCEPTION_CATCHING=0"
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME='createSatoruModule'"
    "-sERROR_ON_UNDEFINED_SYMBOLS=0"
    "-sWARN_ON_UNDEFINED_SYMBOLS=0"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','getValue','setValue','HEAPU8','stringToUTF8','lengthBytesUTF8']"
    "-sEXPORTED_FUNCTIONS=['_init_engine','_html_to_svg','_html_to_png','_html_to_png_binary','_get_png_size','_get_required_fonts','_collect_resources','_add_resource','_scan_css','_malloc','_free','_load_font','_clear_fonts','_load_image','_clear_images']"
    "-o${CMAKE_CURRENT_SOURCE_DIR}/packages/satoru/dist/satoru.js"
    "-sALLOW_MEMORY_GROWTH=1"
)
