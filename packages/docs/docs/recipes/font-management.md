---
sidebar_position: 6
title: フォント管理
---

# フォントを安定して読み込む

OGP 画像や PDF では、font の読み込み失敗が見た目に直結します。Satoru では `fonts`、`fallbackFonts`、`fontMap`、`baseUrl`、`resolveResource` を組み合わせて font resolution を制御できます。

## ローカル font を明示的に渡す

Node.js では `fs.readFile` で font file を読み込み、`fonts` に渡すのが最も確実です。

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

## `fontMap` で remote font を固定する

同じ font を複数の HTML で使う場合は、font-family と URL の対応を `fontMap` で指定できます。

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

## fallback font を用意する

入力 HTML に想定外の文字が含まれる場合は、`fallbackFonts` に広い glyph coverage を持つ font を渡します。

```typescript
const fallback = await readFile("./assets/NotoSansJP-Regular.otf");

await render({
  value: "<p>English / 日本語 / symbols</p>",
  width: 800,
  format: "png",
  fallbackFonts: [new Uint8Array(fallback)],
});
```

## 注意点

- 日本語、emoji、記号を扱う endpoint では fallback font を用意すると安定します。
- Remote font を使う場合は `limits.allowedHosts` で取得先を固定します。
- `diagnostics: true` を有効にすると、どの font が解決されたか、どの resource が失敗したかを追跡できます。
- OGP のような高頻度 endpoint では [本番 OGP 生成](./ogp-production) のように resource cache と組み合わせます。
