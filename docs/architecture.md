# Technical Specifications & Architecture

## 3.1 Directory Structure

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

## 3.2 Layout & Rendering Pipelines

- **Unified Layout Pipeline (O(N) Optimization)**: Decoupled `measure()` and `place()` passes.
  - **Caching Mechanism**: Caches `containing_block_context` and measurement results in `render_item` to ensure linear $O(N)$ complexity.
- **W3C Flexbox Engine**: Full-spec implementation with multi-step resolution (base sizing, main/cross axis resolution, and finalization).
- **W3C Grid Engine**: Implemented using a multi-step resolution pipeline (template tracks, item placement, and alignment).
- **Container Queries**: Full support for `@container` rules with `container-type` (size/inline-size) and `container-name`.
- **Advanced CSS Shapes & Filters**:
  - **clip-path**: Full support for basic shapes (`circle`, `ellipse`, `inset`, `polygon`) and `path()` via Skia's path operations.
  - **backdrop-filter**: Implemented using `saveLayer` and Skia image filters (e.g., `blur()`).
  - **Gradients**: Enhanced `radial-gradient` support with focal point and spread methods.
- **Coordinate Systems & Writing Modes**:
  - **Logical Coordinates**: Used during layout and high-level logic. Includes `inline` (along text flow) and `block` (perpendicular to text flow) axes.
  - **Physical Coordinates**: Skia's native coordinate system (x: left-to-right, y: top-to-bottom).
  - **WritingModeContext**: Core utility (`src/cpp/core/logical_geometry.h`) for bidirectional mapping between logical and physical spaces.
  - **Integration**: `litehtml::render_item` uses logical metrics for box model properties (margins, padding, borders). `TextRenderer` leverages this for writing-mode-independent glyph positioning and decoration drawing.
- **SVG Pipeline v2**:
  - **High-Performance Stream Parser**: FSM-based parser for efficient drawing command stream processing.
  - **Structural Metadata**: Preserves CSS classes and IDs in SVG output for post-processing.
  - **Style Recovery**: Uses "Magic Colors" for style encoding and regex-based restoration.
- **Binary Pipelines**: Optimized direct rendering to `SkSurface` (PNG/WebP) or `SkPDFDocument` (PDF).

## 3.3 CSS Engine (litehtml Customizations)

- **Cascade & Specificity**: Strict W3C cascade order.
- **Dynamic Lengths**: Full support for `calc()`, `min()`, `max()`, and `clamp()` functions with recursive evaluation. Supports `var()` and `env()`.
- **Logical Properties**: Native support for `*-inline-*` and `*-block-*` properties for margins, paddings, borders, and sizes.
- **Modern CSS Functions & Colors**:
  - **Color**: `oklch()`, `oklab()`, `color-mix()`, `light-dark()` and relative color syntax.
  - **Masking**: Support for `mask` and `-webkit-mask` using Skia's composite layers.
- **International Text**:
  - **BiDi**: Full Bidirectional text support via `SkUnicode` and internal BiDi level resolution.
  - **Overflow**: Support for `text-overflow: ellipsis` in line boxes, including inline-level overflow control.
- **Decoration**: Advanced decoration support including `wavy` underlines.

## 3.4 Resource Management & Global Caching

- **Font Caching**: 2-level lookup (URL + FNV-1a hash) with global management of `SkTypeface`.
- **Automatic Resolution**: Integrated Google Fonts provider support and automatic parsing of dynamic CSS font rules.
