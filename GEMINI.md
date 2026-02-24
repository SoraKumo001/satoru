# GEMINI.md

## 1. Operational Guidelines (Agent Specific)

This section defines the core rules for an agent's behavior within this project.

### 1.1 Shell Environment: Windows PowerShell
- **Command Chaining**: Use `;` for sequential execution (do NOT use `&&` or `||`).
- **Constraint**: Do not execute multiple shell commands in parallel.

### 1.2 Communication & Troubleshooting
- **Progress Reporting**: Always report your current status in the user's language concisely before and during each step.
- **Hash Mismatch**: If encountered, immediately re-read the file with `get_text_file_contents` to synchronize. If the issue persists, overwrite the entire file using `write_file`.

### 1.3 Git Usage
- **No Write Operations**: Do not perform `git add`, `commit`, `push`, etc., unless explicitly and specifically instructed by the user.
- **Read-Only Access**: `status`, `diff`, and `log` are permitted and recommended for gathering context.

---

## 2. Project Overview: Satoru

A high-fidelity HTML/CSS to SVG/PNG/PDF converter running in WebAssembly (Emscripten).

- **Core Technologies**: `litehtml` (Layout) + `Skia` (Rendering) + Lightweight custom logging.
- **Structure**: pnpm workspaces monorepo.
  - `packages/satoru`: Core library & TS wrappers. Uses a `core.ts` base class to share logic between `index.ts` (browser) and `node.ts` (Node.js).
  - `packages/visual-test`: Visual regression testing suite.
  - `packages/playground`: Web-based demo application.
  - `packages/cloudflare-ogp`: OGP image generation using Cloudflare Workers.

---

## 3. Technical Specifications & Architecture

### 3.1 Directory Structure
- `src/cpp/api`: Emscripten API implementation (`satoru_api.cpp`).
- `src/cpp/core`: Layout/Rendering core, resource management, and master CSS.
- `src/cpp/core/text`: Modular text processing stack.
  - `UnicodeService`: Encapsulates Unicode logic (utf8proc, libunibreak, BiDi). Now includes caching for `libunibreak` analysis results.
  - `TextLayout`: High-level layout logic (measurement, ellipsizing, splitting, shaping cache).
  - `TextRenderer`: Skia-based rendering, decorations, and tagging.
  - `TextBatcher`: Batches consecutive text runs with the same style into a single `SkTextBlob` to minimize draw calls.
- `src/cpp/renderers`: PNG, SVG, PDF, and WebP specific renderers.
- `src/cpp/utils`: Skia utilities, Base64 helpers, SkUnicode implementation, and logging macros (`logging.h`).
- `src/cpp/libs`: External libraries (`litehtml`, `skia`).

### 3.2 Layout & Rendering Pipelines

- **Unified Layout Pipeline (O(N) Optimization)**: Decoupled `measure()` (size calculation) and `place()` (positioning) methods in `render_item`.
  - **Core Pass Separation**: The layout process is split into two distinct, linear traversals:
    1. **Measure Pass**: Calculates `m_pos.width/height` using `BoxConstraints` (via `size_mode_measure`).
    2. **Place Pass**: Determines `m_pos.x/y` based on measured sizes and container logic.
  - **Caching Mechanism**: Implemented caching for `containing_block_context` and measurement results in `render_item`. This prevents redundant calculations when `measure` is called multiple times (e.g., during the transition from measure to place pass or during table/flex resolution).
  - **Complexity**: Strictly guaranteed linear $O(N)$ by preventing redundant recursive layout calls and utilizing cached states.
- **Flexbox Algorithm (W3C Compliant)**: Fully refactored to follow the multi-step W3C Flexbox resolution process:
  1. **Base Sizing**: Determine `flex_base_size` and `hypothetical_main_size` for each item.
  2. **Main Axis Resolution**: Resolve flexible lengths (`flex-grow`/`flex-shrink`) using a re-distribution loop with `min`/`max` constraints.
  3. **Cross Axis Resolution**: Determine line `cross_size` based on the largest item in the line.
  4. **Finalization**: Re-measure items for `stretch` and `align-self` before final positioning.
- **Lightweight Anonymous Boxes**: Introduced `el_anonymous` class to replace heavy `html_tag` for internal wrapper boxes (Flex text wrapping, Table parts, etc.).
  - **Optimization**: Reduced memory overhead and simplified style resolution for internal boxes.
  - **Unified Property Interface**: Refactored `css_properties` to work with the base `element` class via a virtual property access interface.
- **SVG Pipeline**: Measurement -> Drawing to `SkSVGCanvas` -> Regex Post-Processing.
  - **Style Recovery**: Styles (color, weight, shadow) are encoded into "Magic Colors" during drawing and restored during regex post-processing.
  - **Glyph Reuse**: Common glyph shapes are stored in `<defs>` and referenced via `<use>` to minimize output size.
- **Binary Pipelines (PNG/WebP/PDF)**: Direct rendering to `SkSurface` or `SkPDFDocument`.

### 3.3 CSS Engine (litehtml Customizations)
- **Cascade Priority**: Correct W3C order (`Important` > `Layer` > `Specificity`).
- **International Text**: Integrated `HarfBuzz`, `libunibreak`, and `utf8proc` for complex shaping and line breaking.
- **Decoration**: Supports `wavy` style for underlines using `SkPathBuilder` and `quadTo` curves.

