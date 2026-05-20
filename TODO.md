# Satoru Next TODO

The diagnostics, Playground inspection, visual registry, benchmark, PDF workflow, and compatibility backlog work has been completed. This document now tracks the next phase: making Satoru easier to ship, safer to operate, simpler to integrate, and more measurable over time.

## Priority Overview

| Priority | Feature | Main Value | Suggested Scope |
| --- | --- | --- | --- |
| P0 | Release automation and publish safety | Makes releases repeatable and low-risk | scripts, package metadata, changelog, CI |
| P0 | Public compatibility evidence | Turns test coverage into user-facing trust | docs, generated reports, examples |
| P1 | Resource resolver and cache adapters | Reduces production boilerplate | TS helpers, Node/Workers/browser adapters |
| P1 | Render limits and safety controls | Prevents runaway or unsafe renders | API options, resource loading, CLI |
| P2 | Worker pool observability and control | Improves high-throughput deployments | workers API, diagnostics, queue metrics |
| P2 | Advanced PDF features | Expands document/report use cases | PDF renderer, TS API, print fixtures |
| P3 | API ergonomics and examples | Makes adoption easier | examples, framework recipes, docs |
| P3 | Next CSS fidelity backlog | Continues renderer compatibility gains | litehtml customizations, visual fixtures |

## P0: Release Automation and Publish Safety [COMPLETE]

### Goal

Make releases predictable. A maintainer should be able to run one command that verifies the package, validates generated artifacts, and prepares a publish-ready release without relying on memory.

### Tasks

- [x] Add a release check script that runs build, tests, visual checks, benchmark smoke tests, and package validation.
- [x] Add `npm pack --dry-run` validation for `packages/satoru`.
- [x] Verify `dist` contains every exported entrypoint declared in `package.json`.
- [x] Verify published files include README, LICENSE, changelog, WASM, worker bundles, and type declarations.
- [x] Add a script that fails when package version and changelog version disagree.
- [x] Add a script that prints release notes from the latest changelog section.
- [x] Add CI workflow steps for release checks.
- [x] Document the release process in `docs/workflow.md`.

### Acceptance Criteria

- One command can validate release readiness locally.
- The release script fails on missing export files.
- The package dry run output is captured or summarized.
- Release instructions are documented and current.

## P0: Public Compatibility Evidence [COMPLETE]

### Goal

Convert internal visual coverage into public evidence. Users should be able to see what Satoru supports, which fixtures prove it, and where known caveats remain.

### Tasks

- [x] Generate `docs/compatibility.md` from the visual test registry.
- [x] Include feature group, CSS feature, asset file, output formats, status, and notes.
- [x] Link each compatibility row to the relevant fixture.
- [x] Add a summary section with supported output formats and tested environments.
- [x] Add a known caveats section for partial support or format-specific differences.
- [x] Add a README link to the compatibility document.
- [x] Add CI verification that generated compatibility docs are up to date.

### Acceptance Criteria

- Compatibility docs can be regenerated deterministically.
- Every primary asset has metadata or is explicitly marked as uncategorized.
- README points users to the generated compatibility matrix.
- Known caveats are visible instead of hidden in tests.

## P1: Resource Resolver and Cache Adapters [COMPLETE]

### Goal

Make production resource loading easier and safer. Users should not need to rewrite the same font/image/CSS caching and resolver wrappers for every deployment target.

### Proposed API

```ts
import {
  MemoryResourceCache,
  CacheStorageResourceCache,
  composeResourceResolvers,
} from "satoru-render/resources";
```

### Tasks

- [x] Add a tree-shakeable resource helper entrypoint.
- [x] Implement memory cache for browser, Node.js, Deno, and Workers.
- [x] Implement CacheStorage adapter for browser and Workers.
- [x] Implement Node file-system cache without importing Node APIs from browser/workerd bundles.
- [x] Add resolver composition helpers.
- [x] Add cache metadata: URL, resource type, byte length, created time, last hit time.
- [x] Add options for max entries, max bytes, TTL, and whether failed fetches are cached.
- [x] Document examples for Node, Cloudflare Workers, and browser usage.

### Acceptance Criteria

- Cache helpers work with the existing `resolveResource` API.
- Browser/workerd bundles do not import Node-only modules.
- Repeated renders avoid duplicate fetches when cache is enabled.
- Cache behavior is covered by tests.

## P1: Render Limits and Safety Controls [COMPLETE]

### Goal

Give production users explicit controls for network access, resource size, render duration, and memory-sensitive inputs. This matters for server-side rendering and public OGP endpoints.

