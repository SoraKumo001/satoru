---
sidebar_position: 4
title: CSS 互換性
---

# Satoru 互換性エビデンス

この文書は、さまざまな CSS 機能と出力形式に対する Satoru の描画能力を記録します。ステータスは、Satoru の出力とブラウザベースの参照画像を比較する視覚回帰テストスイートから生成されます。

## 判定基準

- `✅`: 視覚回帰テストの許容範囲内、または差分の原因が既知で実用上許容できるもの。
- `⚠️`: 一部の出力形式に制限がある、または baseline がまだ揃っていないもの。
- `Diff`: ブラウザ参照画像との差分率。数値が高い項目は、Notes や既知の注意点と合わせて確認してください。

## 機能サポート matrix

| Group | Feature | Asset | PNG | SVG | PDF | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| Layout | layout | [01-layout.html](pathname:///assets/01-layout.html) | ✅ | ✅ | ✅ | Diff: 12.03% |
| Text | typography | [02-typography.html](pathname:///assets/02-typography.html) | ✅ | ✅ | ✅ | Diff: 2.75% |
| Graphics | graphics | [03-graphics.html](pathname:///assets/03-graphics.html) | ✅ | ✅ | ✅ | Diff: 3.16% |
| Layout | box model | [04-box-model.html](pathname:///assets/04-box-model.html) | ✅ | ✅ | ✅ | Diff: 3.54% |
| Graphics | images | [05-images.html](pathname:///assets/05-images.html) | ✅ | ✅ | ✅ | Diff: 3.88% |
| Flexbox | flexbox | [06-flexbox.html](pathname:///assets/06-flexbox.html) | ✅ | ✅ | ✅ | Diff: 2.68% |
| Layout | table | [07-table.html](pathname:///assets/07-table.html) | ✅ | ✅ | ✅ | Diff: 5.06% |
| Layout | list marker | [08-list-marker.html](pathname:///assets/08-list-marker.html) | ✅ | ✅ | ✅ | Diff: 1.03% |
| Grid | grid | [09-grid.html](pathname:///assets/09-grid.html) | ✅ | ✅ | ✅ | Diff: 2.50% |
| Modern CSS | container queries | [10-container-queries.html](pathname:///assets/10-container-queries.html) | ✅ | ✅ | ✅ | Diff: 4.71% |
| Text | complex text | [11-complex-text.html](pathname:///assets/11-complex-text.html) | ✅ | ✅ | ✅ | Diff: 1.63% |
| Text | bidi text | [12-bidi-text.html](pathname:///assets/12-bidi-text.html) | ✅ | ✅ | ✅ | Diff: 2.52% |
| Effects | backdrop filter | [13-backdrop-filter.html](pathname:///assets/13-backdrop-filter.html) | ✅ | ✅ | ✅ | Diff: 0.18% |
| Layout | line clamp | [14-line-clamp.html](pathname:///assets/14-line-clamp.html) | ✅ | ✅ | ✅ | Diff: 4.12% |
| Layout | showcase | [15-showcase.html](pathname:///assets/15-showcase.html) | ✅ | ✅ | ✅ | Diff: 3.71% |
| Layout | advanced shapes | [16-advanced-shapes.html](pathname:///assets/16-advanced-shapes.html) | ✅ | ✅ | ✅ | Diff: 0.67% |
| Effects | background clip | [17-background-clip.html](pathname:///assets/17-background-clip.html) | ✅ | ✅ | ✅ | Diff: 1.19% |
| Graphics | border image | [18-border-image.html](pathname:///assets/18-border-image.html) | ✅ | ✅ | ✅ | Diff: 1.90% |
| Effects | mask | [19-mask.html](pathname:///assets/19-mask.html) | ✅ | ✅ | ✅ | Diff: 1.11% |
| Flexbox | flex absolute | [20-flex-absolute.html](pathname:///assets/20-flex-absolute.html) | ✅ | ✅ | ✅ | Diff: 1.09% |
| Text | vertical writing | [21-vertical-writing.html](pathname:///assets/21-vertical-writing.html) | ✅ | ✅ | ✅ | Diff: 1.92% |
| Modern CSS | modern features | [22-modern-features.html](pathname:///assets/22-modern-features.html) | ✅ | ✅ | ✅ | Diff: 13.08% |
| Layout | test repro black | [23-test-repro-black.html](pathname:///assets/23-test-repro-black.html) | ✅ | ✅ | ✅ | Diff: 8.08% |
| Print | media print | [24-media-print.html](pathname:///assets/24-media-print.html) | ✅ | ✅ | ✅ | Diff: 3.30% |
| Layout | page breaks | [25-page-breaks.html](pathname:///assets/25-page-breaks.html) | ⚠️ | ⚠️ | ✅ | Not in baseline |

## サポートする出力形式

| Format | Status | Description |
| --- | --- | --- |
| **PNG** | ✅ Supported | Skia ベースの高性能 raster 出力。 |
| **SVG** | ✅ Supported | 高精度な XML ベースの vector 出力。 |
| **PDF** | ✅ Supported | 複数ページ document 生成をサポート。 |
| **WebP** | ✅ Supported | Skia による効率的な raster 出力。 |

## 既知の注意点

- **Vertical Writing**: 基本的なサポートは実装済みですが、縦書き text と float 要素の複雑な組み合わせでは、軽微な alignment 差が出る場合があります。
- **Container Queries**: JSDOM hydration phase 経由でサポートします。
- **Backdrop Filter**: Skia backend support (PNG/WebP/PDF) が必要です。背景 pixel に依存するため、raw SVG 出力では利用できません。

---
*この文書は visual test registry から自動生成されます。*
