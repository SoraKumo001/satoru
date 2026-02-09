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

# --- libavif Setup ---
set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build-wasm/vcpkg_installed/wasm32-emscripten-wasm-eh")

# dav1d manual target
if(NOT TARGET dav1d::dav1d)
    add_library(dav1d::dav1d STATIC IMPORTED GLOBAL)
    set_target_properties(dav1d::dav1d PROPERTIES
        IMPORTED_LOCATION "${VCPKG_INSTALLED_DIR}/lib/libdav1d.a"
        INTERFACE_INCLUDE_DIRECTORIES "${VCPKG_INSTALLED_DIR}/include"
    )
endif()

# Find ZLIB first so we have the variables for libavif
find_package(ZLIB REQUIRED)

# libavif options
set(AVIF_CODEC_DAV1D SYSTEM CACHE STRING "" FORCE)
set(AVIF_LIBYUV OFF CACHE BOOL "" FORCE)
set(AVIF_BUILD_APPS OFF CACHE BOOL "" FORCE)
set(AVIF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(AVIF_ENABLE_WASM OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  libavif
  GIT_REPOSITORY https://github.com/AOMediaCodec/libavif.git
  GIT_TAG        v1.1.1
)

# Avoid installation rules for libavif to prevent export errors in subproject build
set(CMAKE_SKIP_INSTALL_RULES ON)
FetchContent_MakeAvailable(libavif)
set(CMAKE_SKIP_INSTALL_RULES OFF)

# Find dependencies
find_package(Freetype REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(WebP REQUIRED)
find_package(expat CONFIG REQUIRED)
find_package(unofficial-gumbo CONFIG REQUIRED)
find_package(unofficial-brotli CONFIG REQUIRED)
find_package(BZip2 REQUIRED)

# --- Sources ---
set(SATORU_SOURCES
    src/cpp/main.cpp
    src/cpp/api/satoru_api.cpp
    src/cpp/core/container_skia.cpp
    src/cpp/core/satoru_context.cpp
    src/cpp/core/resource_manager.cpp
    src/cpp/renderers/svg_renderer.cpp
    src/cpp/renderers/png_renderer.cpp
    src/cpp/utils/skia_utils.cpp
    src/cpp/utils/skia_stubs.cpp
)

file(GLOB LITEHTML_SRC "${LITEHTML_DIR}/src/*.cpp")

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
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkBmpBaseCodec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkBmpCodec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkBmpMaskCodec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkBmpRLECodec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkBmpStandardCodec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkIcoCodec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkParseEncodedOrigin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkMaskSwizzler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkSwizzler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkSampler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkColorPalette.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkColorTable.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkPixmapUtils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkHdrMetadata.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkJpegCodec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkJpegDecoderMgr.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkJpegMetadataDecoderImpl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkJpegMultiPicture.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkJpegSegmentScan.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkJpegSourceMgr.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkJpegUtility.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkJpegXmp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkWebpCodec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/src/codec/SkAvifCodec.cpp"
)
list(FILTER SKIA_CORE_SRC EXCLUDE REGEX ".*_win.cpp|.*_mac.cpp|.*_linux.cpp|.*_android.cpp|.*_ios.cpp")
list(FILTER SKIA_CORE_SRC EXCLUDE REGEX ".*SkGetExecutablePath.*|.*SkThread.*|.*SkTime.*|.*SkSemaphore.*|.*SkRuntimeEffect.*|.*SkPDF.*|.*SkGPU.*")

# --- Common Settings ---
set(SATORU_INCLUDES
    "src/cpp"
    "src/cpp/api"
    "src/cpp/bridge"
    "src/cpp/core"
    "src/cpp/renderers"
    "src/cpp/utils"
    "${LITEHTML_DIR}/include"
    "${LITEHTML_DIR}/include/litehtml"
    "${LITEHTML_DIR}/src"
    "${SKIA_CONFIG_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/config"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/utils"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/private"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/ports"
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

set(SATORU_COMPILE_OPTIONS
    -O3
    -fexceptions
    -msimd128
)

set(SATORU_DEFINITIONS
    SK_USER_CONFIG_HEADER="SkUserConfig.h"
)

set(SATORU_LIBS
    Freetype::Freetype
    PNG::PNG
    JPEG::JPEG
    WebP::webp
    WebP::webpdecoder
    WebP::webpdemux
    WebP::sharpyuv
    avif
    dav1d::dav1d
    ZLIB::ZLIB
    expat::expat
    unofficial::gumbo::gumbo
    unofficial::brotli::brotlidec
    unofficial::brotli::brotlienc
    unofficial::brotli::brotlicommon
    BZip2::BZip2
)

set(COMMON_LINK_OPTIONS
    "--bind"
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
    "-sALLOW_MEMORY_GROWTH=1"
)

# --- Shared Object Library (Compiled once) ---
add_library(satoru_obj OBJECT ${SATORU_SOURCES} ${LITEHTML_SRC} ${SKIA_CORE_SRC})
target_include_directories(satoru_obj BEFORE PRIVATE ${SATORU_INCLUDES})
target_compile_options(satoru_obj PRIVATE ${SATORU_COMPILE_OPTIONS})
target_compile_definitions(satoru_obj PRIVATE ${SATORU_DEFINITIONS})
# Linking libraries to an object library handles usage requirements (includes)
target_link_libraries(satoru_obj PRIVATE ${SATORU_LIBS})

# --- Target: satoru (Separate JS and WASM) ---
add_executable(satoru $<TARGET_OBJECTS:satoru_obj>)
target_link_libraries(satoru PRIVATE ${SATORU_LIBS})
target_link_options(satoru PRIVATE ${COMMON_LINK_OPTIONS})

set_target_properties(satoru PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packages/satoru/dist"
    OUTPUT_NAME "satoru"
    SUFFIX ".js"
)

# --- Target: satoru_single (WASM embedded in JS) ---
add_executable(satoru_single $<TARGET_OBJECTS:satoru_obj>)
target_link_libraries(satoru_single PRIVATE ${SATORU_LIBS})
target_link_options(satoru_single PRIVATE ${COMMON_LINK_OPTIONS} "-sSINGLE_FILE=1")

set_target_properties(satoru_single PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packages/satoru/dist"
    OUTPUT_NAME "satoru-single"
    SUFFIX ".js"
)
