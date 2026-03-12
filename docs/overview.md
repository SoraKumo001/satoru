# Project Overview: Satoru

A high-fidelity HTML/CSS to SVG/PNG/PDF converter running in WebAssembly (Emscripten).

- **Core Technologies**: `litehtml` (Layout) + `Skia` (Rendering) + Lightweight custom logging.
- **Structure**: pnpm workspaces monorepo.
  - `packages/satoru`: Core library & TS wrappers. Uses a `core.ts` base class to share logic between `index.ts` (browser) and `node.ts` (Node.js).
  - `packages/visual-test`: Visual regression testing suite.
  - `packages/playground`: Web-based demo application.
  - `packages/cloudflare-ogp`: OGP image generation using Cloudflare Workers.
