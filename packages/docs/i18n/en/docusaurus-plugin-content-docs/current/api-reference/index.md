---
sidebar_position: 1
title: API Reference
slug: /api-reference
---

# API Reference

Satoru's main API is the `render()` function, which converts HTML/CSS into SVG, PNG, WebP, or PDF. Choose an entrypoint based on the runtime and throughput requirements.

## Entrypoints

| Import | Purpose |
| --- | --- |
| `satoru-render/single` | High-level API for Node.js and bundler environments. It initializes embedded Wasm automatically and reuses the instance. |
| `satoru-render/workerd` | API specialized for Cloudflare Workers. |
| `satoru-render/workers` | Worker-pool API for parallel rendering. |
| `satoru-render/resources` | Cache helpers for resource resolvers. |
| `satoru-render/jsdom` | Helper for hydrating URLs or HTML with JSDOM and returning final HTML. |
| `satoru-render/react` / `satoru-render/preact` | Helpers for converting React/Preact elements to HTML. |
| `satoru-render/tailwind` | Helper for generating Tailwind/UnoCSS-style CSS. |

## Sections

- [Runtime Guide](/docs/api-reference/runtime-guide): How to choose between Next.js, Cloudflare Workers, Deno, Web browser, and Node.js.
- [render API](/docs/api-reference/render): `render()`, `RenderOptions`, PDF options, and `RenderLimits`.
- [Resources](/docs/api-reference/resources): `ResourceResolver` and cache helpers.
- [Worker Pool](/docs/api-reference/workers): `createSatoruWorker()` and worker pool lifecycle.
- [CLI](/docs/api-reference/cli): How to use `npx satoru-render`.
- [Diagnostics](/docs/api-reference/diagnostics): Diagnostics reports and error codes.