### 3.4 Logging Infrastructure
- **Integration**: Custom lightweight logging bridging to JS.
- **Macros**: Use `SATORU_LOG_INFO`, `SATORU_LOG_ERROR`, etc., from `utils/logging.h` for formatted output (using standard `vsnprintf`).
- **Control**: Log levels are controllable from JS via `Satoru.render({ logLevel: ... })` or `api_set_log_level`.

### 3.5 Resource Management & Global Caching
- **Font Caching (Global)**: `SkTypeface` and `SkFontMgr` are managed globally to minimize instantiation overhead across `SatoruInstance` objects.
  - **Strategy**: 2-level lookup using URL and data hash (FNV-1a).
  - **Variable Fonts**: Cloned `SkTypeface` instances (e.g., specific weights) are also cached globally.
- **Automatic Font Resolution**: If a font is missing, the engine checks a `fontMap` (provided by JS). If matched, it either fetches a direct URL or constructs a `provider:google-fonts` request.
- **Dynamic CSS Support**: `ResourceManager` can detect if a font resource is actually a CSS file (e.g., from Google Fonts API). It automatically parses the CSS, registers `@font-face` rules, and handles font name aliasing so that requested names (like `serif`) correctly map to fetched families (like `Noto Serif JP`).
- **Resolution**: `render` options support `resolveResource` callback.
- **Default Resolver**: The JS wrapper provides a default resource resolver that handles local file system (Node.js) and HTTP `fetch`.
- **Interception**: Users can intercept resource requests and fallback to the default resolver via the second argument of the callback.

### 3.6 litehtml Customizations & Modifications
`litehtml` has been significantly refactored to support high-fidelity rendering and modern CSS features.

- **From Recursive to Linear Layout**: Original `litehtml` layout logic was heavily recursive, often leading to exponential complexity $O(2^n)$ in complex nested structures. This was replaced by a **2-pass Unified Layout Pipeline** ($O(N)$):
  - **Measure Pass**: Uses `BoxConstraints` to calculate sizes.
  - **Place Pass**: Determines positions based on pre-calculated sizes.
- **W3C Flexbox Engine**: The original limited flexbox implementation was completely removed and replaced with a full-spec W3C algorithm. It now correctly handles:
  - Flex base size and hypothetical main size resolution.
  - Multi-line wrapping and cross-axis alignment.
  - Flex grow/shrink redistribution loops with min/max constraints.
- **Element Hierarchy & Lightweight Boxes**: 
  - Introduced `el_anonymous` to replace `html_tag` for engine-generated boxes (e.g., table parts, flex wrappers). This significantly reduces memory overhead.
  - Decoupled `css_properties` from `html_tag`, allowing any `element` subclass to access resolved styles via a virtual interface.
- **Strict CSS Cascade**: Fixed specificity-only sorting in original `litehtml`. The engine now respects the full W3C cascade order: `User Agent` < `User` < `Author` < `Author !important` < `User !important` < `User Agent !important`. (Includes initial support for `@layer`).
- **Text & Unicode Integration**: Deeply integrated `Skia`, `HarfBuzz`, and `libunibreak` directly into the layout core, replacing the generic font interface with a high-performance text stack capable of BiDi, complex shaping, and advanced line-breaking.

---

## 4. Development & Verification Workflow

### 4.1 Build Commands
- `pnpm wasm:configure`: Configure CMake with Ninja generator.
- `pnpm wasm:build`: Compile C++ to Wasm.
- `pnpm build`: Full build of both WASM and TS wrappers.

### 4.2 Verification Protocol (For Features & Fixes)
1.  **Build Check**: Verify `pnpm wasm:build` completes without errors.
2.  **Debug Logs**: Run `convert-assets --verbose` and inspect `satoru_log` outputs for internal state.
3.  **SVG Inspection**: For rendering changes, check the generated SVG source for correct tag structure and attributes.
4.  **Visual Audit**: Manually inspect generated images in `packages/visual-test/temp`.

### 4.3 Maintenance
- **Formatting**: Always run `pnpm format` (`clang-format` for C++, `Prettier` for TS).
- **Asset Conversion**: `pnpm --filter visual-test convert-assets [file.html]`
  - Flags: `--outline` (Force SVG paths), `--no-outline` (Disable paths), `--verbose` (Enable logs).

---

## 5. Roadmap & Progress

- [x] **Phase 1: BoxConstraints Introduction** (Unified sizing logic).
- [x] **Phase 2: Decoupled Layout Pipeline** (Separated Measure/Place passes, $O(N)$ optimization).
- [x] **Phase 3: W3C Flexbox Compliance** (Multi-step resolution algorithm).
- [x] **Phase 4: Lightweight Anonymous Boxes** (`el_anonymous` introduction).
- [x] **Phase 5: Centering Bug Fixes** (Preserved `size_mode_exact` flags in `calculate_containing_block_context`).
- [x] **Phase 6: Visual Validation** (Verified all core assets and shadow clipping fixes).
- [x] **Phase 7: Pipeline Optimization & Caching** (Eliminated redundant `render()` calls and implemented layout state caching).

