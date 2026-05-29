---
sidebar_position: 5
title: PDF Reports
---

# Generate Multi-Page PDF Reports

Satoru generates a PDF with one page per HTML string when `value` is an array. This works well for fixed-layout documents such as reports, invoices, and weekly summaries.

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

## Notes

- `width` and `height` define the PDF page size. Use `595 x 842` for A4-like output, or another fixed size for your document format.
- `pdfHeader` and `pdfFooter` support `{{pageNumber}}` and `{{totalPages}}`.
- Use `mediaType: "print"` when your CSS is print-oriented.
- For long documents, split HTML into page-sized chunks and pass `value: string[]` for predictable output.
