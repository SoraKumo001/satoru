---
sidebar_position: 5
title: PDF レポート生成
---

# 複数ページ PDF レポートを生成する

Satoru は `value` に HTML 文字列の配列を渡すと、各要素を 1 ページとして PDF を生成します。帳票、請求書、週次レポートのように固定レイアウトの document を作る場合に便利です。

## Recipe

```typescript
import { render } from "satoru-render/single";

const pages = [
  `
    <section class="page cover">
      <h1>Weekly Report</h1>
      <p>2026-05-29</p>
    </section>
  `,
  `
    <section class="page">
      <h2>Summary</h2>
      <p>Revenue and usage stayed within the expected range.</p>
    </section>
  `,
  `
    <section class="page">
      <h2>Next Actions</h2>
      <ul>
        <li>Review slow render diagnostics.</li>
        <li>Refresh cached brand assets.</li>
      </ul>
    </section>
  `,
];

const css = `
  body {
    margin: 0;
    font-family: Inter, sans-serif;
    color: #111827;
  }

  .page {
    width: 595px;
    min-height: 842px;
    padding: 56px;
    box-sizing: border-box;
  }

  .cover {
    display: flex;
    flex-direction: column;
    justify-content: center;
    background: #eef2ff;
  }
`;

const pdf = await render({
  value: pages,
  css,
  width: 595,
  height: 842,
  format: "pdf",
  pdfTitle: "Weekly Report",
  pdfAuthor: "Satoru",
  pdfMargin: {
    top: 24,
    right: 24,
    bottom: 36,
    left: 24,
  },
  pdfFooter: `
    <div style="font-size: 10px; color: #6b7280; text-align: center;">
      Page {{pageNumber}} / {{totalPages}}
    </div>
  `,
});
```

## 注意点

- `width` と `height` は PDF のページ基準サイズとして扱います。A4 相当なら `595 x 842`、OGP 風なら `1200 x 630` のように用途に合わせます。
- `pdfHeader` と `pdfFooter` では `{{pageNumber}}` と `{{totalPages}}` を使えます。
- 印刷向け CSS を使う場合は `mediaType: "print"` を指定します。
- 長い本文を自動でページ分割したい場合は、事前に HTML をページ単位へ分割して `value: string[]` に渡す構成が安定します。
