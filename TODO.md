# Satoru Feature TODO

This document lists high-value feature work for Satoru. The current engine already has broad HTML/CSS rendering coverage, so the next layer of value should focus on diagnostics, developer workflow, production readiness, and measurable compatibility.

## Priority Overview

| Priority | Feature | Main Value | Suggested Scope |
| --- | --- | --- | --- |
| P0 | Render diagnostics report | Makes rendering failures explainable | Core API, C++ resource/font reporting, CLI |
| P1 | Browser/Satoru diff viewer | Makes visual differences easy to inspect | Playground, visual-test utilities |
| P1 | CLI production options | Makes batch/CI usage practical | `packages/satoru/src/cli.ts` |
| P2 | Resource cache helpers | Reduces repeated userland boilerplate | TS resource resolver helpers |
| P2 | PDF document options | Expands report/document use cases | TS options, PDF renderer |
| P3 | Compatibility matrix generation | Turns visual test output into public docs | visual-test tools, docs |

## P0: Render Diagnostics Report

### Goal

Add a structured diagnostics output that explains what happened during rendering:

- Which resources were discovered.
- Which resources were loaded successfully.
- Which resources failed.
- Which fonts were requested, resolved, missing, or substituted.
- How long major phases took.
- Which output backend was used.
- Whether fallback behavior affected the final output.

This should make it much easier to debug cases like missing images, unexpected fonts, different output between SVG and PNG, or slow renders in edge environments.

### Proposed API

Extend `RenderOptions` with:

```ts
diagnostics?: boolean;
onDiagnostics?: (report: RenderDiagnostics) => void;
```

Add a result helper for users who want the output and report together:

```ts
const result = await renderWithDiagnostics({
  value: html,
  width: 1200,
  format: "png",
});

result.output;
result.diagnostics;
```

Keep `render()` backward-compatible. If `diagnostics` is enabled on `render()`, return the same output type and deliver diagnostics through `onDiagnostics`.

### Proposed Types

```ts
export interface RenderDiagnostics {
  format: "svg" | "png" | "webp" | "pdf";
  width: number;
  height?: number;
  mediaType: "screen" | "print";
  timings: Record<string, number>;
  resources: ResourceDiagnostic[];
  fonts: FontDiagnostic[];
  warnings: DiagnosticMessage[];
  errors: DiagnosticMessage[];
}

export interface ResourceDiagnostic {
  type: "font" | "css" | "image";
  url: string;
  name?: string;
  status: "pending" | "loaded" | "failed" | "skipped";
  bytes?: number;
  reason?: string;
}

export interface FontDiagnostic {
  family: string;
  weight?: number;
  style?: "normal" | "italic" | "oblique";
  status: "loaded" | "fallback" | "missing";
  source?: string;
  characters?: string;
}

export interface DiagnosticMessage {
  code: string;
  message: string;
  source?: string;
}
```

### Implementation Notes

- Start in `packages/satoru/src/core.ts`.
- Reuse the existing `profile` / `onProfile` concept where possible.
- Expose missing font data already tracked by `container_skia`.
- Add C++ API calls only for data that cannot be reconstructed in TypeScript.
- Keep diagnostic generation disabled by default to avoid overhead.

### Likely Files

- `packages/satoru/src/core.ts`
- `src/cpp/api/satoru_api.cpp`
- `src/cpp/api/satoru_api.h`
- `src/cpp/core/container_skia.h`
- `src/cpp/core/container_skia.cpp`
- `packages/satoru/src/cli.ts`

### Acceptance Criteria

- Existing `render()` behavior and return types remain compatible.
- Enabling diagnostics returns resource, font, warning, and timing data.
- Missing image and missing font cases are represented in the diagnostics report.
- CLI can write the diagnostics report as JSON.
- Unit or integration coverage exists for at least one missing resource and one successful resource.

## P1: Browser/Satoru Diff Viewer

### Goal

Add a comparison mode to the Playground that shows:

- Browser preview.
- Satoru output.
- Visual diff.
- Render timing and diagnostics summary.

This turns the Playground into a practical debugging tool instead of only a demo.

### Proposed UX

Add a view mode selector:

- Preview
- Output
- Diff
- Side-by-side

The side-by-side mode should show browser rendering and Satoru rendering at the same viewport size. Diff mode should highlight changed pixels using the existing visual-test comparison approach when possible.

### Implementation Notes

- Reuse asset loading from the current Playground.
- Generate browser reference by screenshotting the iframe or by rendering the same HTML in a hidden controlled surface.
- For SVG output, compare rasterized SVG against browser output.
- Keep large images and object URLs cleaned up when params change.

### Likely Files

- `packages/playground/src/App.tsx`
- `packages/visual-test/src/utils.ts`
- `packages/visual-test/tools/convert_assets.ts`

### Acceptance Criteria

- Users can switch between Preview, Output, Diff, and Side-by-side modes.
- PNG, WebP, SVG, and PDF formats still render without regressions.
- Diff mode clearly reports when comparison is unavailable.
- Object URLs are revoked when no longer needed.
- The Playground remains usable on narrow screens.

## P1: CLI Production Options

### Goal

Make the CLI useful for CI, batch rendering, and debugging without writing custom scripts.

### Proposed Options

- `--base-url <url-or-path>`: Override resource base URL.
- `--css <path-or-string>`: Add extra CSS.
- `--font-map <json-or-path>`: Provide Google Fonts/provider mapping.
- `--json-report <path>`: Write diagnostics and timings.
- `--timeout <ms>`: Fail slow renders or hydration.
- `--scale <number>`: Render at higher/lower density.
- `--crop <x,y,width,height>`: Crop source canvas.
- `--output-width <number>` and `--output-height <number>`: Resize output.
- `--fit <contain|cover|fill>`: Control resize behavior.
- `--media <screen|print>`: Validate value and default clearly.

