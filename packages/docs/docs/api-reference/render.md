---
sidebar_position: 3
title: render API
---

# render API

```typescript
import { render } from "satoru-render/single";

const svg = await render({
  value: "<main><h1>Hello</h1></main>",
  width: 1200,
  height: 630,
  format: "svg",
});

const png = await render({
  value: "<main><h1>Hello</h1></main>",
  width: 1200,
  height: 630,
  format: "png",
});
```

`format: "svg"` または `format` 省略時は `string` を返します。`"png"`、`"webp"`、`"pdf"` は `Uint8Array` を返します。

## RenderOptions

| Option | Type | 説明 |
| --- | --- | --- |
| `value` | `string \| string[] \| any \| any[]` | 入力 HTML、または state object。複数 page PDF では配列を渡せます。 |
| `url` | `string` | source URL。相対 URL の解決や診断に使います。 |
| `width` | `number` | layout 用 canvas width。必須です。 |
| `height` | `number` | layout 用 canvas height。省略時は content height から決定されます。 |
| `format` | `"svg" \| "png" \| "webp" \| "pdf"` | 出力形式。既定は `"svg"` です。 |
| `backgroundColor` | `string` | 出力背景色。例: `"#fff"`、`"rgba(0,0,0,0.5)"`。 |
| `crop` | `{ x; y; width; height }` | source canvas の切り抜き範囲。 |
| `outputWidth` / `outputHeight` | `number` | 最終出力 size。resize が必要な場合に使います。 |
| `fit` | `"contain" \| "cover" \| "fill"` | output size への fitting strategy。既定は `"contain"`。 |
| `fitPosition` | `{ x: number; y: number }` | fitting 時の origin。既定は `{ x: 0.5, y: 0.5 }`。 |
| `css` | `string` | 追加 CSS。 |
| `baseUrl` | `string` | 相対 URL の基準 URL。 |
| `mediaType` | `"screen" \| "print"` | CSS media query 用の media type。既定は `"screen"`。 |
| `fonts` | `{ name; data }[]` | 事前に読み込んだ font data。 |
| `fallbackFonts` | `Uint8Array[]` | fallback font data。 |
| `images` | `{ name; url; width?; height? }[]` | named image resource。 |
| `fontMap` | `Record<string, string>` | font family から CSS/font URL への mapping。 |
| `resolveResource` | `ResourceResolver` | font、CSS、image の取得処理を差し替えます。 |
| `limits` | `RenderLimits` | timeout や resource size などの制限。 |
| `diagnostics` | `boolean` | 詳細診断を有効化します。 |
| `onDiagnostics` | `(report) => void` | 診断 report を受け取る callback。 |
| `profile` | `boolean` | timing profile を有効化します。 |
| `onProfile` | `(profile) => void` | timing profile を受け取る callback。 |
| `signal` | `AbortSignal` | render の abort signal。 |

## PDF Options

| Option | Type | 説明 |
| --- | --- | --- |
| `pdfTitle` | `string` | PDF title metadata。 |
| `pdfAuthor` | `string` | PDF author metadata。 |
| `pdfSubject` | `string` | PDF subject metadata。 |
| `pdfKeywords` | `string` | PDF keywords metadata。 |
| `pdfCreator` | `string` | PDF creator metadata。 |
| `pdfProducer` | `string` | PDF producer metadata。 |
| `pdfMargin` | `{ top?; right?; bottom?; left? }` | page margin。単位は pixel。 |
| `pdfHeader` | `string` | header HTML template。`{{pageNumber}}` と `{{totalPages}}` を使えます。 |
| `pdfFooter` | `string` | footer HTML template。`{{pageNumber}}` と `{{totalPages}}` を使えます。 |

## RenderLimits

```typescript
const png = await render({
  value: html,
  width: 1200,
  height: 630,
  format: "png",
  limits: {
    timeoutMs: 5000,
    maxResourceBytes: 5 * 1024 * 1024,
    maxTotalResourceBytes: 20 * 1024 * 1024,
    maxResourceCount: 20,
    allowedProtocols: ["https:"],
    blockedHosts: ["localhost", "127.0.0.1"],
  },
});
```

| Option | 説明 |
| --- | --- |
| `timeoutMs` | render 全体の timeout。 |
| `maxResourceBytes` | 1 resource あたりの最大 byte 数。 |
| `maxTotalResourceBytes` | 全 resource 合計の最大 byte 数。 |
| `maxResourceCount` | 読み込む resource 数の上限。 |
| `allowedProtocols` | 許可する URL protocol。例: `["https:"]`。 |
| `allowedHosts` | 許可する hostname。 |
| `blockedHosts` | 拒否する hostname。 |
