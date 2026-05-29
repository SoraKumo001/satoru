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

`format: "svg"` or an omitted `format` returns a `string`. `"png"`, `"webp"`, and `"pdf"` return `Uint8Array`.

## RenderOptions

| Option | Type | Description |
| --- | --- | --- |
| `value` | `string \| string[] \| any \| any[]` | Input HTML or state object. Arrays can be used for multi-page PDF output. |
| `url` | `string` | Source URL, used for relative URL resolution and diagnostics. |
| `width` | `number` | Layout canvas width. Required. |
| `height` | `number` | Layout canvas height. If omitted, content height is used. |
| `format` | `"svg" \| "png" \| "webp" \| "pdf"` | Output format. Defaults to `"svg"`. |
| `backgroundColor` | `string` | Output background color, such as `"#fff"` or `"rgba(0,0,0,0.5)"`. |
| `crop` | `{ x; y; width; height }` | Crop rectangle for the source canvas. |
| `outputWidth` / `outputHeight` | `number` | Final output size when resizing is needed. |
| `fit` | `"contain" \| "cover" \| "fill"` | Strategy for fitting the canvas into the output size. Defaults to `"contain"`. |
| `fitPosition` | `{ x: number; y: number }` | Fitting origin. Defaults to `{ x: 0.5, y: 0.5 }`. |
| `css` | `string` | Additional CSS. |
| `baseUrl` | `string` | Base URL for resolving relative URLs. |
| `mediaType` | `"screen" \| "print"` | Media type for CSS media queries. Defaults to `"screen"`. |
| `fonts` | `{ name; data }[]` | Preloaded font data. |
| `fallbackFonts` | `Uint8Array[]` | Fallback font data. |
| `images` | `{ name; url; width?; height? }[]` | Named image resources. |
| `fontMap` | `Record<string, string>` | Font family to CSS/font URL mapping. |
| `resolveResource` | `ResourceResolver` | Custom resolver for fonts, CSS, and images. |
| `limits` | `RenderLimits` | Timeout and resource limits. |
| `diagnostics` | `boolean` | Enables detailed diagnostics. |
| `onDiagnostics` | `(report) => void` | Receives the diagnostics report. |
| `profile` | `boolean` | Enables timing profiles. |
| `onProfile` | `(profile) => void` | Receives timing profile data. |
| `signal` | `AbortSignal` | Abort signal for the render. |

## PDF Options

| Option | Type | Description |
| --- | --- | --- |
| `pdfTitle` | `string` | PDF title metadata. |
| `pdfAuthor` | `string` | PDF author metadata. |
| `pdfSubject` | `string` | PDF subject metadata. |
| `pdfKeywords` | `string` | PDF keywords metadata. |
| `pdfCreator` | `string` | PDF creator metadata. |
| `pdfProducer` | `string` | PDF producer metadata. |
| `pdfMargin` | `{ top?; right?; bottom?; left? }` | Page margin in pixels. |
| `pdfHeader` | `string` | Header HTML template. Supports `{{pageNumber}}` and `{{totalPages}}`. |
| `pdfFooter` | `string` | Footer HTML template. Supports `{{pageNumber}}` and `{{totalPages}}`. |

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

| Option | Description |
| --- | --- |
| `timeoutMs` | Timeout for the full render process. |
| `maxResourceBytes` | Maximum byte size for a single resource. |
| `maxTotalResourceBytes` | Maximum total byte size for all resources. |
| `maxResourceCount` | Maximum number of resources to load. |
| `allowedProtocols` | Allowed URL protocols, such as `["https:"]`. |
| `allowedHosts` | Allowed hostnames. |
| `blockedHosts` | Blocked hostnames. |
