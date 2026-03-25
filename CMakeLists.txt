cmake_minimum_required(VERSION 3.14)

project(satoru_wasm)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

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
set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "" FORCE)

# --- Skia ---
FetchContent_Declare(
  skia
  GIT_REPOSITORY https://github.com/google/skia.git
  GIT_TAG        main
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(skia)

# --- zlib ---
FetchContent_Declare(
  zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG        v1.3.1
)
FetchContent_GetProperties(zlib)
if(NOT zlib_POPULATED)
    FetchContent_Populate(zlib)
    add_library(zlib_static STATIC
        ${zlib_SOURCE_DIR}/adler32.c
        ${zlib_SOURCE_DIR}/compress.c
        ${zlib_SOURCE_DIR}/crc32.c
        ${zlib_SOURCE_DIR}/deflate.c
        ${zlib_SOURCE_DIR}/gzclose.c
        ${zlib_SOURCE_DIR}/gzlib.c
        ${zlib_SOURCE_DIR}/gzread.c
        ${zlib_SOURCE_DIR}/gzwrite.c
        ${zlib_SOURCE_DIR}/infback.c
        ${zlib_SOURCE_DIR}/inffast.c
        ${zlib_SOURCE_DIR}/inflate.c
        ${zlib_SOURCE_DIR}/inftrees.c
        ${zlib_SOURCE_DIR}/trees.c
        ${zlib_SOURCE_DIR}/uncompr.c
        ${zlib_SOURCE_DIR}/zutil.c
    )
    # Generate complete enough zconf.h for WASM
    file(WRITE "${zlib_SOURCE_DIR}/zconf.h" "
#ifndef ZCONF_H
#define ZCONF_H
#define STDC
#include <stddef.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define FAR
#define z_const const
#define ZEXPORT
#define ZEXPORTVA
#define ZEXTERN extern
typedef unsigned char  Byte;
typedef unsigned int   uInt;
typedef unsigned long  uLong;
typedef Byte Bytef;
typedef char charf;
typedef int intf;
typedef uInt uIntf;
typedef uLong uLongf;
typedef void const *voidpc;
typedef void   *voidpf;
typedef void       *voidp;
typedef size_t z_size_t;
typedef int64_t z_off64_t;
typedef uint32_t z_crc_t;
typedef int64_t z_off_t;
#ifndef SEEK_SET
#  define SEEK_SET        0
#  define SEEK_CUR        1
#  define SEEK_END        2
#endif
#define MAX_MEM_LEVEL 9
#define MAX_WBITS 15
#endif
")
    target_include_directories(zlib_static PUBLIC ${zlib_SOURCE_DIR})
endif()
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlib_static)
    set(ZLIB_LIBRARY zlib_static CACHE INTERNAL "")
    set(ZLIB_INCLUDE_DIR ${zlib_SOURCE_DIR} CACHE INTERNAL "")
    set(ZLIB_FOUND TRUE CACHE INTERNAL "")
endif()

# --- libpng ---
FetchContent_Declare(
  libpng
  GIT_REPOSITORY https://github.com/pnggroup/libpng.git
  GIT_TAG        v1.6.43
)
FetchContent_GetProperties(libpng)
if(NOT libpng_POPULATED)
    FetchContent_Populate(libpng)
    add_library(png_static STATIC
        ${libpng_SOURCE_DIR}/png.c
        ${libpng_SOURCE_DIR}/pngerror.c
        ${libpng_SOURCE_DIR}/pngget.c
        ${libpng_SOURCE_DIR}/pngmem.c
        ${libpng_SOURCE_DIR}/pngpread.c
        ${libpng_SOURCE_DIR}/pngread.c
        ${libpng_SOURCE_DIR}/pngrio.c
        ${libpng_SOURCE_DIR}/pngrtran.c
        ${libpng_SOURCE_DIR}/pngrutil.c
        ${libpng_SOURCE_DIR}/pngset.c
        ${libpng_SOURCE_DIR}/pngtrans.c
        ${libpng_SOURCE_DIR}/pngwio.c
        ${libpng_SOURCE_DIR}/pngwrite.c
        ${libpng_SOURCE_DIR}/pngwtran.c
        ${libpng_SOURCE_DIR}/pngwutil.c
    )
    target_include_directories(png_static PUBLIC ${libpng_SOURCE_DIR} ${zlib_SOURCE_DIR})
    # Use prebuilt pnglibconf.h
    file(COPY "${libpng_SOURCE_DIR}/scripts/pnglibconf.h.prebuilt" DESTINATION "${libpng_SOURCE_DIR}")
    file(RENAME "${libpng_SOURCE_DIR}/pnglibconf.h.prebuilt" "${libpng_SOURCE_DIR}/pnglibconf.h")
endif()
if(NOT TARGET PNG::PNG)
    add_library(PNG::PNG ALIAS png_static)
endif()

# --- bzip2 ---
FetchContent_Declare(
  bzip2
  URL https://www.sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz
)
FetchContent_GetProperties(bzip2)
if(NOT bzip2_POPULATED)
  FetchContent_Populate(bzip2)
  add_library(bzip2 STATIC
    ${bzip2_SOURCE_DIR}/blocksort.c
    ${bzip2_SOURCE_DIR}/huffman.c
    ${bzip2_SOURCE_DIR}/crctable.c
    ${bzip2_SOURCE_DIR}/randtable.c
    ${bzip2_SOURCE_DIR}/compress.c
    ${bzip2_SOURCE_DIR}/decompress.c
    ${bzip2_SOURCE_DIR}/bzlib.c
  )
  target_include_directories(bzip2 PUBLIC ${bzip2_SOURCE_DIR})
  target_compile_definitions(bzip2 PRIVATE _POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)
endif()
if(NOT TARGET BZip2::BZip2)
    add_library(BZip2::BZip2 ALIAS bzip2)
endif()

# --- brotli ---
FetchContent_Declare(
  brotli
  GIT_REPOSITORY https://github.com/google/brotli.git
  GIT_TAG        v1.1.0
)
set(BROTLI_DISABLE_TESTS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(brotli)
set(BROTLIDEC_FOUND TRUE CACHE INTERNAL "")
set(BROTLIDEC_INCLUDE_DIRS "${brotli_SOURCE_DIR}/c/include" CACHE INTERNAL "")
set(BROTLIDEC_LIBRARIES brotlidec brotlicommon CACHE INTERNAL "")
if(NOT TARGET unofficial::brotli::brotlidec)
    add_library(unofficial::brotli::brotlidec ALIAS brotlidec)
    add_library(unofficial::brotli::brotlienc ALIAS brotlienc)
    add_library(unofficial::brotli::brotlicommon ALIAS brotlicommon)
endif()

# --- freetype ---
FetchContent_Declare(
  freetype
  GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
  GIT_TAG        VER-2-13-3
)
set(FT_WITH_ZLIB ON CACHE BOOL "" FORCE)
set(FT_WITH_PNG ON CACHE BOOL "" FORCE)
set(FT_WITH_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_WITH_BROTLI ON CACHE BOOL "" FORCE)
set(FT_WITH_HARFBUZZ OFF CACHE BOOL "" FORCE) # Avoid circular dependency
FetchContent_MakeAvailable(freetype)
if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()

# --- harfbuzz ---
FetchContent_Declare(
  harfbuzz
  GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
  GIT_TAG        10.4.0
)
set(HB_HAVE_FREETYPE ON CACHE BOOL "" FORCE)
set(HB_BUILD_SUBSET ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(harfbuzz)
if(NOT TARGET harfbuzz::harfbuzz)
    add_library(harfbuzz::harfbuzz ALIAS harfbuzz)
endif()

# --- expat ---
FetchContent_Declare(
  expat
  GIT_REPOSITORY https://github.com/libexpat/libexpat.git
  GIT_TAG        R_2_6_2
  SOURCE_SUBDIR  expat
)
set(EXPAT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(EXPAT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(EXPAT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(EXPAT_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(expat)
if(TARGET expat AND NOT TARGET expat::expat)
    add_library(expat::expat ALIAS expat)
endif()

# --- libjpeg-turbo ---
include(ExternalProject)

# 1. Define paths upfront. Removed the "-static" suffix assumption.
set(LIBJPEG_TURBO_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo")
set(LIBJPEG_TURBO_LIBRARY "${LIBJPEG_TURBO_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}jpeg${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(LIBJPEG_TURBO_INCLUDE_DIRS "${LIBJPEG_TURBO_INSTALL_DIR}/include")

ExternalProject_Add(libjpeg-turbo-project
  URL https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/3.0.3.tar.gz
  PREFIX "${LIBJPEG_TURBO_INSTALL_DIR}"
  INSTALL_DIR "${LIBJPEG_TURBO_INSTALL_DIR}"
  BUILD_BYPRODUCTS "${LIBJPEG_TURBO_LIBRARY}"
  CMAKE_CACHE_ARGS
  -DENABLE_SHARED:BOOL=OFF
  -DENABLE_STATIC:BOOL=ON
  -DWITH_SIMD:BOOL=OFF
  -DCMAKE_BUILD_TYPE:STRING=${buildType}
  -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}
  -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
  -DCMAKE_INSTALL_LIBDIR:PATH=lib # Force install to 'lib' instead of 'lib64'
)

file(MAKE_DIRECTORY ${LIBJPEG_TURBO_INCLUDE_DIRS})

add_library(jpeg-static STATIC IMPORTED)
set_target_properties(jpeg-static PROPERTIES
  IMPORTED_LOCATION ${LIBJPEG_TURBO_LIBRARY}
  INTERFACE_INCLUDE_DIRECTORIES ${LIBJPEG_TURBO_INCLUDE_DIRS})
add_dependencies(jpeg-static libjpeg-turbo-project)

if(NOT TARGET JPEG::JPEG)
    add_library(JPEG::JPEG ALIAS jpeg-static)
endif()

# --- libwebp ---
FetchContent_Declare(
  libwebp
  GIT_REPOSITORY https://github.com/webmproject/libwebp.git
  GIT_TAG        v1.4.0
)
set(WEBP_BUILD_LIBWEBPMUX ON CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPDEMUX ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libwebp)
FetchContent_GetProperties(libwebp) # Ensure libwebp_SOURCE_DIR is set
if(TARGET webp AND NOT TARGET WebP::webp)
    add_library(WebP::webp ALIAS webp)
endif()
if(TARGET webpdecoder AND NOT TARGET WebP::webpdecoder)
    add_library(WebP::webpdecoder ALIAS webpdecoder)
endif()
if(TARGET webpdemux AND NOT TARGET WebP::webpdemux)
    add_library(WebP::webpdemux ALIAS webpdemux)
endif()
if(NOT TARGET WebP::libwebpmux)
    file(GLOB WEBP_MUX_SOURCES "${libwebp_SOURCE_DIR}/src/mux/*.c")
    add_library(webpmux_lib STATIC ${WEBP_MUX_SOURCES})
    target_include_directories(webpmux_lib PUBLIC ${libwebp_SOURCE_DIR}/src ${libwebp_SOURCE_DIR})
    add_library(WebP::libwebpmux ALIAS webpmux_lib)
endif()
if(TARGET sharpyuv AND NOT TARGET WebP::sharpyuv)
    add_library(WebP::sharpyuv ALIAS sharpyuv)
endif()

# --- ctre ---
FetchContent_Declare(
  ctre
  GIT_REPOSITORY https://github.com/hanickadot/compile-time-regular-expressions.git
  GIT_TAG        v3.9.0
)
FetchContent_GetProperties(ctre)
if(NOT ctre_POPULATED)
    FetchContent_Populate(ctre)
    # Patch ctre to fix Clang/Emscripten template issue
    set(CTRE_WRAPPER_HPP "${ctre_SOURCE_DIR}/include/ctre/wrapper.hpp")
    if(EXISTS "${CTRE_WRAPPER_HPP}")
        file(READ "${CTRE_WRAPPER_HPP}" CTRE_WRAPPER_CONTENT)
        string(REPLACE "return Method::template exec();" "return Method::exec();" CTRE_WRAPPER_PATCHED "${CTRE_WRAPPER_CONTENT}")
        file(WRITE "${CTRE_WRAPPER_HPP}" "${CTRE_WRAPPER_PATCHED}")
    endif()
endif()
if(NOT TARGET ctre)
    add_library(ctre INTERFACE)
    target_include_directories(ctre INTERFACE ${ctre_SOURCE_DIR}/include)
endif()
if(NOT TARGET ctre::ctre)
    add_library(ctre::ctre ALIAS ctre)
endif()

# --- utf8proc ---
FetchContent_Declare(
  utf8proc
  GIT_REPOSITORY https://github.com/JuliaStrings/utf8proc.git
  GIT_TAG        v2.9.0
)
set(UTF8PROC_ENABLE_REPLACEMENT ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(utf8proc)
if(NOT TARGET utf8proc::utf8proc)
    add_library(utf8proc::utf8proc ALIAS utf8proc)
endif()

# --- libunibreak ---
FetchContent_Declare(
  libunibreak
  GIT_REPOSITORY https://github.com/adah1972/libunibreak.git
  GIT_TAG        libunibreak_6_1
)
FetchContent_GetProperties(libunibreak)
if(NOT libunibreak_POPULATED)
    FetchContent_Populate(libunibreak)
    add_library(libunibreak STATIC
        ${libunibreak_SOURCE_DIR}/src/unibreakbase.c
        ${libunibreak_SOURCE_DIR}/src/unibreakdef.c
        ${libunibreak_SOURCE_DIR}/src/linebreak.c
        ${libunibreak_SOURCE_DIR}/src/linebreakdata.c
        ${libunibreak_SOURCE_DIR}/src/linebreakdef.c
        ${libunibreak_SOURCE_DIR}/src/wordbreak.c
        ${libunibreak_SOURCE_DIR}/src/wordbreakdata.c
        ${libunibreak_SOURCE_DIR}/src/graphemebreak.c
        ${libunibreak_SOURCE_DIR}/src/graphemebreakdata.c
        ${libunibreak_SOURCE_DIR}/src/emojidef.c
        ${libunibreak_SOURCE_DIR}/src/eastasianwidthdef.c
        ${libunibreak_SOURCE_DIR}/src/eastasianwidthdata.c
    )
    target_include_directories(libunibreak PUBLIC ${libunibreak_SOURCE_DIR}/src)
    file(WRITE "${libunibreak_SOURCE_DIR}/src/config.h" "/* Generic config.h for WASM */\n")
endif()
if(NOT TARGET libunibreak::libunibreak)
    add_library(libunibreak::libunibreak ALIAS libunibreak)
endif()

# --- dav1d ---
FetchContent_Declare(
  dav1d
  GIT_REPOSITORY https://code.videolan.org/videolan/dav1d.git
  GIT_TAG        1.4.2
)
FetchContent_GetProperties(dav1d)
if(NOT dav1d_POPULATED)
    FetchContent_Populate(dav1d)
    
    # Generate bitdepth-specific files
    set(DAV1D_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/dav1d_gen")
    file(MAKE_DIRECTORY "${DAV1D_GEN_DIR}")
    
    set(DAV1D_BIT_TEMPLATES 
        cdef_apply cdef fg_apply filmgrain ipred_prepare ipred itx lf_apply 
        loopfilter looprestoration lr_apply mc recon
    )
    
    set(DAV1D_GEN_SOURCES "")
    foreach(bit 8 16)
        foreach(tmpl ${DAV1D_BIT_TEMPLATES})
            set(gen_file "${DAV1D_GEN_DIR}/${tmpl}_${bit}.c")
            file(WRITE "${gen_file}" "#define BITDEPTH ${bit}\n#include \"${tmpl}_tmpl.c\"\n")
            list(APPEND DAV1D_GEN_SOURCES "${gen_file}")
        endforeach()
    endforeach()

    # Core sources (excluding templates and wrappers)
    file(GLOB DAV1D_CORE_SOURCES "${dav1d_SOURCE_DIR}/src/*.c")
    list(FILTER DAV1D_CORE_SOURCES EXCLUDE REGEX ".*_tmpl.c|.*itx_1d.c")
    
    add_library(dav1d_static STATIC 
        ${DAV1D_CORE_SOURCES}
        ${DAV1D_GEN_SOURCES}
        "${dav1d_SOURCE_DIR}/src/itx_1d.c"
    )
    target_include_directories(dav1d_static PUBLIC 
        "${dav1d_SOURCE_DIR}/include"
        "${dav1d_SOURCE_DIR}/include/dav1d"
        "${dav1d_SOURCE_DIR}"
        "${dav1d_SOURCE_DIR}/src"
        "${DAV1D_GEN_DIR}"
    )
    # Generic config for WASM
    file(WRITE "${dav1d_SOURCE_DIR}/config.h" "
#define ARCH_X86 0
#define ARCH_X86_64 0
#define ARCH_X86_32 0
#define ARCH_ARM 0
#define ARCH_AARCH64 0
#define ARCH_PPC 0
#define ARCH_PPC64LE 0
#define ARCH_LOONGARCH 0
#define ARCH_RISCV 0
#define ARCH_WASM 1
#define HAVE_ASM 0
#define CONFIG_LOG 1
#define CONFIG_16BIT 1
#define CONFIG_8BIT 1
#define CONFIG_8BPC 1
#define CONFIG_16BPC 1
#define HAVE_PTHREAD_H 0
#define HAVE_UNISTD_H 1
#define HAVE_IO_H 0
#define HAVE_DIRENT_H 1
#define HAVE_GETAUXVAL 0
#define HAVE_ELF_AUX_INFO 0
#define HAVE_ASM_HWCAP_H 0
#define TRIM_DSP_FUNCTIONS 0
")
    file(WRITE "${dav1d_SOURCE_DIR}/include/dav1d/version.h" "#define DAV1D_VERSION \"1.4.2\"\n#define DAV1D_API_VERSION_MAJOR 7\n#define DAV1D_API_VERSION_MINOR 0\n#define DAV1D_API_VERSION_PATCH 0\n")
    file(WRITE "${dav1d_SOURCE_DIR}/src/vcs_version.h" "#define DAV1D_VERSION \"1.4.2\"\n")
    target_compile_definitions(dav1d_static PRIVATE HAVE_CONFIG_H=1)
endif()
if(NOT TARGET dav1d::dav1d)
    add_library(dav1d::dav1d ALIAS dav1d_static)
endif()

# --- gumbo ---
FetchContent_Declare(
  gumbo
  GIT_REPOSITORY https://github.com/google/gumbo-parser.git
  GIT_TAG        v0.10.1
)
FetchContent_GetProperties(gumbo)
if(NOT gumbo_POPULATED)
    FetchContent_Populate(gumbo)
    file(GLOB GUMBO_SOURCES "${gumbo_SOURCE_DIR}/src/*.c")
    add_library(gumbo STATIC ${GUMBO_SOURCES})
    target_include_directories(gumbo PUBLIC "${gumbo_SOURCE_DIR}/src")
endif()
if(NOT TARGET unofficial::gumbo::gumbo)
    add_library(unofficial::gumbo::gumbo ALIAS gumbo)
endif()

# --- libavif ---
set(AVIF_CODEC_AOM OFF CACHE BOOL "" FORCE)
set(AVIF_CODEC_AOM_DECODE OFF CACHE BOOL "" FORCE)
set(AVIF_CODEC_AOM_ENCODE OFF CACHE BOOL "" FORCE)
set(AVIF_CODEC_DAV1D SYSTEM CACHE STRING "" FORCE)
set(AVIF_LIBYUV OFF CACHE BOOL "" FORCE)
set(AVIF_BUILD_APPS OFF CACHE BOOL "" FORCE)
set(AVIF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(AVIF_ENABLE_WASM ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  libavif
  GIT_REPOSITORY https://github.com/AOMediaCodec/libavif.git
  GIT_TAG        v1.1.1
)

set(CMAKE_SKIP_INSTALL_RULES ON)

# Save current flags and suppress all warnings for libavif
set(OLD_CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
set(OLD_CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
set(UNIX 0)

FetchContent_MakeAvailable(libavif)

# Restore flags
set(CMAKE_C_FLAGS ${OLD_CMAKE_C_FLAGS})
set(CMAKE_CXX_FLAGS ${OLD_CMAKE_CXX_FLAGS})

set(CMAKE_SKIP_INSTALL_RULES OFF)

# Manually fix libavif missing include directories for aom
if(TARGET avif_obj)
    target_include_directories(avif_obj PRIVATE 
        ${libaom_SOURCE_DIR}
        ${libaom_BINARY_DIR}
        ${libaom_SOURCE_DIR}/aom
    )
endif()

target_link_libraries(avif PRIVATE dav1d::dav1d WebP::sharpyuv)

# --- wuffs ---
set(WUFFS_RELEASE_DIR "${CMAKE_BINARY_DIR}/wuffs")
if(NOT EXISTS "${WUFFS_RELEASE_DIR}/wuffs-v0.3.c")
    file(MAKE_DIRECTORY "${WUFFS_RELEASE_DIR}")
    message(STATUS "Downloading wuffs-v0.3.c...")
    file(DOWNLOAD https://raw.githubusercontent.com/google/wuffs/main/release/c/wuffs-v0.3.c
         "${WUFFS_RELEASE_DIR}/wuffs-v0.3.c")
endif()

set(SATORU_LIBS
    Freetype::Freetype
    png_static
    WebP::webp
    WebP::webpdecoder
    WebP::webpdemux
    WebP::libwebpmux
    WebP::sharpyuv
    avif
    dav1d::dav1d
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
    # qpdf::libqpdf
    jpeg-static
    zlib_static
)

    set(COMMON_COMPILE_OPTIONS
        -Oz
        -g0
        -fexceptions
        -msimd128
        -flto=thin
        -mbulk-memory
    )
    set(COMMON_LINK_OPTIONS
        "--bind"
        "-O3"
        "-g0"
        -fexceptions
        -msimd128
        "-flto=thin"
        "-mbulk-memory"
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
        "-sERROR_ON_UNDEFINED_SYMBOLS=1"
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
    "${skia_SOURCE_DIR}/modules/skresources/src/*.cpp"
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
    "${skia_SOURCE_DIR}/modules/skresources/include"
    "${skia_SOURCE_DIR}/modules/skshaper/include"
    "${skia_SOURCE_DIR}/modules/skunicode/include"
)
target_compile_definitions(skia_lib PUBLIC SK_USER_CONFIG_HEADER="SkUserConfig.h")
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
    src/cpp/core/text/text_decoration_renderer.cpp
    src/cpp/core/text/tagging_context.cpp
    src/cpp/core/text/text_geometry.cpp
    src/cpp/core/litehtml_extensions.cpp
    src/cpp/core/el_svg.cpp
    src/cpp/core/satoru_context.cpp
    src/cpp/core/resource_manager.cpp
    src/cpp/core/font_manager.cpp
    src/cpp/renderers/svg_renderer.cpp
    src/cpp/renderers/png_renderer.cpp
    src/cpp/renderers/webp_renderer.cpp
    src/cpp/renderers/pdf_renderer.cpp
    src/cpp/utils/pdf_merger.cpp
    src/cpp/utils/skia_utils.cpp
    src/cpp/utils/skia_stubs.cpp
    src/cpp/utils/skunicode_satoru.cpp
    src/cpp/utils/image_decoder.cpp
)
target_include_directories(satoru_core PRIVATE
    "src/cpp"
    "${LITEHTML_DIR}/include"
    "${LITEHTML_DIR}/include/litehtml"
    "${LITEHTML_DIR}/src"
)
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
