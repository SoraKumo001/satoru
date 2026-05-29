---
sidebar_position: 3
title: Cloudflare Workers CacheStorage
---

# Cloudflare Workers で CacheStorage を使う

Cloudflare Workers では `satoru-render/workerd` entrypoint を使います。画像や外部 CSS などの resource は `CacheStorageResourceCache` で cache できます。

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

## 注意点

- `satoru-render/workerd` は Cloudflare Workers の Wasm 初期化に合わせた entrypoint です。
- `CacheStorageResourceCache` は binary resource の cache に向いています。複数 file に展開される font result は cache 対象外です。
- User input を HTML 文字列へ直接入れる場合は escape してください。
- Public endpoint では `limits` を必ず設定します。特に `allowedProtocols` と resource size limit は重要です。
