cmake_minimum_required(VERSION 3.14)
project(satoru_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Fetch Google Test ---
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- Include directories ---
# Adjust paths to match your project structure
set(SATORU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(SATORU_CPP_DIR "${SATORU_ROOT}/src/cpp")
set(SATORU_STUBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stubs")

# --- Litehtml sources (for non-inline functions) ---
# NOTE: web_color.cpp excluded - too many CSS parser dependencies
file(GLOB LITEHTML_TEST_SRC "${SATORU_CPP_DIR}/libs/litehtml/src/html.cpp"
                            "${SATORU_CPP_DIR}/libs/litehtml/src/num_cvt.cpp"
                            "${SATORU_CPP_DIR}/libs/litehtml/src/strtod.cpp"
                            "${SATORU_CPP_DIR}/libs/litehtml/src/utf8_strings.cpp"
                            "${SATORU_CPP_DIR}/libs/litehtml/src/url.cpp"
                            "${SATORU_CPP_DIR}/libs/litehtml/src/url_path.cpp"
                            "${SATORU_CPP_DIR}/libs/litehtml/src/codepoint.cpp"
                            "${SATORU_CPP_DIR}/libs/litehtml/src/css_tokenizer.cpp"
                            "${SATORU_CPP_DIR}/libs/litehtml/src/string_id.cpp"
                            "${SATORU_CPP_DIR}/libs/litehtml/src/html_microsyntaxes.cpp")

# --- Test executable ---
add_executable(satoru_tests
  test_lru_cache.cpp
  test_num_cvt.cpp
  test_html_utils.cpp
  test_url_utils.cpp
  test_codepoint.cpp
  test_string_id.cpp
  test_html_microsyntaxes.cpp
  test_web_color.cpp
  test_web_color_stubs.cpp
  test_magic_tags.cpp
  test_logger.cpp
  test_font_manager.cpp
  ${SATORU_CPP_DIR}/utils/logging.cpp
  ${SATORU_CPP_DIR}/core/font_manager.cpp
  stubs/skia_utils_stub.cpp
  stubs/unicode_service_stub.cpp
  ${LITEHTML_TEST_SRC}
)

# Include headers — stubs BEFORE real headers so SkColor.h and litehtml.h stubs take precedence
target_include_directories(satoru_tests PRIVATE
  "${SATORU_STUBS_DIR}"
  "${SATORU_STUBS_DIR}/include"
  "${SATORU_CPP_DIR}"
  "${SATORU_CPP_DIR}/utils"
  "${SATORU_CPP_DIR}/core"
  "${SATORU_CPP_DIR}/core/text"
  "${SATORU_CPP_DIR}/bridge"
  "${SATORU_CPP_DIR}/api"
  "${SATORU_CPP_DIR}/renderers"
  "${SATORU_CPP_DIR}/libs/litehtml/include"
  "${SATORU_CPP_DIR}/libs/litehtml/include/litehtml"
  "${SATORU_CPP_DIR}/libs/litehtml/src"
)

# Link Google Test
target_link_libraries(satoru_tests PRIVATE GTest::gtest_main)

# --- Enable CTest ---
enable_testing()
include(GoogleTest)
gtest_discover_tests(satoru_tests)
