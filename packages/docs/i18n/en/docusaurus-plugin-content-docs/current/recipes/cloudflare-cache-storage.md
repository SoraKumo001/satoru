---
sidebar_position: 3
title: Cloudflare Workers CacheStorage
---

# Use CacheStorage in Cloudflare Workers

In Cloudflare Workers, use the `satoru-render/workerd` entrypoint. Resources such as images and external CSS can be cached with `CacheStorageResourceCache`.

## Recipe

```typescript
import { render } from "satoru-render/workerd";
import { CacheStorageResourceCache } from "satoru-render/resources";

const resourceCache = new CacheStorageResourceCache("satoru-ogp-resources");

export default {
  async fetch(request: Request) {
    const url = new URL(request.url);
    const title = url.searchParams.get("title") ?? "Satoru on Workers";

    const html = `
      <main style="
        width: 1200px;
        height: 630px;
        display: flex;
        align-items: center;
        justify-content: center;
        background: #0f172a;
        color: white;
        font-family: sans-serif;
      ">
        <h1 style="font-size: 72px">${escapeHtml(title)}</h1>
      </main>
    `;

    const png = await render({
      value: html,
      width: 1200,
      height: 630,
      format: "png",
      resolveResource: resourceCache.wrap(),
      limits: {
        timeoutMs: 5000,
        maxResourceBytes: 5 * 1024 * 1024,
        maxTotalResourceBytes: 20 * 1024 * 1024,
        maxResourceCount: 20,
        allowedProtocols: ["https:"],
        blockedHosts: ["localhost", "127.0.0.1"],
      },
    });

    return new Response(png, {
      headers: {
        "Content-Type": "image/png",
        "Cache-Control": "public, max-age=86400",
      },
    });
  },
};

function escapeHtml(value: string) {
  return value.replace(/[&<>"']/g, (ch) => {
    const entities: Record<string, string> = {
      "&": "&amp;",
      "<": "&lt;",
      ">": "&gt;",
      '"': "&quot;",
      "'": "&#39;",
    };
    return entities[ch];
  });
}
```

## Notes

- `satoru-render/workerd` matches Cloudflare Workers' Wasm instantiation requirements.
- `CacheStorageResourceCache` is useful for binary resources. Compound font results are not cached.
- Escape user input before inserting it into raw HTML strings.
- Public endpoints should always set `limits`, especially protocol and resource size limits.
