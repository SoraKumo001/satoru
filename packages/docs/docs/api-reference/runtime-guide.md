---
sidebar_position: 2
title: 環境ごとの使い分け
---

# 環境ごとの使い分け

| 環境 | 推奨 entrypoint | 主な用途 | 注意点 |
| --- | --- | --- | --- |
| Node.js script / server | `satoru-render/single` | 通常の HTML から画像/PDF を生成する server-side rendering。 | instance は内部で再利用されます。大量 job では worker pool を検討してください。 |
| Next.js Route Handler / Server Action | `satoru-render/single` + `satoru-render/react` | React component から OGP 画像や PDF を生成。 | `runtime = "nodejs"` を推奨します。client component の hydrate 後 DOM を取りたい場合は `jsdom` helper を使います。 |
| 既存 Next.js / SPA の URL capture | `satoru-render/jsdom` + `satoru-render/single` | URL を読み込み、script 実行後の HTML を render。 | `jsdom` は peer dependency です。browser API が足りない場合は `beforeParse` で polyfill します。 |
| Cloudflare Workers | `satoru-render/workerd` | edge 上で OGP 画像などを生成。 | workerd の Wasm 初期化に合わせた entrypoint です。CacheStorage を使う場合は `CacheStorageResourceCache` が使えます。 |
| Deno / Deno Deploy | `npm:satoru-render` | Deno.serve で OGP 画像などを生成。 | Deno の npm package support を使います。resource cache には Web Cache API を使えます。 |
| Web browser | `satoru-render` + `satoru-render/satoru.js` | live DOM element の capture、html2canvas 代替。 | browser bundle では `Satoru.create(createSatoruModule)` で Wasm module factory を渡します。 |
| 高スループット Node.js | `satoru-render/workers` | 複数 render job の並列処理。 | job 完了待ちに `waitAll()`、終了時に `close()` を呼びます。 |

## Next.js Route Handler

React component を静的 HTML に変換してから render します。OGP image endpoint や PDF generation endpoint に向いています。

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

既存ページを hydrate 後の HTML として取得したい場合は `getHtml()` を挟みます。

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

Cloudflare Workers では `workerd` entrypoint を使います。

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

Deno では npm specifier を使って import できます。

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

browser 上の live DOM を capture する場合は、Wasm module factory を明示して `Satoru` instance を作ります。

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
