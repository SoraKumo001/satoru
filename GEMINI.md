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

- `pnpm build:all`: Standard build command. Use this for normal builds to verify code changes (Builds both WASM + TS wrappers).
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

- **Initialization:**
  - Satoru instances are created using the static `create` method.
  - The base `Satoru` class requires a factory function (Emscripten module factory).
  - Specialized wrappers like `satoru/single` and `satoru/workerd` provide `create` methods that handle environment-specific instantiation automatically.
  - For high-level use, the `render` function (exported from `.`, `./single`, `./workerd`) provides a simplified API that handles instance management internally.

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

- **Grid Layout:**
  - Implemented in `render_item_grid`.
  - Supports `grid-template-columns` and `grid-template-rows` with fixed, percentage, and `fr` units.
  - Supports spanning (`grid-column: span N`) and fixed placements.
  - Advanced automatic placement with collision detection using an occupancy map.
  - Supports `gap`, `column-gap`, and `row-gap`.
  - Full support for container alignment: `justify-content` and `align-content` (`center`, `start`, `end`, `space-between`, `space-around`, `space-evenly`).
  - Items are stretched by default (`align-self: stretch`) to fill the track height.

- **Media Queries (Level 4/5):**
  - Supports modern range syntax (e.g., `(width >= 400px)` or `(200px < height <= 800px)`).
  - Flexible whitespace handling between operators.
  - Support for `calc()` and other functions in media feature values.
  - Added support for discrete features: `hover`, `any-hover`, `pointer`, `any-pointer`, `prefers-reduced-motion`.

- **Container Queries:**
  - Implemented `@container` support with size and name queries.
  - Supports `container-type` (`size`, `inline-size`) and `container-name`.
  - Supports `container` shorthand property.
  - Uses a multi-pass layout approach (2-pass) in `document::render` to resolve styles after container sizes are determined.
  - Reuses media query condition evaluation logic for container conditions.

- **litehtml Types:**
  - Container overrides MUST return `litehtml::pixel_t` (float), not `int`.

- **Layout Defaults:**
  - `line-height`: Calculated as `ascent + descent + leading` for `normal` to better match modern browser behavior. A fallback floor of `font_size * 1.2` is only applied if the calculated metrics are unavailable (<= 0).
  - `box-sizing`: `border-box` for inputs/buttons.

- **CSS Engine (litehtml Customizations):**
  - **Cascade Priority Logic:** The CSS priority logic is encapsulated in the `css_priority` struct (in `style.h`). It implements the correct W3C cascade order: `Important` > `Layer` > `Specificity`. Comparison operators (`operator<`, `operator>=`) are used in `style::add_parsed_property` to ensure predictable overriding behavior.
  - **Inline Styles Priority:** Inline styles (`style` attribute) must be parsed with `litehtml::css::unlayered_id` to ensure they correctly override properties from stylesheets (which also use `unlayered_id`).
  - **Style Recalculation:** `litehtml::html_tag::refresh_styles` must re-parse the `style` attribute to prevent inline styles from being lost during style recalculations (e.g., when pseudo-classes change).

- **International Text Support (UAX #14 & Shaping):**
  - Integrated `utf8proc` for Unicode normalization and grapheme boundary detection.
  - Integrated `libunibreak` for UAX #14 compliant line breaking.
  - Integrated `HarfBuzz` and `SkShaper` for professional text shaping (ligatures, kerning, and complex scripts).
  - `container_skia::split_text` uses range-based checking to map `libunibreak` byte-offsets to character boundaries, ensuring correct wrapping for CJK and mixed-script text.
  - `text_utils::ellipsize_text` is grapheme-safe, preventing broken multi-byte characters when truncating text.
  - Implemented a stubbed but robust `SkUnicode` provider (`SkUnicode_Satoru`) to enable `SkShaper` without heavy ICU dependencies.

### 4. Skia API & Release Notes

- **SkPath Immutability:** Use `SkPathBuilder` instead of direct `SkPath` modification.
- **Radial Gradients:** Circular by default. Use `SkMatrix` scale for Elliptical.
- **PathOps:** Required for complex clipping/paths.
- **C++ Logs:** Use `satoru_log` for structured logging.

### 5. Testing & Validation

- **Visual Regression Suite (`packages/visual-test`)**:
  - Compares Skia PNG, SVG-rendered PNG, and PDF output.
  - Uses `flattenAlpha` and white-pixel padding for stabilization.
  - **Read-Only Protocol:** Tests in `png.test.ts` and `svg.test.ts` do NOT write to `REFERENCE_DIR`. They strictly compare against existing references. If a reference is missing, the test fails with an instruction to run `gen-ref`.
- **Assets Protection:** The original HTML files in the `assets` directory are critical baseline files for testing. Do not modify or overwrite them directly. If testing or experimentation is required, create a new file or use a temporary directory such as `packages/visual-test/temp/`.
- **Output Validation**:
  - Use `pnpm --filter visual-test convert-assets [file.html] [--verbose] [--no-outline]` to verify rendering.
  - `--no-outline`: Disable text outlining in SVG output (uses `<text>` and `@font-face` instead).
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

### 9. Logging & Debugging

- **C++ Logging:** Use `satoru_log(LogLevel level, const char* message)` defined in `bridge/bridge_types.h`.
  - `printf` or `std::cout` may not be visible in all environments (especially workers where `stdout` is not piped).
  - Logs are routed to the JavaScript `onLog` callback via `satoru_api.cpp`.
  - Example: `satoru_log(LogLevel::Info, "Message");`
- **Visual Tests:** Run with `--verbose` to see logs in the console.
  - Example: `pnpm --filter visual-test convert-assets test.html --verbose`
  - Example (No Outline): `pnpm --filter visual-test convert-assets test.html --no-outline`
  - Ensure you pass the correct relative path to `convert-assets`.
