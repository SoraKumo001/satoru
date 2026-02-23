cmake_minimum_required(VERSION 3.14)

# --- Toolchain setup ---
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
set(VCPKG_TARGET_TRIPLET "wasm32-emscripten-wasm-eh" CACHE STRING "")
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/triplets" CACHE STRING "")
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" CACHE STRING "")

project(satoru_wasm)
set(CMAKE_CXX_STANDARD 17)

# --- Enable ccache ---
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache enabled")
endif()

# --- Directories ---
set(LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/libs")
set(LITEHTML_DIR "${LIBS_DIR}/litehtml")
set(SKIA_CONFIG_DIR "${LIBS_DIR}/skia")

include(FetchContent)

# --- Skia ---
FetchContent_Declare(
  skia
  GIT_REPOSITORY https://github.com/google/skia.git
  GIT_TAG        main
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(skia)

# --- libavif ---
set(VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/wasm32-emscripten-wasm-eh")

if(NOT TARGET dav1d::dav1d)
    add_library(dav1d::dav1d STATIC IMPORTED GLOBAL)
    set_target_properties(dav1d::dav1d PROPERTIES
        IMPORTED_LOCATION "${VCPKG_INSTALLED_DIR}/lib/libdav1d.a"
        INTERFACE_INCLUDE_DIRECTORIES "${VCPKG_INSTALLED_DIR}/include"
    )
endif()

find_package(ZLIB REQUIRED)

set(AVIF_CODEC_DAV1D SYSTEM CACHE STRING "" FORCE)
set(AVIF_LIBYUV OFF CACHE BOOL "" FORCE)
set(AVIF_BUILD_APPS OFF CACHE BOOL "" FORCE)
set(AVIF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(AVIF_ENABLE_WASM OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # Force static for libavif

FetchContent_Declare(
  libavif
  GIT_REPOSITORY https://github.com/AOMediaCodec/libavif.git
  GIT_TAG        v1.1.1
)

set(CMAKE_SKIP_INSTALL_RULES ON)
FetchContent_MakeAvailable(libavif)
set(CMAKE_SKIP_INSTALL_RULES OFF)

# --- wuffs ---
set(WUFFS_RELEASE_DIR "${CMAKE_BINARY_DIR}/wuffs")
if(NOT EXISTS "${WUFFS_RELEASE_DIR}/wuffs-v0.3.c")
    file(MAKE_DIRECTORY "${WUFFS_RELEASE_DIR}")
    message(STATUS "Downloading wuffs-v0.3.c...")
    file(DOWNLOAD https://raw.githubusercontent.com/google/wuffs/main/release/c/wuffs-v0.3.c
         "${WUFFS_RELEASE_DIR}/wuffs-v0.3.c")
endif()

# --- Dependencies ---
find_package(Freetype REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(WebP REQUIRED)
find_package(expat CONFIG REQUIRED)
find_package(unofficial-gumbo CONFIG REQUIRED)
find_package(unofficial-brotli CONFIG REQUIRED)
find_package(BZip2 REQUIRED)
find_package(ctre CONFIG REQUIRED)
find_package(utf8proc CONFIG REQUIRED)
find_package(libunibreak CONFIG REQUIRED)
find_package(harfbuzz REQUIRED)

# Workaround for vcpkg harfbuzz path bug
if(TARGET harfbuzz)
    set_target_properties(harfbuzz PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${VCPKG_INSTALLED_DIR}/include/harfbuzz")
endif()

set(SATORU_LIBS
    Freetype::Freetype
    PNG::PNG
    JPEG::JPEG
    WebP::webp
    WebP::webpdecoder
    WebP::webpdemux
    WebP::libwebpmux
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
    ctre::ctre
    utf8proc::utf8proc
    libunibreak::libunibreak
    harfbuzz::harfbuzz
)

    set(COMMON_COMPILE_OPTIONS
        -Oz
        -g0
        -fexceptions
        -msimd128
    )
    set(COMMON_LINK_OPTIONS
        "--bind"
        "-Oz"
        "-g0"
        -fexceptions
        -msimd128
        "-sWASM_BIGINT=1"
        "-sSUPPORT_LONGJMP=emscripten"
        "-sDISABLE_EXCEPTION_CATCHING=0"
        "-sDYNAMIC_EXECUTION=0"
        "-sASSERTIONS=0"
        "-sMODULARIZE=1"
        "-sEXPORT_ES6=1"
        "-sEXPORT_NAME='createSatoruModule'"
        "-sENVIRONMENT=web,worker"
        "-sFILESYSTEM=0"
        # Some Skia symbols (SkSL, RuntimeEffect) are undefined because we exclude their sources
        # to keep the binary size small. They are not used in our current rendering path.
        "-sERROR_ON_UNDEFINED_SYMBOLS=0"
        "-sWARN_ON_UNDEFINED_SYMBOLS=1"
        "-sALLOW_MEMORY_GROWTH=1"
    )

    if(DEFINED ENV{SATORU_EXTRA_LINK_OPTIONS})
        string(REPLACE " " ";" SATORU_EXTRA_LINK_OPTIONS_LIST $ENV{SATORU_EXTRA_LINK_OPTIONS})
        list(APPEND COMMON_LINK_OPTIONS ${SATORU_EXTRA_LINK_OPTIONS_LIST})
        message(STATUS "Loaded extra link options from SATORU_EXTRA_LINK_OPTIONS")
    endif()

# --- Target: litehtml ---
add_library(litehtml STATIC)
file(GLOB LITEHTML_SRC "${LITEHTML_DIR}/src/*.cpp")
target_sources(litehtml PRIVATE ${LITEHTML_SRC})
target_include_directories(litehtml PUBLIC
    "${LITEHTML_DIR}/include"
    "${LITEHTML_DIR}/include/litehtml"
)
target_link_libraries(litehtml PRIVATE unofficial::gumbo::gumbo)
target_compile_options(litehtml PRIVATE ${COMMON_COMPILE_OPTIONS})

# --- Target: skia_lib ---
add_library(skia_lib STATIC)
file(GLOB SKIA_CORE_SRC
    "${skia_SOURCE_DIR}/src/core/*.cpp"
    "${skia_SOURCE_DIR}/src/base/*.cpp"
    "${skia_SOURCE_DIR}/src/utils/*.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkFontHost_FreeType.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkFontHost_FreeType_common.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkOSFile_posix.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkOSFile_stdio.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkTime_Unix.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkMemory_malloc.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkDiscardableMemory_none.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkFontMgr_custom.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkFontMgr_custom_empty.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkLog_stdio.cpp"
    "${skia_SOURCE_DIR}/src/ports/SkGlobalInitialization_default.cpp"
    "${skia_SOURCE_DIR}/src/image/*.cpp"
    "${skia_SOURCE_DIR}/src/shaders/*.cpp"
    "${skia_SOURCE_DIR}/src/shaders/gradients/*.cpp"
    "${skia_SOURCE_DIR}/src/effects/*.cpp"
    "${skia_SOURCE_DIR}/src/effects/imagefilters/*.cpp"
    "${skia_SOURCE_DIR}/src/effects/colorfilters/*.cpp"
    "${skia_SOURCE_DIR}/src/svg/*.cpp"
    "${skia_SOURCE_DIR}/src/xml/*.cpp"
    "${skia_SOURCE_DIR}/src/text/*.cpp"
    "${skia_SOURCE_DIR}/modules/skcms/skcms.cc"
    "${skia_SOURCE_DIR}/modules/skcms/src/skcms_TransformBaseline.cc"
    "${skia_SOURCE_DIR}/src/encode/SkEncoder.cpp"
    "${skia_SOURCE_DIR}/src/encode/SkPngEncoderImpl.cpp"
    "${skia_SOURCE_DIR}/src/encode/SkPngEncoderBase.cpp"
    "${skia_SOURCE_DIR}/src/encode/SkJpegEncoderImpl.cpp"
    "${skia_SOURCE_DIR}/src/encode/SkJPEGWriteUtility.cpp"
    "${skia_SOURCE_DIR}/src/encode/SkWebpEncoderImpl.cpp"
    "${skia_SOURCE_DIR}/src/encode/SkICC.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkCodec.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkCodecColorProfile.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkCodecImageGenerator.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkImageGenerator_FromEncoded.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkEncodedInfo.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkPngCodec.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkPngCodecBase.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkPngCompositeChunkReader.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkExif.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkTiffUtility.cpp"
    "${skia_SOURCE_DIR}/src/codec/SkGainmapInfo.cpp"
    "${skia_SOURCE_DIR}/src/utils/SkOTUtils.cpp"
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
    "${skia_SOURCE_DIR}/src/codec/SkWuffsCodec.cpp"
    "src/cpp/libs/skia/wuffs_implementation.cpp"
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
    "${skia_SOURCE_DIR}/modules/svg/src/*.cpp"
    "${skia_SOURCE_DIR}/modules/skshaper/src/SkShaper.cpp"
    "${skia_SOURCE_DIR}/modules/skshaper/src/SkShaper_primitive.cpp"
    "${skia_SOURCE_DIR}/modules/skshaper/src/SkShaper_harfbuzz.cpp"
    "${skia_SOURCE_DIR}/modules/skshaper/src/SkShaper_skunicode.cpp"
    "${skia_SOURCE_DIR}/modules/skshaper/src/SkShaper_factory.cpp"
    "${skia_SOURCE_DIR}/modules/skunicode/src/SkUnicode.cpp"
    "${skia_SOURCE_DIR}/modules/skunicode/src/SkUnicode_hardcoded.cpp"
)
list(FILTER SKIA_CORE_SRC EXCLUDE REGEX ".*_win.cpp|.*_mac.cpp|.*_linux.cpp|.*_android.cpp|.*_ios.cpp")
list(FILTER SKIA_CORE_SRC EXCLUDE REGEX ".*SkGetExecutablePath.*|.*SkThread.*|.*SkSemaphore.*|.*SkRuntimeEffect.*|.*SkMesh.*|.*SkRuntimeBlender.*|.*SkSL.*|.*SkGPU.*|.*SkGpu.*|.*SkGraphite.*|.*SkDocument_PDF_None.cpp|.*src/gpu/.*|.*src/graphite/.*")

target_sources(skia_lib PRIVATE ${SKIA_CORE_SRC})
target_include_directories(skia_lib PUBLIC
    "${SKIA_CONFIG_DIR}"
    "${skia_SOURCE_DIR}"
    "${WUFFS_RELEASE_DIR}"
    "${skia_SOURCE_DIR}/modules/svg/include"
    "${skia_SOURCE_DIR}/modules/skshaper/include"
    "${skia_SOURCE_DIR}/modules/skunicode/include"
)
target_compile_definitions(skia_lib PUBLIC SK_USER_CONFIG_HEADER="SkUserConfig.h" SK_ENABLE_SVG SK_SHAPER_HARFBUZZ_AVAILABLE SK_SHAPER_UNICODE_AVAILABLE SK_DISABLE_FILESYSTEM SKCMS_PORTABLE)
target_compile_options(skia_lib PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(skia_lib PUBLIC ${SATORU_LIBS})

# --- Target: satoru_core ---
add_library(satoru_core OBJECT)
target_sources(satoru_core PRIVATE
    src/cpp/main.cpp
    src/cpp/api/satoru_api.cpp
    src/cpp/core/container_skia.cpp
    src/cpp/core/text_utils.cpp
    src/cpp/core/text/unicode_service.cpp
    src/cpp/core/text/text_layout.cpp
    src/cpp/core/text/text_renderer.cpp
    src/cpp/core/el_svg.cpp
    src/cpp/core/satoru_context.cpp
    src/cpp/core/resource_manager.cpp
    src/cpp/core/font_manager.cpp
    src/cpp/renderers/svg_renderer.cpp
    src/cpp/renderers/png_renderer.cpp
    src/cpp/renderers/webp_renderer.cpp
    src/cpp/renderers/pdf_renderer.cpp
    src/cpp/utils/skia_utils.cpp
    src/cpp/utils/skia_stubs.cpp
    src/cpp/utils/skunicode_satoru.cpp
)
target_include_directories(satoru_core PRIVATE "src/cpp")
target_link_libraries(satoru_core PUBLIC litehtml skia_lib)
target_compile_options(satoru_core PRIVATE ${COMMON_COMPILE_OPTIONS})
target_compile_definitions(satoru_core PRIVATE SK_USER_CONFIG_HEADER="SkUserConfig.h")

# --- Executables ---
add_executable(satoru $<TARGET_OBJECTS:satoru_core>)
target_link_libraries(satoru PRIVATE litehtml skia_lib)
target_link_options(satoru PRIVATE ${COMMON_LINK_OPTIONS})
set_target_properties(satoru PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packages/satoru/dist"
    OUTPUT_NAME "satoru"
    SUFFIX ".js"
)

add_executable(satoru_single $<TARGET_OBJECTS:satoru_core>)
target_link_libraries(satoru_single PRIVATE litehtml skia_lib)
target_link_options(satoru_single PRIVATE ${COMMON_LINK_OPTIONS} "-sSINGLE_FILE=1" "-sENVIRONMENT=web,worker")
set_target_properties(satoru_single PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packages/satoru/dist"
    OUTPUT_NAME "satoru-single"
    SUFFIX ".js"
)
