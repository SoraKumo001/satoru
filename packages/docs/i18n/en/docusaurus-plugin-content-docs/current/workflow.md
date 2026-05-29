---
sidebar_position: 5
title: Development Workflow
---

# Development and Verification Workflow

## 4.1 Build Commands

- `pnpm wasm:configure`: Configure CMake with Ninja.
- `pnpm wasm:build`: Compile C++ to Wasm. Production builds use Thin LTO and `-O3`.
- `pnpm build`: Build the full monorepo.

## 4.2 Verification Protocol

1. **Build check**: Ensure Wasm compilation succeeds.
2. **Visual audit**: Run `pnpm --filter visual-test convert-assets` and inspect outputs in `temp/`.
3. **Log analysis**: Use `--verbose` to inspect layout passes and cache hits/misses.

## 4.3 Release Verification and Publish Workflow

To keep publishing safe and APIs consistent, follow this protocol before a release:

1. **Local release verification**: Run the automated release check script.
   ```bash
   pnpm release:check
   ```
   This script checks:
   - Version consistency between `packages/satoru/package.json` and `packages/satoru/CHANGELOG.md`.
   - A full clean build (`pnpm build`).
   - Exported entrypoints and required files such as README, LICENSE, WASM binary, and worker bundles.
   - The visual regression test suite.
   - A package dry-run pack (`pnpm pack --dry-run`).

2. **Compatibility evidence documentation**:
   Make sure the visual capability matrix is up to date. CI validates this by checking for uncommitted changes. Regenerate it locally with:
   ```bash
   pnpm docs:compatibility
   ```
   Always commit updates to `docs/compatibility.md` before opening a release PR.

3. **Worker pool lifecycle management**:
   When using `createSatoruWorker` for high-throughput rendering, understand these lifecycle controls:
   - `waitAll()`: Returns a promise that resolves only after all dispatched render jobs finish.
   - `close()`: Immediately terminates all worker threads/processes. Call this during cleanup to avoid leaks or hanging processes.
   - `reset()`: Recycles the worker pool by terminating current workers and starting new ones. It is triggered automatically for per-job timeouts to recover from native execution hangs.