### Tasks

- [x] Add timeout handling for resource resolution and render phases where practical.
- [x] Add max single-resource byte limit.
- [x] Add max total resource byte limit.
- [x] Add max resource count limit.
- [x] Add protocol and host allow/block controls for default resolver.
- [x] Ensure blocked resources appear in diagnostics with stable codes.
- [x] Add CLI options for common limits.
- [x] Document SSRF and untrusted HTML guidance.

### Acceptance Criteria

- Limits fail with clear errors and diagnostics.
- Default behavior remains backward-compatible.
- CLI exposes the most important safety controls.
- Tests cover blocked protocol, blocked host, oversized resource, and timeout cases.

## P2: Worker Pool Observability and Control [COMPLETE]

### Goal

Improve throughput-oriented deployments that use `createSatoruWorker`. Users should be able to see queue health, cancel work, and shut down cleanly.

### Tasks

- [x] Expose worker pool stats: active jobs, queued jobs, completed jobs, failed jobs, average render time.
- [x] Add cancellation support via `reset()` (terminates all workers).
- [x] Add optional per-job timeout in worker pool specifically (currently relies on global render timeout).
- [x] Add `onWorkerLog` or structured worker diagnostics events.
- [x] Improve `waitAll` and `close` behavior documentation.
- [x] Add tests for worker stats and reset.
- [x] Add a high-throughput example.

### Acceptance Criteria

- Worker stats can be read without disrupting active renders.
- `reset()` successfully clears queue and restarts workers.
- Shutdown does not leave unresolved jobs hanging.
- Documentation explains recommended pool sizing.

## P2: Advanced PDF Features [COMPLETE]

### Goal

Continue growing PDF from page snapshots into practical document generation.

### Tasks

- [x] Add optional PDF metadata: title, author, subject, keywords, creator.
- [x] Add header and footer templates with `{{pageNumber}}` and `{{totalPages}}`.
- [x] Add PDF margins support.
- [x] Add CSS page break fixture coverage (currently requires manual splitting).
- [ ] Investigate bookmarks/outlines for multi-section documents.
- [ ] Investigate link annotations for anchors and external links.
- [x] Document limitations compared with a browser print engine.

### Acceptance Criteria

- Existing PDF behavior remains compatible.
- Metadata is embedded when provided.
- Header/footer output is deterministic and supports page numbers.
- Multi-page fixtures cover page numbering and margins.

## P3: API Ergonomics and Examples

### Goal

Make common adoption paths obvious.

### Tasks

- Add examples for Next.js, Cloudflare Workers, Deno Deploy, Node batch rendering, and browser capture.
- Add a recipe for React/Preact + Tailwind rendering.
- Add a recipe for OGP generation with caching and limits.
- Add a recipe for PDF reports.
- Add a troubleshooting page based on diagnostics codes.
- Add minimal TypeScript snippets that can be copied directly.

### Acceptance Criteria

- Each major environment has one runnable or near-runnable example.
- Diagnostics troubleshooting references stable diagnostic codes.
- README stays concise and links to deeper docs instead of becoming too large.

## P3: Next CSS Fidelity Backlog

### At-Rules and Selectors

- Investigate `@supports`.
- Improve unsupported selector diagnostics.
- Add fixtures for nested at-rules and unsupported selector recovery.

### Typography

- Investigate variable font axes.
- Improve color emoji and color font behavior across output formats.
- Add fixtures for mixed CJK, emoji, BiDi, and vertical writing combinations.

### SVG and Effects

- Add more SVG filter fixtures.
- Improve parity for SVG masks, clips, gradients, and nested transforms.
- Add format-specific caveats where SVG output differs from raster output.

### Form and Replaced Elements

- Add fixtures for inputs, textarea, select, checkbox, radio, canvas capture, and video fallback.
- Decide which form controls should be styled approximations versus unsupported.

### Print and Paged Media

- Investigate `@page`.
- Add fixtures for print margins, named pages, and page breaks.
- Clarify which paged-media features are supported by Satoru's PDF workflow.

## Suggested Next Milestone

The next milestone should focus on Performance, API Ergonomics, and CSS Fidelity:

1. Add high-throughput worker pool example with a real-world multi-worker deployment guide.
2. Implement PDF bookmarks/outlines support in WASM.
3. Improve CSS Fidelity: Investigate color fonts (COLRv1) and variable font axes.
4. Add troubleshooting guide based on stable diagnostic codes.
5. Implement a "Fast Path" for SVG-only renders if possible.
