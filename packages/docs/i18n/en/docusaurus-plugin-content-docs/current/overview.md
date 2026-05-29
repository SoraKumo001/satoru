---
sidebar_position: 1
title: Overview
---

# Project Overview: Satoru

Satoru is a high-fidelity HTML/CSS to SVG/PNG/PDF converter running on WebAssembly (Emscripten).

## Quick Start

```bash
pnpm add satoru-render
```

```typescript
import { render } from "satoru-render/single";

const png = await render({
  value: "<h1>Hello Satoru</h1>",
  width: 1200,
  height: 630,
  format: "png",
});
```

Use `satoru-render/single` for Node.js, `satoru-render/workers` for high-throughput rendering, and `satoru-render/workerd` for Cloudflare Workers.

## Project Structure

- **Core technologies**: `litehtml` for layout, `Skia` for rendering, and lightweight custom logging.
- **Repository structure**: pnpm workspaces monorepo.
  - `packages/satoru`: Core library and TypeScript wrappers. A shared `core.ts` base class keeps browser and Node.js behavior aligned.
  - `packages/visual-test`: Visual regression test suite.
  - `packages/playground`: Web-based demo application.
  - `packages/cloudflare-ogp`: OGP image generation example for Cloudflare Workers.
