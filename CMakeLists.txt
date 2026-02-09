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

include(FetchContent)
# --- Skia Setup ---
FetchContent_Declare(
  skia
  GIT_REPOSITORY https://github.com/google/skia.git
  GIT_TAG        main
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
    src/cpp/renderers/pdf_renderer.cpp
    src/cpp/utils/skia_utils.cpp
    src/cpp/utils/skia_stubs.cpp
)

file(GLOB LITEHTML_SRC "${LITEHTML_DIR}/src/*.cpp")

file(GLOB SKIA_CORE_SRC
    "${skia_SOURCE_DIR}/src/core/*.cpp"
    "${skia_SOURCE_DIR}/src/base/*.cpp"
    "${skia_SOURCE_DIR}/src/utils/*.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkFontHost_FreeType.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkFontHost_FreeType_common.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkOSFile_posix.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkMemory_malloc.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkDiscardableMemory_none.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkFontMgr_custom.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkFontMgr_custom_empty.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkLog_stdio.cpp"
    "${skia_SOURCE_DIR}/src/image/*.cpp"
    "${skia_SOURCE_DIR}/src/shaders/*.cpp"
    "${skia_SOURCE_DIR}/src/shaders/gradients/*.cpp"
    "${skia_SOURCE_DIR}/src/effects/SkDashPathEffect.cpp"
    "${skia_SOURCE_DIR}/src/effects/SkCornerPathEffect.cpp"
    "${skia_SOURCE_DIR}/src/effects/imagefilters/SkBlurImageFilter.cpp"
    "${skia_SOURCE_DIR}/src/svg/*.cpp"
    "${skia_SOURCE_DIR}/src/xml/*.cpp"
    "${skia_SOURCE_DIR}/src/text/*.cpp"
    "${skia_SOURCE_DIR}/modules/skcms/skcms.cc"
    "${skia_SOURCE_DIR}/src/encode/SkEncoder.cpp"
    "${skia_SOURCE_DIR}/src/encode/SkPngEncoderImpl.cpp"
    "${skia_SOURCE_DIR}/src/encode/SkPngEncoderBase.cpp"
    "${skia_SOURCE_DIR}/src/encode/SkJpegEncoderImpl.cpp"
    "${skia_SOURCE_DIR}/src/encode/SkJPEGWriteUtility.cpp"
    "${skia_SOURCE_DIR}/src/encode/SkICC.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkCodec.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkCodecColorProfile.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkCodecImageGenerator.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkImageGenerator_FromEncoded.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkEncodedInfo.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkPngCodec.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkPngCodecBase.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkPngCompositeChunkReader.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkBmpBaseCodec.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkBmpCodec.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkBmpMaskCodec.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkBmpRLECodec.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkBmpStandardCodec.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkIcoCodec.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkParseEncodedOrigin.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkMaskSwizzler.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkSwizzler.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkSampler.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkColorPalette.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkColorTable.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkPixmapUtils.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkHdrMetadata.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkJpegCodec.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkJpegDecoderMgr.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkJpegMetadataDecoderImpl.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkJpegMultiPicture.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkJpegSegmentScan.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkJpegSourceMgr.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkJpegUtility.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkJpegXmp.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkWebpCodec.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkAvifCodec.cpp"
    "${skia_SOURCE_DIR}/src/pdf/*.cpp"
    "${skia_SOURCE_DIR}/src/pathops/*.cpp"
)
list(FILTER SKIA_CORE_SRC EXCLUDE REGEX ".*_win.cpp|.*_mac.cpp|.*_linux.cpp|.*_android.cpp|.*_ios.cpp")
list(FILTER SKIA_CORE_SRC EXCLUDE REGEX ".*SkGetExecutablePath.*|.*SkThread.*|.*SkTime.*|.*SkSemaphore.*|.*SkRuntimeEffect.*|.*SkGPU.*|.*SkDocument_PDF_None.cpp")

# --- Common Settings ---
set(SATORU_INCLUDES
    "src/cpp"
    "src/cpp/api"
    "src/cpp/bridge"
    "src/cpp/core"
    "src/cpp/renderers"
    "src/cpp"
    "src/cpp/utils"
    "${LITEHTML_DIR}/include"
    "${LITEHTML_DIR}/include/litehtml"
    "${LITEHTML_DIR}/src"
    "${SKIA_CONFIG_DIR}"
    "${skia_SOURCE_DIR}"
    "${skia_SOURCE_DIR}/include/core"
    "${skia_SOURCE_DIR}/include/config"
    "${skia_SOURCE_DIR}/include/utils"
    "${skia_SOURCE_DIR}/include/private"
    "${skia_SOURCE_DIR}/include/ports"
    "${skia_SOURCE_DIR}/include/ports"
    "${skia_SOURCE_DIR}/include/effects"
    "${skia_SOURCE_DIR}/include/svg"
    "${skia_SOURCE_DIR}/include/encode"
    "${skia_SOURCE_DIR}/include/codec"
    "${skia_SOURCE_DIR}/include/docs"
    "${skia_SOURCE_DIR}/include/pathops"
    "${skia_SOURCE_DIR}/modules/skcms"
    "${skia_SOURCE_DIR}/src/core"
    "${skia_SOURCE_DIR}/src/base"
    "${skia_SOURCE_DIR}/src/text"
    "${skia_SOURCE_DIR}/src/utils"
    "${skia_SOURCE_DIR}/src/codec"
    "${skia_SOURCE_DIR}/src/encode"
    "${skia_SOURCE_DIR}/src/pathops"
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
    "-sDYNAMIC_EXECUTION=0"
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME='createSatoruModule'"
    "-sERROR_ON_UNDEFINED_SYMBOLS=0"
    "-sWARN_ON_UNDEFINED_SYMBOLS=0"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','getValue','setValue','HEAPU8','stringToUTF8','lengthBytesUTF8']"
    "-sEXPORTED_FUNCTIONS=['_init_engine','_html_to_svg','_html_to_png','_html_to_png_binary','_get_png_size','_html_to_pdf_binary','_get_pdf_size','_get_required_fonts','_collect_resources','_add_resource','_scan_css','_malloc','_free','_load_font','_clear_fonts','_load_image','_clear_images']"
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
