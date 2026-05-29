---
sidebar_position: 2
title: Runtime Guide
---

# Runtime Guide

| Environment | Recommended entrypoint | Best for | Notes |
| --- | --- | --- | --- |
| Node.js script / server | `satoru-render/single` | Standard server-side HTML to image/PDF rendering. | The instance is reused internally. For many jobs, consider the worker pool. |
| Next.js Route Handler / Server Action | `satoru-render/single` + `satoru-render/react` | OGP images or PDFs generated from React components. | Prefer `runtime = "nodejs"`. Use the `jsdom` helper when you need hydrated client-side DOM output. |
| Existing Next.js / SPA URL capture | `satoru-render/jsdom` + `satoru-render/single` | Load a URL, execute scripts, then render the resulting HTML. | `jsdom` is a peer dependency. Add missing browser API polyfills with `beforeParse`. |
| Cloudflare Workers | `satoru-render/workerd` | OGP images and edge rendering. | This entrypoint matches workerd's Wasm instantiation requirements. Use `CacheStorageResourceCache` when you want Web Cache API backed resources. |
| Deno / Deno Deploy | `npm:satoru-render` | OGP images with `Deno.serve`. | Uses Deno's npm package support. You can use the Web Cache API for resource caching. |
| Web browser | `satoru-render` + `satoru-render/satoru.js` | Live DOM capture and html2canvas-style use cases. | Browser bundles pass the Wasm module factory to `Satoru.create(createSatoruModule)`. |
| High-throughput Node.js | `satoru-render/workers` | Parallel rendering jobs. | Call `waitAll()` before shutdown and `close()` when the pool is no longer needed. |

## Next.js Route Handler

Convert a React component to static HTML, then render it. This is a good fit for OGP image endpoints and PDF generation endpoints.

```tsx
// app/api/og/route.tsx
import { render } from "satoru-render/single";
import { toHtml } from "satoru-render/react";

export const runtime = "nodejs";

export async function GET() {
  const html = toHtml(
    <main style={{ width: 1200, height: 630, padding: 64 }}>
      <h1>Hello from Next.js</h1>
    </main>,
  );

  const png = await render({
    value: html,
    width: 1200,
    height: 630,
    format: "png",
  });

  return new Response(png, {
    headers: { "Content-Type": "image/png" },
  });
}
```

## Next.js / SPA URL Capture

Use `getHtml()` when you need the HTML after hydration.

```typescript
import { render } from "satoru-render/single";
import { getHtml } from "satoru-render/jsdom";

const url = "https://example.com/";

const html = await getHtml({
  src: url,
  waitUntil: "networkidle",
  beforeParse: (window) => {
    window.matchMedia = window.matchMedia ?? (() => ({ matches: false }));
  },
});

const png = await render({
  value: html,
  baseUrl: url,
  width: 1200,
  height: 630,
  format: "png",
});
```

## Cloudflare Workers

Use the `workerd` entrypoint in Cloudflare Workers.

```typescript
import { render } from "satoru-render/workerd";

export default {
  async fetch() {
    const png = await render({
      value: "<h1>Cloudflare Workers</h1>",
      width: 1200,
      height: 630,
      format: "png",
      limits: {
        timeoutMs: 5000,
        allowedProtocols: ["https:"],
      },
    });

    return new Response(png, {
      headers: { "Content-Type": "image/png" },
    });
  },
};
```

## Deno

Use npm specifiers in Deno.

```tsx
/** @jsx h */
import { h, toHtml } from "npm:satoru-render/preact";
import { createCSS } from "npm:satoru-render/tailwind";
import { render } from "npm:satoru-render";

Deno.serve(async () => {
  const html = toHtml(
    <main className="w-[1200px] h-[630px] flex items-center justify-center">
      <h1 className="text-[80px] font-bold">Hello Deno</h1>
    </main>,
  );

  const png = await render({
    value: html,
    css: await createCSS(html),
    width: 1200,
    height: 630,
    format: "png",
  });

  return new Response(png, {
    headers: { "Content-Type": "image/png" },
  });
});
```

## Web Browser

Create a `Satoru` instance with the Wasm module factory when capturing live DOM in the browser.

```typescript
import { Satoru } from "satoru-render";
import createSatoruModule from "satoru-render/satoru.js";

const satoru = await Satoru.create(createSatoruModule);
const element = document.getElementById("target");

if (element) {
  const canvas = await satoru.capture(element, {
    format: "png",
  });
  document.body.appendChild(canvas);
}
```
