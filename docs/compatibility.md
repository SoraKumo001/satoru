# Satoru Compatibility Evidence

This document tracks the rendering capabilities of Satoru across various CSS features and output formats. Status is derived from the visual regression test suite which compares Satoru output against browser-based reference images.

## Feature Support Matrix

| Group | Feature | Asset | PNG | SVG | PDF | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| Layout | layout | [01-layout.html](../assets/01-layout.html) | ✅ | ✅ | ✅ | Diff: 12.03% |
| Text | typography | [02-typography.html](../assets/02-typography.html) | ✅ | ✅ | ✅ | Diff: 2.75% |
| Graphics | graphics | [03-graphics.html](../assets/03-graphics.html) | ✅ | ✅ | ✅ | Diff: 3.16% |
| Layout | box model | [04-box-model.html](../assets/04-box-model.html) | ✅ | ✅ | ✅ | Diff: 3.54% |
| Graphics | images | [05-images.html](../assets/05-images.html) | ✅ | ✅ | ✅ | Diff: 3.88% |
| Flexbox | flexbox | [06-flexbox.html](../assets/06-flexbox.html) | ✅ | ✅ | ✅ | Diff: 2.68% |
| Layout | table | [07-table.html](../assets/07-table.html) | ✅ | ✅ | ✅ | Diff: 5.06% |
| Layout | list marker | [08-list-marker.html](../assets/08-list-marker.html) | ✅ | ✅ | ✅ | Diff: 1.03% |
| Grid | grid | [09-grid.html](../assets/09-grid.html) | ✅ | ✅ | ✅ | Diff: 2.50% |
| Modern CSS | container queries | [10-container-queries.html](../assets/10-container-queries.html) | ✅ | ✅ | ✅ | Diff: 4.71% |
| Text | complex text | [11-complex-text.html](../assets/11-complex-text.html) | ✅ | ✅ | ✅ | Diff: 1.63% |
| Text | bidi text | [12-bidi-text.html](../assets/12-bidi-text.html) | ✅ | ✅ | ✅ | Diff: 2.52% |
| Effects | backdrop filter | [13-backdrop-filter.html](../assets/13-backdrop-filter.html) | ✅ | ✅ | ✅ | Diff: 0.18% |
| Layout | line clamp | [14-line-clamp.html](../assets/14-line-clamp.html) | ✅ | ✅ | ✅ | Diff: 4.12% |
| Layout | showcase | [15-showcase.html](../assets/15-showcase.html) | ✅ | ✅ | ✅ | Diff: 3.71% |
| Layout | advanced shapes | [16-advanced-shapes.html](../assets/16-advanced-shapes.html) | ✅ | ✅ | ✅ | Diff: 0.67% |
| Effects | background clip | [17-background-clip.html](../assets/17-background-clip.html) | ✅ | ✅ | ✅ | Diff: 1.19% |
| Graphics | border image | [18-border-image.html](../assets/18-border-image.html) | ✅ | ✅ | ✅ | Diff: 1.90% |
| Effects | mask | [19-mask.html](../assets/19-mask.html) | ✅ | ✅ | ✅ | Diff: 1.11% |
| Flexbox | flex absolute | [20-flex-absolute.html](../assets/20-flex-absolute.html) | ✅ | ✅ | ✅ | Diff: 1.09% |
| Text | vertical writing | [21-vertical-writing.html](../assets/21-vertical-writing.html) | ✅ | ✅ | ✅ | Diff: 1.92% |
| Modern CSS | modern features | [22-modern-features.html](../assets/22-modern-features.html) | ✅ | ✅ | ✅ | Diff: 13.08% |
| Layout | test repro black | [23-test-repro-black.html](../assets/23-test-repro-black.html) | ✅ | ✅ | ✅ | Diff: 8.08% |
| Print | media print | [24-media-print.html](../assets/24-media-print.html) | ✅ | ✅ | ✅ | Diff: 3.30% |
| Layout | page breaks | [25-page-breaks.html](../assets/25-page-breaks.html) | ⚠️ | ⚠️ | ✅ | Not in baseline |

## Supported Output Formats

| Format | Status | Description |
| --- | --- | --- |
| **PNG** | ✅ Supported | High-performance Skia-based raster output. |
| **SVG** | ✅ Supported | XML-based vector output with high fidelity. |
| **PDF** | ✅ Supported | Multi-page document generation support. |
| **WebP** | ✅ Supported | Efficient raster output using Skia. |

## Known Caveats

- **Vertical Writing**: Basic support is implemented but complex combinations of vertical text with floating elements may have minor alignment differences.
- **Container Queries**: Supported via JSDOM hydration phase.
- **Backdrop Filter**: Requires Skia backend support (PNG/WebP/PDF). Not available in raw SVG output as it depends on background pixels.

---
*This document is automatically generated from the visual test registry.*
