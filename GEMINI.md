# GEMINI.md

## System Context & Operational Guidelines

This document defines the operational rules for Gemini when interacting with the local file system and shell environment.

### 1. Shell Environment: Windows PowerShell

You are operating in a **Windows PowerShell** environment.

- **Constraint:** PowerShell does not support `&&` or `||` for command chaining.
- **Instruction:** Use the semicolon `;` separator for sequential execution.
- ✅ **Correct:** `mkdir test_dir ; cd test_dir`

### 2. File Editing Protocol (mcp-text-editor)

When using `get_text_file_contents` and `edit_text_file_contents`, strictly follow these rules to ensure data integrity and avoid `Hash Mismatch` errors.

#### Rule 1: Synchronized Reading

- Always use `get_text_file_contents` to obtain the current `file_hash` and `range_hash` for the specific lines you intend to edit.

#### Rule 2: Bottom-to-Top Patching

- **CRITICAL:** When applying multiple patches to the same file in a single `edit_text_file_contents` call, you **MUST** order the patches from the bottom of the file to the top (descending line numbers).

#### Rule 3: write_file Tool Rules

- When generating the `content` argument for the `write_file` tool, do not double-escape newline characters (e.g., use `\n`, not `\\n`).
- Always use standard JSON string escape sequences (`\n`) only.
- Ensure that the generated file content contains actual newlines instead of the literal string `\n`.

### 3. Troubleshooting

- If a `Hash Mismatch` error occurs: Re-read the file ranges and retry the edit with fresh hashes. If the error persists or becomes recurrent, rewrite the entire file using `write_file` to ensure consistency.

## Project Context: Satoru

### 1. Overview

**Satoru** is a high-fidelity HTML/CSS to SVG/PNG/PDF converter running in WebAssembly.

- **Monorepo:** Organized as pnpm workspaces.
- **Core:** `litehtml` (Layout) + `Skia` (Rendering).
- **Target:** WASM via Emscripten.

### 2. Build System

- `pnpm wasm:configure`: Configure CMake for WASM build.
- `pnpm wasm:build`: Compile C++ to WASM (Separate: `satoru.js`/`.wasm`, Single: `satoru-single.js`).
- `pnpm build`: Build all package wrappers.

### 3. Implementation Details

- **C++ Directory Structure:**
  - `src/cpp/api`: Emscripten API implementation (`satoru_api.cpp`). High-level logic and state management.
  - `src/cpp/bridge`: Common types used across the project (`bridge_types.h`).
  - `src/cpp/core`: Core rendering logic (`container_skia`), resource management, and master CSS.
  - `src/cpp/renderers`: PNG/SVG/PDF specific rendering implementation.
  - `src/cpp/utils`: Skia-specific utility functions and Base64 helpers.
- **API Layer:** All functionality exported to WASM should be defined in `src/cpp/api/satoru_api.h` and implemented in `satoru_api.cpp`. `main.cpp` serves as the Emscripten entry point and binding definition.
- **Logging Bridge:**
  - Unified logging function `satoru_log(LogLevel level, const char *message)` is used in C++.
  - Bridges to JS via `EM_JS` calling `Module.onLog`.
  - Standard Emscripten `print` and `printErr` are also redirected to the `onLog` interface in the TS wrapper.
- **Global State:** Global instances like `SatoruContext` and `ResourceManager` are maintained in `satoru_api.cpp`. Do not mark them `static` if they need to be accessed via `extern` from other core components.
- **SVG Rendering (2-Pass):**
  1. **Pass 1 (Measurement):** Layout with a dummy container to determine exact content height.
  2. **Pass 2 (Drawing):** Render to `SkSVGCanvas` with the calculated dimensions.
- **PDF Rendering (2-Pass):**
  1. **Pass 1 (Measurement):** Determine content height.
  2. **Pass 2 (Drawing):** Render to `SkPDFDocument`. Requires explicit registration of `jpegDecoder` and `jpegEncoder` in `SkPDF::Metadata` to avoid runtime assertions.
- **Tag-Based Post-Processing (SVG only):**
  - Advanced effects (Shadows, Images, Conics) are \"tagged\" during drawing with unique colors (e.g., `rgb(0,1,index)` for shadows).
  - The resulting SVG string is processed via **Regular Expressions** to replace these tagged elements with real SVG filters or `<image>` tags.
- **Box Shadow Logic:**
  - **Outer Shadow:** Uses `feGaussianBlur` + `feOffset` + `feFlood`.
  - **Inset Shadow:** Uses `feComposite` with `operator=\"out\"` to invert the shape, then `blur/offset`, and finally `operator=\"in\"` to clip the shadow within the original shape.
  - **Alpha Reference:** Tagged elements must maintain a fill (e.g., `fill=\"black\"`) so `SourceAlpha` works for filters, but `feMerge` controls visibility.
