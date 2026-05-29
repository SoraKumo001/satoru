---
sidebar_position: 1
title: 概要
---

# Satoru の概要

Satoru は WebAssembly (Emscripten) 上で動作する、高精度な HTML/CSS から SVG/PNG/PDF への変換エンジンです。

## クイックスタート

```bash
pnpm add satoru-render
```

```typescript
import { render } from "satoru-render/single";

const png = await render({
  value: "<h1>Hello Satoru</h1>",
  width: 1200,
  height: 630,
  format: "png",
});
```

用途に応じて、Node.js では `satoru-render/single`、高スループット処理では `satoru-render/workers`、Cloudflare Workers では `satoru-render/workerd` を使います。

## プロジェクト構成

- **主要技術**: `litehtml` によるレイアウト、`Skia` による描画、軽量な独自ログ基盤。
- **リポジトリ構成**: pnpm workspaces による monorepo。
  - `packages/satoru`: コアライブラリと TypeScript ラッパー。`core.ts` を基底クラスとして、ブラウザ向けと Node.js 向けの処理を共有します。
  - `packages/visual-test`: 視覚回帰テストスイート。
  - `packages/playground`: Web ベースのデモアプリケーション。
  - `packages/cloudflare-ogp`: Cloudflare Workers を使った OGP 画像生成例。
