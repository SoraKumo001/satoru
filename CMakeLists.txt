cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
set(VCPKG_TARGET_TRIPLET "wasm32-emscripten" CACHE STRING "")
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "C:/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" CACHE STRING "")

project(satoru_wasm)

# --- Restore SkUserConfig.h if missing ---
set(SK_USER_CONFIG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/skia/include/config/SkUserConfig.h")
if(NOT EXISTS "${SK_USER_CONFIG_PATH}")
    message(STATUS "Restoring missing SkUserConfig.h...")
    file(WRITE "${SK_USER_CONFIG_PATH}"
"#ifndef SkUserConfig_DEFINED\n"
"#define SkUserConfig_DEFINED\n\n"
"// Minimal config for Wasm\n"
"#ifndef SK_SUPPORT_GPU\n"
"#define SK_SUPPORT_GPU 0\n"
"#endif\n\n"
"#ifndef SK_SUPPORT_PDF\n"
"#define SK_SUPPORT_PDF 0\n"
"#endif\n\n"
"#ifndef SK_ENABLE_SVG\n"
"#define SK_ENABLE_SVG\n"
"#endif\n\n"
"#endif\n"
    )
endif()

set(CMAKE_CXX_STANDARD 17)

set(LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/libs")
set(LITEHTML_DIR "${LIBS_DIR}/litehtml")

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
    "external/skia/src/effects/*.cpp"
    "external/skia/src/effects/imagefilters/*.cpp"
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
    -O2
    -Wno-switch
    -Wno-macro-redefined
    -Wno-unused-function
    -fexceptions
    -msimd128
)

target_compile_definitions(main PRIVATE
    SK_RELEASE
    SK_GAMMA_SRGB
    SK_HAS_FREETYPE_SUPPORT
    SK_SUPPORT_PDF=0
    SK_SUPPORT_GPU=0
    SK_ENABLE_SVG=1
    SK_CODEC_DECODES_PNG
    SK_ENCODE_PNG
)

set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/public"
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
    "-O2"
    "-fexceptions"
    "-msimd128"
    "-sSUPPORT_LONGJMP=emscripten"
    "-sDISABLE_EXCEPTION_CATCHING=0"
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=0"
    "-sEXPORT_NAME='createSatoruModule'"
    "-sERROR_ON_UNDEFINED_SYMBOLS=0"
    "-sWARN_ON_UNDEFINED_SYMBOLS=0"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','getValue','setValue','HEAPU8','stringToUTF8','lengthBytesUTF8']"
    "-sEXPORTED_FUNCTIONS=['_init_engine','_html_to_svg','_html_to_png','_malloc','_free','_load_font','_clear_fonts','_load_image','_clear_images']"      
    "-o${CMAKE_CURRENT_SOURCE_DIR}/public/satoru.js"
    "-sALLOW_MEMORY_GROWTH=1"
)
