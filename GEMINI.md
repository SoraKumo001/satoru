# GEMINI.md

## System Context & Operational Guidelines

This document defines the operational rules for Gemini when interacting with the local file system and shell environment.

### 1. Shell Environment: Windows PowerShell

You are operating in a **Windows PowerShell** environment.

- **Constraint:** PowerShell does not support `&&` or `||` for command chaining.
- **Instruction:** Use the semicolon `;` separator for sequential execution.
- **Restriction:** Do not write multiple shell commands simultaneously.
- ✅ **Correct:** `mkdir test_dir ; cd test_dir`

### 2. Communication Protocol

- **Progress Reporting:** Always report what you are currently doing before and during each step of your work. Provide concise updates on your analysis, planning, and implementation progress to keep the user informed. **Progress reporting should be conducted in the user's language.**

### 3. Troubleshooting

- **Hash Mismatch Handling:** If a `Hash Mismatch` error occurs, immediately re-read the file using `get_text_file_contents` to synchronize. If the error persists or if the file has significant changes, rewrite the entire file using `write_file` to ensure consistency and resolve the conflict.

### 4. Git Usage

- **No Write Operations:** Do not perform any git write operations (e.g., `git add`, `git commit`, `git push`, `git branch`, `git checkout -b`, `git merge`) unless explicitly and specifically instructed by the user. Read-only operations for context (e.g., `git status`, `git diff`, `git log`) are permitted.

## Project Context: Satoru

### 1. Overview

**Satoru** is a high-fidelity HTML/CSS to SVG/PNG/PDF converter running in WebAssembly.

- **Monorepo:** Organized as pnpm workspaces.
- **Core:** `litehtml` (Layout) + `Skia` (Rendering).
- **Target:** WASM via Emscripten.
- **Packages:**
  - `packages/satoru`: Core library and TS wrappers.
  - `packages/cloudflare-ogp`: OGP image generation using JSX and Cloudflare Workers.
  - `packages/visual-test`: Visual regression testing suite.
  - `packages/playground`: Web-based demo application.

### 2. Build System

- **Multi-Environment Support**: Optimized for local development (Windows/PowerShell) and CI (Ubuntu).
- **Speed Optimizations**:
  - **Ninja**: Uses Ninja as the primary CMake generator for faster parallel builds.
  - **ccache**: Enabled by default to cache compiler objects.
  - **Incremental Builds**: `pnpm wasm:configure` does not delete build directories by default (use `--force` to clean).
- **Directory Structure**:
  - `build-wasm-release/`: Intermediate files for Release builds (`-Oz`, `ThinLTO`).
  - `build-wasm-debug/`: Intermediate files for Debug builds (`-O0`, `-g`).
- **Commands**:
  - `pnpm build:all`: Builds both WASM + TS wrappers.
  - `pnpm wasm:configure[:debug]`: Configures CMake via `scripts/build-wasm.ts`.
  - `pnpm wasm:build[:debug]`: Compiles C++ to WASM using Ninja.
  - `pnpm wasm:docker:build`: Optimized Docker build using BuildKit cache mounts.

### 3. Implementation Details

- **C++ Directory Structure:**
  - `src/cpp/api`: Emscripten API implementation (`satoru_api.cpp`). High-level logic and state management.
  - `src/cpp/bridge`: Common types used across the project (`bridge_types.h`).
  - `src/cpp/core`: Core rendering logic (`container_skia`), resource management, and master CSS.
  - `src/cpp/renderers`: PNG/SVG/PDF specific rendering implementation.
  - `src/cpp/utils`: Skia-specific utility functions and Base64 helpers.

- **API Layer:**
  - All functionality exported to WASM is defined in `src/cpp/api/satoru_api.h` and implemented in `satoru_api.cpp`.
  - `main.cpp` serves as the Emscripten entry point and binding definition.
  - Uses Embind (`--bind`) for C++/JS communication. Explicit exports (`EXPORTED_FUNCTIONS`) are minimal.

- **Size Optimizations**:
  - **GPU Disabled**: `SK_SUPPORT_GPU=0` and `SK_GANESH`/`SK_GRAPHITE` disabled to reduce Wasm size.
  - **Filesystem Disabled**: `-s FILESYSTEM=0` and `SK_DISABLE_FILESYSTEM` enabled.
  - **LTO**: `-flto=thin` used in Release mode.
  - **Oz**: Aggressive size optimization (`-Oz`) used in Release mode.

- **Resource Management:**
  - **Callback-Based Resolution:** WASM notifies JS of resource requests via the `satoru_request_resource_js` bridge.
  - **JS Integration:** The `SatoruModule` interface includes an optional `onRequestResource` callback.
  - **Worker Support:** `createSatoruWorker` provides a multi-threaded proxy using `worker-lib`.

- **Logging Bridge:**
  - Unified logging function `satoru_log(LogLevel level, const char *message)` is used in C++.
  - Bridges to JS via `EM_JS` calling `Module.onLog`.

- **Rendering Pipelines:**
  - **SVG (2-Pass):** Measurement -> Drawing to `SkSVGCanvas` -> Regex Post-Processing.
  - **PDF (2-Pass):** Measurement -> Drawing to `SkPDFDocument`.
  - **PNG/WEBP**: Render to `SkSurface` -> `SkImage` -> Encode.

- **Font Handling:**
  - **No Default Font:** All fonts must be explicitly loaded.
  - **fallback:** Generic keywords trigger fallback to first available `@font-face`.

- **CSS Engine (litehtml Customizations):**
  - **Cascade Priority Logic:** Correct W3C cascade order: `Important` > `Layer` > `Specificity`.
  - **International Text Support**: `HarfBuzz`, `libunibreak`, and `utf8proc` integrated for complex text shaping and line breaking.

### 4. Testing & Validation

- **Visual Regression Suite (`packages/visual-test`)**:
  - Compares Skia outputs against Chromium references.
  - **Read-Only Protocol:** `png.test.ts` and `svg.test.ts` do not modify references.
  - **Output Validation**: `pnpm --filter visual-test convert-assets [file.html]`.

### 5. Code Maintenance

- **Formatting:** Always run `pnpm format` (`clang-format` for C++, Prettier for TS).
- **Diagnostics**: Run visual tests with `--verbose` to see `satoru_log` output in the console.
