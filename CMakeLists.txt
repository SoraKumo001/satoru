cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
set(VCPKG_TARGET_TRIPLET "wasm32-emscripten" CACHE STRING "")
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" CACHE STRING "")

project(satoru_wasm)

set(CMAKE_CXX_STANDARD 17)

set(LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/libs")
set(LITEHTML_DIR "${LIBS_DIR}/litehtml")
set(SKIA_CONFIG_DIR "${LIBS_DIR}/skia")

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
    src/cpp/container_skia.cpp
    src/cpp/utils.cpp
    src/cpp/skia_stubs.cpp
    src/cpp/satoru_context.cpp
    src/cpp/svg_renderer.cpp
    src/cpp/png_renderer.cpp
)

# --- Local litehtml Source ---
file(GLOB LITEHTML_SRC "${LITEHTML_DIR}/src/*.cpp")
target_sources(main PRIVATE ${LITEHTML_SRC})

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
    "external/skia/src/shaders/gradients/*.cpp"
    "external/skia/src/effects/SkDashPathEffect.cpp"
    "external/skia/src/effects/SkCornerPathEffect.cpp"
    "external/skia/src/effects/imagefilters/SkBlurImageFilter.cpp"
    "external/skia/src/svg/*.cpp"
    "external/skia/src/xml/*.cpp"
    "external/skia/src/text/*.cpp"
    "external/skia/modules/skcms/skcms.cc"
    "external/skia/src/encode/SkEncoder.cpp"
    "external/skia/src/encode/SkPngEncoderImpl.cpp"
    "external/skia/src/encode/SkPngEncoderBase.cpp"
    "external/skia/src/encode/SkICC.cpp"
    "external/skia/src/codec/SkCodec.cpp"
    "external/skia/src/codec/SkCodecColorProfile.cpp"
    "external/skia/src/codec/SkCodecImageGenerator.cpp"
    "external/skia/src/codec/SkImageGenerator_FromEncoded.cpp"
    "external/skia/src/codec/SkEncodedInfo.cpp"
    "external/skia/src/codec/SkPngCodec.cpp"
    "external/skia/src/codec/SkPngCodecBase.cpp"
    "external/skia/src/codec/SkPngCompositeChunkReader.cpp"
    "external/skia/src/codec/SkParseEncodedOrigin.cpp"
    "external/skia/src/codec/SkMaskSwizzler.cpp"
    "external/skia/src/codec/SkSwizzler.cpp"
    "external/skia/src/codec/SkSampler.cpp"
    "external/skia/src/codec/SkColorPalette.cpp"
    "external/skia/src/codec/SkColorTable.cpp"
    "external/skia/src/codec/SkPixmapUtils.cpp"
    "external/skia/src/codec/SkHdrMetadata.cpp"
)
list(FILTER SKIA_CORE_SRC EXCLUDE REGEX ".*_win.cpp|.*_mac.cpp|.*_linux.cpp|.*_android.cpp|.*_ios.cpp")
list(FILTER SKIA_CORE_SRC EXCLUDE REGEX ".*SkGetExecutablePath.*|.*SkThread.*|.*SkTime.*|.*SkSemaphore.*|.*SkRuntimeEffect.*|.*SkPDF.*|.*SkGPU.*")
target_sources(main PRIVATE ${SKIA_CORE_SRC})

# Includes
target_include_directories(main BEFORE PRIVATE
    "src/cpp"
    "${LITEHTML_DIR}/include"
    "${LITEHTML_DIR}/include/litehtml"
    "${LITEHTML_DIR}/src"
    "${SKIA_CONFIG_DIR}"
)

target_include_directories(main PRIVATE
    "external/skia"
    "external/skia/include/core"
    "external/skia/include/config"
    "external/skia/include/utils"
    "external/skia/include/private"
    "external/skia/include/ports"
    "external/skia/include/effects"
    "external/skia/include/svg"
    "external/skia/include/encode"
    "external/skia/include/codec"
    "external/skia/modules/skcms"
    "external/skia/src/core"
    "external/skia/src/base"
    "external/skia/src/text"
    "external/skia/src/utils"
    "external/skia/src/codec"
    "external/skia/src/encode"
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
    "-sEXPORT_ES6=0"
    "-sEXPORT_NAME='createSatoruModule'"
    "-sERROR_ON_UNDEFINED_SYMBOLS=0"
    "-sWARN_ON_UNDEFINED_SYMBOLS=0"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','getValue','setValue','HEAPU8','stringToUTF8','lengthBytesUTF8']"
    "-sEXPORTED_FUNCTIONS=['_init_engine','_html_to_svg','_html_to_png','_html_to_png_binary','_get_png_size','_malloc','_free','_load_font','_clear_fonts','_load_image','_clear_images','_get_pixel_color']"
    "-o${CMAKE_CURRENT_SOURCE_DIR}/packages/satoru/dist/satoru.js"
    "-sALLOW_MEMORY_GROWTH=1"
)
