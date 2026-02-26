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

### 1.4 Serena Tool Constraints

- **serena\_\_read_file**: Use of `serena__read_file` is strictly prohibited. Use `read_text_file` or other alternative tools instead.
- **replace_content**: Use of `replace_content` is strictly prohibited. Use `edit_file` or other alternative tools instead.
- **find_symbol**: Use of `find_symbol` is strictly prohibited. Use `get_symbols_overview` and `read_text_file` with line numbers or other alternative tools instead.

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
- `src/cpp/bridge`: Unified type definitions and constants shared across modules (`bridge_types.h`, `magic_tags.h`).
- `src/cpp/core`: Layout/Rendering core, resource management, and master CSS.
- `src/cpp/core/text`: Modular text processing stack.
  - `UnicodeService`: Encapsulates Unicode logic (utf8proc, libunibreak, BiDi). Includes **LRU caching** for line break analysis and BiDi levels.
  - `TextLayout`: High-level layout logic. Uses a unified `TextAnalysis` pass and **LRU caches** for shaping and measurement results.
  - `TextRenderer`: Skia-based rendering, decorations, and tagging.
  - `TextBatcher`: Batches consecutive text runs with the same style into a single `SkTextBlob`.
- `src/cpp/renderers`: PNG, SVG, PDF, and WebP specific renderers.
- `src/cpp/utils`: Skia utilities, LRU cache implementation (`lru_cache.h`), and logging macros.
- `src/cpp/libs`: External libraries (`litehtml`, `skia`).

### 3.2 Layout & Rendering Pipelines

- **Unified Layout Pipeline (O(N) Optimization)**: Decoupled `measure()` and `place()` passes.
  - **Caching Mechanism**: Caches `containing_block_context` and measurement results in `render_item` to ensure linear $O(N)$ complexity.
- **W3C Flexbox Engine**: Full-spec implementation with multi-step resolution (base sizing, main/cross axis resolution, and finalization).
- **Advanced CSS Shapes & Filters**:
  - **clip-path**: Full support for basic shapes (`circle`, `ellipse`, `inset`, `polygon`) and `path()` via Skia's path operations.
  - **backdrop-filter**: Implemented using `saveLayer` and Skia image filters (e.g., `blur()`).
  - **Gradients**: Enhanced `radial-gradient` support with focal point and spread methods.
- **SVG Pipeline v2**:
  - **High-Performance Stream Parser**: FSM-based parser for efficient drawing command stream processing.
  - **Structural Metadata**: Preserves CSS classes and IDs in SVG output for post-processing.
  - **Style Recovery**: Uses "Magic Colors" for style encoding and regex-based restoration.
- **Binary Pipelines**: Optimized direct rendering to `SkSurface` (PNG/WebP) or `SkPDFDocument` (PDF).

### 3.3 CSS Engine (litehtml Customizations)

- **Cascade & Specificity**: Strict W3C cascade order.
- **Dynamic Lengths**: Full support for `min()`, `max()`, and `clamp()` functions with recursive evaluation.
- **International Text**:
  - **BiDi**: Full Bidirectional text support via `SkUnicode` and internal BiDi level resolution.
  - **Overflow**: Support for `text-overflow: ellipsis` in line boxes, including inline-level overflow control.
- **Decoration**: Advanced decoration support including `wavy` underlines.

### 3.4 Resource Management & Global Caching

- **Font Caching**: 2-level lookup (URL + FNV-1a hash) with global management of `SkTypeface`.
- **Automatic Resolution**: Integrated Google Fonts provider support and automatic parsing of dynamic CSS font rules.

---

## 4. Development & Verification Workflow

### 4.1 Build Commands

- `pnpm wasm:configure`: Configure CMake with Ninja.
- `pnpm wasm:build`: Compile C++ to Wasm (uses Thin LTO and -O3 in production).
- `pnpm build`: Full monorepo build.

### 4.2 Verification Protocol

1. **Build Check**: Ensure Wasm compilation success.
2. **Visual Audit**: Run `pnpm --filter visual-test convert-assets` and inspect outputs in `temp/`.
3. **Log Analysis**: Use `--verbose` to inspect layout passes and cache hits/misses.

---
