---
sidebar_position: 2
title: Architecture
---

# Technical Specifications and Architecture

## 3.1 Directory Structure

- `src/cpp/api`: Emscripten API implementation (`satoru_api.cpp`).
- `src/cpp/bridge`: Shared type definitions and constants (`bridge_types.h`, `magic_tags.h`).
- `src/cpp/core`: Layout/rendering core, resource management, and master CSS.
- `src/cpp/core/text`: Modular text processing stack.
  - `UnicodeService`: Encapsulates Unicode logic (utf8proc, libunibreak, BiDi) and uses **LRU caches** for line-break analysis and BiDi levels.
  - `TextLayout`: High-level layout logic with a unified `TextAnalysis` pass and **LRU caches** for shaping and measurement results.
  - `TextRenderer`: Skia-based rendering, decorations, and tagging.
  - `TextBatcher`: Batches consecutive text runs with the same style into a single `SkTextBlob`.
- `src/cpp/renderers`: PNG, SVG, PDF, and WebP renderers.
- `src/cpp/utils`: Skia utilities, LRU cache implementation (`lru_cache.h`), and logging macros.
- `src/cpp/libs`: External libraries (`litehtml`, `skia`).

## 3.2 Layout and Rendering Pipelines

- **Unified layout pipeline (O(N) optimization)**: Separates the `measure()` and `place()` passes.
  - **Caching mechanism**: Caches `containing_block_context` and measurement results in `render_item` to preserve linear $O(N)$ complexity.
- **W3C Flexbox engine**: Multi-step resolution for base sizing, main/cross axis resolution, and finalization.
- **W3C Grid engine**: Multi-step pipeline for template tracks, item placement, and alignment.
- **Container queries**: Supports `@container` rules with `container-type` (size/inline-size) and `container-name`.
- **Advanced CSS shapes and filters**:
  - **clip-path**: Supports basic shapes (`circle`, `ellipse`, `inset`, `polygon`) and `path()` through Skia path operations.
  - **backdrop-filter**: Implemented with `saveLayer` and Skia image filters such as `blur()`.
  - **Gradients**: Enhanced `radial-gradient` support with focal points and spread methods.
- **Coordinate systems and writing modes**:
  - **Logical coordinates**: Used by layout and high-level logic. Includes `inline` and `block` axes.
  - **Physical coordinates**: Skia's native coordinate system (x left-to-right, y top-to-bottom).
  - **WritingModeContext**: Core utility (`src/cpp/core/logical_geometry.h`) for mapping between logical and physical spaces.
  - **Integration**: `litehtml::render_item` uses logical metrics for box model properties. `TextRenderer` uses them for writing-mode-independent glyph positioning and decoration drawing.
- **SVG Pipeline v2**:
  - **High-performance stream parser**: FSM-based parser for efficient drawing command stream processing.
  - **Structural metadata**: Preserves CSS classes and IDs in SVG output for post-processing.
  - **Style recovery**: Encodes styles with "Magic Colors" and restores them with regex-based processing.
- **Binary pipelines**: Optimized direct rendering to `SkSurface` (PNG/WebP) or `SkPDFDocument` (PDF).

## 3.3 CSS Engine (litehtml Customizations)

- **Cascade and specificity**: Follows the W3C cascade order.
- **Dynamic lengths**: Supports `calc()`, `min()`, `max()`, and `clamp()` with recursive evaluation. Also supports `var()` and `env()`.
- **Logical properties**: Native support for `*-inline-*` and `*-block-*` margin, padding, border, and size properties.
- **Modern CSS functions and colors**:
  - **Color**: `oklch()`, `oklab()`, `color-mix()`, `light-dark()`, and relative color syntax.
  - **Masking**: Supports `mask` and `-webkit-mask` through Skia composite layers.
- **International text**:
  - **BiDi**: Bidirectional text support through `SkUnicode` and internal BiDi level resolution.
  - **Overflow**: Supports `text-overflow: ellipsis` in line boxes, including inline-level overflow control.
- **Decoration**: Advanced text decoration support including `wavy` underlines.

## 3.4 Resource Management and Global Caching

- **Font caching**: Two-level lookup by URL and FNV-1a hash, with global `SkTypeface` management.
- **Automatic resolution**: Integrated Google Fonts provider support and automatic parsing of dynamic CSS font rules.