- **Font Handling:**
  - **2-Pass Loading:** Missing fonts are detected during layout and requested from the JS host.
  - **Fallback Logic:** The engine iterates through the entire `font-family` list. Even if the first font is available, it may still request subsequent fonts if they are needed for specific glyphs (e.g., Japanese).
  - **Generic Families:** Keywords like `sans-serif` or `serif` trigger a fallback to the first available font defined in `@font-face` if no exact system match is found.
  - **Metadata Inference:** The TS host infers `weight` and `style` from font URLs to generate correct `@font-face` blocks for WASM.
  - **Asset Fonts:** When creating or modifying HTML files in the `assets/` directory that contain text, you MUST include `@font-face` declarations with valid font URLs to ensure correct rendering in the WASM environment.
- **litehtml Types:**
  - Container overrides MUST return `litehtml::pixel_t` (float), not `int`, to match base class signatures.
- **Layout Defaults:**
  - **line-height:** Default value for `normal` is set to `1.2` times the font height in `css_properties.cpp`.
  - **box-sizing:** Defaulted to `border-box` for `button`, `input`, `select`, and `textarea` in `master_css.h`.
- **Flexbox Fixes:**
  - **Measurement Accuracy:** Wrapped `text-align` logic in `line_box::finish` with a check for `size_mode_content` to prevent layout shifts during the intrinsic sizing phase.
  - **Flag Propagation:** Ensured `size_mode_content` is propagated to child rendering contexts in `render_item::calculate_containing_block_context`.

### 4. Skia API & Release Notes

- **SkPath Immutability:** Use `SkPathBuilder` instead of direct `SkPath` modification methods.
- **Radial Gradients:** Circular by default. For **Elliptical** gradients, apply an `SkMatrix` scale transform (e.g., `ry/rx` on Y-axis) to the shader.
- **PathOps:** Required for complex clipping and path operations, especially in PDF/SVG backends.
- **C++ Logs:** Use `satoru_log` for structured logging to JS. Standard `printf` is bridged to JS `console.log` but lacks level information.

### 5. Testing & Validation

- **Visual Regression Suite (`packages/test-visual`)**:
  - **Multi-Format Validation**: Compares **Direct Skia PNG**, **SVG-rendered PNG**, and **PDF output** against reference expectations.
  - **Numerical Diff**: Reports pixel difference percentage for raster paths.
  - **Stabilization**: Uses `flattenAlpha` (blending with white) and white-pixel padding to handle dimension mismatches and transparency flakiness.
- **Output Validation**:
  - **Crucial**: To verify generated output files (PNG/SVG/PDF) for all assets, always use the `convert-assets` command.
  - Command: `pnpm --filter test-visual convert-assets [file.html] [--verbose]`
  - **Partial Conversion**: You can specify one or more filenames to convert only those specific assets.
  - **Verbose Logging**: Use `--verbose` or `-v` to show detailed WASM/rendering logs in the console (hidden by default).
  - Output: Files are generated in `packages/test-visual/temp/`. Use these files to manually inspect the rendering quality and correctness.
- **Performance Optimizations**:
  - **Reference Generation (`tools/generate-reference.ts`)**: Uses Playwright Concurrency (Batch size/Concurrency: 4) to speed up reference image capture.
  - **Multi-threaded Asset Conversion (`tools/convert_assets.ts`)**: Uses `worker_threads` to spawn multiple WASM instances (one per core). Each worker carries its own WASM context for parallel conversion.
- **Tooling Paths**:
  - Reference generation: `packages/test-visual/tools/generate-reference.ts`.
  - Conversion worker: `packages/test-visual/tools/convert_worker.ts`.
  - Assets: `assets/*.html` (at project root).

### 6. GitHub Pages Deployment

To ensure the web-based test environments (e.g., `playground`) work correctly on GitHub Pages (which often hosts projects in subdirectories):

- **Relative Paths in HTML**: Always use relative paths for scripts and links (e.g., `src/main.ts` instead of `/src/main.ts`).
- **Vite Configuration**: Set `base: \"./\"` in `vite.config.ts` to allow the application to be served from any base path.
- **Asset Resolution**: In TypeScript, resolve asset URLs relative to the deployment root (e.g., `assets/file.html`) rather than using local development relative paths (e.g., `../../assets/`).
- **Artifact Management**: Ensure Wasm (`satoru.wasm`) and JS (`satoru.js`) artifacts are copied to the `dist` directory during the build process, as they are typically located in the workspace's shared output directory.