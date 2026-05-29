---
sidebar_position: 1
title: API リファレンス
slug: /api-reference
---

# API リファレンス

Satoru の主要 API は、HTML/CSS を SVG、PNG、WebP、PDF に変換する `render()` 関数です。用途に応じて entrypoint を選びます。

## Entrypoints

| Import | 用途 |
| --- | --- |
| `satoru-render/single` | Node.js や bundler 環境で使う高レベル API。embedded Wasm を自動初期化し、instance を再利用します。 |
| `satoru-render/workerd` | Cloudflare Workers 向け API。 |
| `satoru-render/workers` | 複数 job を worker pool で並列処理する API。 |
| `satoru-render/resources` | resource resolver 用の cache helper。 |
| `satoru-render/jsdom` | URL や HTML を JSDOM で hydrate して最終 HTML を取得する helper。 |
| `satoru-render/react` / `satoru-render/preact` | React/Preact element を HTML に変換する helper。 |
| `satoru-render/tailwind` | Tailwind/UnoCSS 系の CSS 生成 helper。 |

## 項目

- [環境ごとの使い分け](/docs/api-reference/runtime-guide): Next.js、Cloudflare Workers、Deno、Web browser などの選び方。
- [render API](/docs/api-reference/render): `render()`、`RenderOptions`、PDF options、`RenderLimits`。
- [Resources](/docs/api-reference/resources): `ResourceResolver`、cache helper。
- [Worker Pool](/docs/api-reference/workers): `createSatoruWorker()` と worker pool lifecycle。
- [CLI](/docs/api-reference/cli): `npx satoru-render` の使い方。
- [Diagnostics](/docs/api-reference/diagnostics): diagnostics report と error code。
