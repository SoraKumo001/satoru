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

### 3. File Editing Protocol (mcp-text-editor)

When using `get_text_file_contents` and `edit_text_file_contents`, strictly follow these rules to ensure data integrity and avoid `Hash Mismatch` errors.

#### Rule 1: Synchronized Reading

- Always use `get_text_file_contents` to obtain the current `file_hash` and `range_hash` for the specific lines you intend to edit.

#### Rule 2: Bottom-to-Top Patching

- **CRITICAL:** When applying multiple patches to the same file in a single `edit_text_file_contents` call, you **MUST** order the patches from the bottom of the file to the top (descending line numbers).

#### Rule 3: write_file Tool Rules

- When generating the `content` argument for the `write_file` tool, do not double-escape newline characters (e.g., use `\n`, not `\\n`).
- Always use standard JSON string escape sequences (`\n`) only.
- Ensure that the generated file content contains actual newlines instead of the literal string `\n`.

### 4. Troubleshooting

- **Hash Mismatch Handling:** If a `Hash Mismatch` error occurs, immediately re-read the file using `get_text_file_contents` to synchronize. If the error persists or if the file has significant changes, rewrite the entire file using `write_file` to ensure consistency and resolve the conflict.

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

- `pnpm build:all`: Standard build command (Builds everything including WASM + TS wrappers).
- `pnpm wasm:configure`: Configure CMake for WASM build via `tsx ./scripts/build-wasm.ts`.
- `pnpm wasm:build`: Compile C++ to WASM. Produces `satoru.js`/`.wasm` and `satoru-single.js` in `packages/satoru/dist`.
- `pnpm wasm:docker:build`: Build WASM inside Docker.

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

- **Resource Management:**
  - **Callback-Based Resolution:** WASM notifies JS of resource requests (Fonts, Images, CSS) via the `satoru_request_resource_js` bridge.
  - **JS Integration:** The `SatoruModule` interface includes an optional `onRequestResource` callback.
  - **Worker Support:** `createSatoruWorker` provides a multi-threaded proxy using `worker-lib`, allowing parallel execution of Wasm instances.
  - **Data Flow:** `_collect_resources` (C++) iterates requests -> calls `onRequestResource` (JS) -> JS collects into array -> `Promise.all` fetches data -> `_add_resource` (C++) injects data.
  - **Efficiency:** `api_collect_resources` returns `void` to avoid large string allocations/parsing.

- **Logging Bridge:**
  - Unified logging function `satoru_log(LogLevel level, const char *message)` is used in C++.
  - Bridges to JS via `EM_JS` calling `Module.onLog`.
  - Standard Emscripten `print` and `printErr` are also redirected to the `onLog` interface in the TS wrapper.
  - Logging is configured per-render via `RenderOptions`. If no logger is provided, it defaults to console output.

- **Global State:**
  - Global instances like `SatoruContext` and `ResourceManager` are maintained in `satoru_api.cpp`.
  - Do not mark them `static` if they need to be accessed via `extern` from other core components.

- **Rendering Pipelines:**
  - **SVG (2-Pass):**
    1. **Measurement:** Layout with dummy container for height.
    2. **Drawing:** Render to `SkSVGCanvas`.
    3. **Post-Processing:** Regex replacement of tagged elements (shadows, images) with real SVG filters.
  - **PDF (2-Pass):**
    1. **Measurement:** Determine content height.
    2. **Drawing:** Render to `SkPDFDocument`. Requires explicit JPEG decoder/encoder registration.
  - **PNG:** Render to `SkSurface` -> `SkImage` -> Encode to PNG.

- **Box Shadow Logic:**
  - **Outer Shadow:** `feGaussianBlur` + `feOffset` + `feFlood`.
  - **Inset Shadow:** `feComposite` (out) -> blur/offset -> `feComposite` (in).
  - **Alpha Reference:** Tagged elements use `fill=\"black\"` for `SourceAlpha`, visibility controlled by `feMerge`.

- **Font Handling:**
  - **No Default Font:** Satoru does not have any built-in default fonts. All fonts used in the HTML must be explicitly defined using `@font-face` or loaded via `<link>` tags. If no font is defined, text rendering will fail.
  - **2-Pass Loading:** Layout detects missing fonts -> Requests from JS -> Re-layout.
  - **Fallback:** Iterates through `font-family` list; generic keywords (`sans-serif`) trigger fallback to first available `@font-face`.
  - **Metadata:** JS infers weight/style from URLs for `@font-face` generation.

- **List Markers:**
  - Implemented in `container_skia::draw_list_marker`.
  - Supports `disc` (filled oval), `circle` (stroked oval), `square` (filled rect).
  - Supports `list-style-image` via `SkImage` drawing.

- **litehtml Types:**
  - Container overrides MUST return `litehtml::pixel_t` (float), not `int`.

- **Layout Defaults:**
  - `line-height`: `1.2` times font height for `normal`.
  - `box-sizing`: `border-box` for inputs/buttons.

### 4. Skia API & Release Notes

- **SkPath Immutability:** Use `SkPathBuilder` instead of direct `SkPath` modification.
- **Radial Gradients:** Circular by default. Use `SkMatrix` scale for Elliptical.
- **PathOps:** Required for complex clipping/paths.
- **C++ Logs:** Use `satoru_log` for structured logging.

### 5. Testing & Validation

- **Visual Regression Suite (`packages/visual-test`)**:
  - Compares Skia PNG, SVG-rendered PNG, and PDF output.
  - Uses `flattenAlpha` and white-pixel padding for stabilization.
- **Output Validation**:
  - Use `pnpm --filter visual-test convert-assets [file.html] [--verbose]` to verify rendering.
  - Output generated in `packages/visual-test/temp/`.
- **Tooling Paths**:
  - Reference generation: `packages/visual-test/tools/generate-reference.ts`.
  - Conversion worker: `packages/visual-test/tools/convert_worker.ts`.
  - Assets: `assets/*.html`.

### 6. GitHub Pages Deployment

- **Relative Paths:** Use relative paths in HTML/TS (`./src/main.ts`).
- **Vite Config:** Set `base: \"./\"`.
- **Asset Resolution:** Resolve relative to deployment root.
- **Artifacts:** Copy `satoru.wasm` and `satoru.js` to `dist`.

### 7. Bundling & Workers

- **Rolldown:** Used for bundling web workers in `packages/satoru`.
- **Entry Points:**
  - `index.ts`: Core logic, depends on external `.wasm`.
  - `react.ts`: Utility for converting React elements to HTML strings.
  - `single.ts`: Embedded WASM version (Base64). Default export.
  - `workerd.ts`: Specialized for Cloudflare Workers.
  - `workers.ts`: Multi-threaded worker proxy entry point.
- **Environment Detection:** Workers automatically detect Node.js vs Web environment and load appropriate worker implementation.
- **worker-lib:** Facilitates communication between main thread and workers.

### 8. Code Maintenance

- **Formatting:** Always run `pnpm format` to ensure code style consistency. This project uses `clang-format` for C++ and Prettier/TSX scripts for TypeScript.

