---
sidebar_position: 6
title: Font Management
---

# Load Fonts Reliably

Font loading failures directly affect OGP images and PDFs. Satoru lets you control font resolution with `fonts`, `fallbackFonts`, `fontMap`, `baseUrl`, and `resolveResource`.

## Pass Local Fonts Explicitly

In Node.js, the most reliable approach is to read font files with `fs.readFile` and pass them through `fonts`.

```typescript
import { readFile } from "node:fs/promises";
import { render } from "satoru-render/single";

const inter = await readFile("./assets/Inter-Regular.ttf");
const notoSansJp = await readFile("./assets/NotoSansJP-Regular.otf");

const png = await render({
  value: `
    <main style="font-family: Inter, 'Noto Sans JP', sans-serif;">
      <h1>Hello Satoru</h1>
      <p>日本語の本文も安定して描画します。</p>
    </main>
  `,
  width: 1200,
  height: 630,
  format: "png",
  fonts: [
    { name: "Inter", data: new Uint8Array(inter) },
    { name: "Noto Sans JP", data: new Uint8Array(notoSansJp) },
  ],
});
```

## Pin Remote Fonts with `fontMap`

When many documents share the same font, map the font-family name to a fixed URL with `fontMap`.

```typescript
const png = await render({
  value: `
    <main style="font-family: 'Noto Sans JP', sans-serif;">
      <h1>こんにちは</h1>
    </main>
  `,
  width: 1200,
  height: 630,
  format: "png",
  fontMap: {
    "Noto Sans JP": "https://example.com/fonts/NotoSansJP-Regular.woff2",
  },
  limits: {
    allowedProtocols: ["https:"],
    allowedHosts: ["example.com"],
    maxResourceBytes: 5 * 1024 * 1024,
  },
});
```

## Provide Fallback Fonts

When input HTML can contain unexpected characters, pass a broad-coverage font through `fallbackFonts`.

```typescript
const fallback = await readFile("./assets/NotoSansJP-Regular.otf");

await render({
  value: "<p>English / 日本語 / symbols</p>",
  width: 800,
  format: "png",
  fallbackFonts: [new Uint8Array(fallback)],
});
```

## Notes

- Provide fallback fonts for Japanese, emoji, and symbols.
- When using remote fonts, pin the source with `limits.allowedHosts`.
- Enable `diagnostics: true` to see which fonts resolved and which resources failed.
- For high-traffic OGP endpoints, combine font loading with resource caching as shown in [Production OGP Generation](./ogp-production).
