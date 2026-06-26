# Satoru C++ Unit Tests

Native C++ unit tests using Google Test.

## Prerequisites

- CMake 3.14+
- C++17 compiler (GCC, Clang, MSVC)
- Internet connection (for fetching Google Test via FetchContent)

## Build & Run

```bash
# From project root
cd tests
cmake -B build -G "Visual Studio 17 2022"    # or "Unix Makefiles", "Ninja", etc.
cmake --build build --config Release
ctest --test-dir build --output-on-failure

# Or in one shot (Linux/macOS)
cd tests && cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure
```

## Adding New Tests

1. Create `tests/test_<module>.cpp`
2. Add the source file to `tests/CMakeLists.txt` in the `add_executable(...)` line
3. If the test needs additional headers/libraries, update `target_include_directories` and `target_link_libraries`

## Structure

```
tests/
├── CMakeLists.txt          # Standalone build file (separate from main WASM build)
├── README.md               # This file
└── test_lru_cache.cpp      # LruCache unit tests
```

## Notes

- These tests compile the C++ code **natively** (not via Emscripten/WASM).
- Ideal for fast iteration on pure-logic functions (text processing, caching, etc.).
- Functions that depend on `SatoruContext` or Skia require mocking or integration tests.