### Implementation Notes

- Keep the existing no-dependency CLI style unless argument parsing becomes too complex.
- Validate inputs with clear error messages.
- Infer output format from extension only when `--format` is omitted.
- For `--json-report`, enable diagnostics automatically.

### Likely Files

- `packages/satoru/src/cli.ts`
- `packages/satoru/README.md`
- `README.md`

### Acceptance Criteria

- CLI supports all proposed options.
- Invalid option values fail with a useful message and non-zero exit code.
- `--json-report` writes a stable JSON structure.
- Existing documented CLI usage still works.

## P2: Resource Cache Helpers

### Goal

Provide reusable cache helpers for fonts, images, and external CSS. Many users will otherwise reimplement the same `resolveResource` wrapper.

### Proposed API

```ts
const cache = createMemoryResourceCache();

await render({
  value: html,
  width: 1200,
  resolveResource: cache.wrap(),
});
```

Additional helpers:

```ts
createMemoryResourceCache(options?: { maxEntries?: number; maxBytes?: number });
createCacheStorageResourceCache(name?: string);
createFileResourceCache(path: string);
```

### Environment Support

- Memory cache: browser, Node.js, Deno, Workers.
- CacheStorage cache: browser and Workers.
- File cache: Node.js only.

### Implementation Notes

- Cache by final resolved URL and resource type.
- Avoid caching failed responses unless explicitly configured.
- Consider cache metadata for content type, byte length, and created time.
- Keep helpers optional and tree-shakeable.

### Likely Files

- `packages/satoru/src/core.ts`
- `packages/satoru/src/node.ts`
- `packages/satoru/src/workerd.ts`
- New file: `packages/satoru/src/cache.ts`

### Acceptance Criteria

- Memory cache avoids repeated fetches for the same resource.
- Cache helpers compose with custom `resolveResource`.
- Node-only file cache is not imported by browser/workerd entrypoints.
- README includes one browser/Workers example and one Node example.

## P2: PDF Document Options

### Goal

Make PDF generation usable for documents and reports, not only page snapshots.

### Proposed Options

```ts
pdf?: {
  pageSize?: "A4" | "A5" | "Letter" | { width: number; height: number };
  margin?: number | { top: number; right: number; bottom: number; left: number };
  header?: string | ((page: number, total: number) => string);
  footer?: string | ((page: number, total: number) => string);
  pageNumbers?: boolean;
  split?: "none" | "css-page-break" | "auto";
};
```

### Implementation Notes

- Start with page size and margins before dynamic headers/footers.
- Respect `@media print`.
- Support existing array-of-HTML-pages behavior.
- Consider CSS `break-before`, `break-after`, and `break-inside` as a later phase.

### Likely Files

- `packages/satoru/src/core.ts`
- `src/cpp/renderers/pdf_renderer.cpp`
- `src/cpp/renderers/pdf_renderer.h`
- `src/cpp/api/satoru_api.cpp`

### Acceptance Criteria

- A4 and Letter presets render correctly.
- Margins affect content placement predictably.
- Existing multi-page PDF API continues to work.
- At least one visual or binary regression test covers multi-page output.

## P3: Compatibility Matrix Generation

### Goal

Generate a public compatibility table from actual test assets and visual test results.

### Proposed Output

Create or update:

- `docs/compatibility.md`

The table should list:

- Feature group.
- CSS properties or functions covered.
- Test asset.
- PNG status.
- SVG status.
- PDF status.
- Notes or known caveats.

### Implementation Notes

- Add metadata comments to `assets/*.html`, for example:

```html
<!--
compat:
  group: Layout
  features:
    - display: grid
    - gap
-->
```

- Parse metadata in a script.
- Combine metadata with visual-test baseline files.
- Make the generated doc deterministic.

### Likely Files

- `assets/*.html`
- `packages/visual-test/test/data/*.json`
- New script: `packages/visual-test/tools/generate-compatibility.ts`
- New doc: `docs/compatibility.md`

### Acceptance Criteria

- Compatibility doc can be regenerated with one command.
- Generated output is deterministic.
- Each existing asset can optionally map to one or more feature groups.
- Missing metadata is reported but does not fail the script at first.

## CSS Engine Backlog

These items are more engine-specific and should be prioritized after diagnostics and workflow improvements unless a user issue requires them sooner.

### Selector and Pseudo Support

- Investigate support for additional pseudo-elements such as `::first-line` and `::first-letter`.
- Add diagnostics for unsupported pseudo-classes or invalid selectors.
- Improve user-facing reporting for ignored selector blocks.

### Inline Overflow

- Complete overflow handling for inline elements.
- Add visual cases for inline clipping, ellipsis, nested inline boxes, and text decorations.

### Table Layout Edge Cases

- Improve table-caption and table-column behavior.
- Add regression assets for complex table structures.

### Radial Gradient Syntax

- Review CSS Images Level 4 radial-size syntax.
- Add tests for newer radial-gradient shape and size combinations.

### Flex Logical Min/Max

- Finish min/max block-size checks in flex layout using logical properties.
- Add tests for vertical writing modes and flex constraints.

## Suggested First Milestone

The first milestone should be:

1. Add `RenderDiagnostics` TypeScript types.
2. Collect resource and font diagnostics in `SatoruBase.render`.
3. Expose missing font data from C++ if needed.
4. Add `--json-report` to the CLI.
5. Add one visual-test fixture with a missing image and one with a missing font.
6. Document the diagnostics feature in `packages/satoru/README.md`.

This milestone is deliberately small enough to ship, but it creates the foundation for the Playground diff viewer, better CLI workflows, and compatibility documentation.
