# Project Overview: Satoru

## Purpose
Satoru is a high-fidelity HTML/CSS to SVG/PNG/PDF converter running in WebAssembly (Emscripten). It leverages `litehtml` for layout and `Skia` for rendering.

## Tech Stack
- **Core (C++)**: Emscripten, litehtml (customized), Skia, HarfBuzz, libunibreak, utf8proc.
- **TS Wrapper**: TypeScript, Rolldown (for bundling), Worker-lib.
- **Workspaces**: pnpm workspaces monorepo.

## Codebase Structure
- `src/cpp/api`: Emscripten API implementation.
- `src/cpp/core`: Layout/Rendering core and resource management.
- `src/cpp/core/text`: Modular text processing (Unicode, Layout, Renderer).
- `src/cpp/renderers`: Specific renderers for PNG, SVG, PDF, and WebP.
- `src/cpp/utils`: Utilities and Skia integration.
- `packages/satoru`: Core library and TS wrappers.
- `packages/visual-test`: Visual regression testing suite.
- `packages/playground`: Web-based demo application.
- `packages/cloudflare-ogp`: OGP image generation for Cloudflare Workers.
