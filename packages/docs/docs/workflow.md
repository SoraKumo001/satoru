---
sidebar_position: 4
title: 開発ワークフロー
---

# Development & Verification Workflow

## 4.1 Build Commands

- `pnpm wasm:configure`: Configure CMake with Ninja.
- `pnpm wasm:build`: Compile C++ to Wasm (uses Thin LTO and -O3 in production).
- `pnpm build`: Full monorepo build.

## 4.2 Verification Protocol

1. **Build Check**: Ensure Wasm compilation success.
2. **Visual Audit**: Run `pnpm --filter visual-test convert-assets` and inspect outputs in `temp/`.
3. **Log Analysis**: Use `--verbose` to inspect layout passes and cache hits/misses.

## 4.3 Release Verification & Publish Workflow

To ensure publish safety and API consistency, follow this protocol before making a release:

1. **Local Release Verification**: Run the automated release check script:
   ```bash
   pnpm release:check
   ```
   This script performs the following tasks:
   - Verifies version consistency between `packages/satoru/package.json` and `packages/satoru/CHANGELOG.md`.
   - Runs a full clean build (`pnpm build`).
   - Verifies that all exported entrypoints and files (such as README, LICENSE, WASM binary, worker bundles) exist.
   - Runs the visual regression test suite.
   - Performs a package dry-run pack (`pnpm pack --dry-run`).

2. **Compatibility Evidence Documentation**:
   Ensure the visual capability matrix documentation is updated. The CI validates this by checking for uncommitted changes. You can regenerate it locally via:
   ```bash
   pnpm docs:compatibility
   ```
   Always commit any updates to `docs/compatibility.md` prior to opening a release PR.

3. **Worker Pool Lifecycle Management**:
   When using `createSatoruWorker` for high-throughput renderings, understand these lifecycle controls:
   - `waitAll()`: Returns a promise resolving only when all dispatched render jobs are finished. Useful to block script termination until completion.
   - `close()`: Instantly terminates all worker threads/processes. Always call this during cleanup to prevent memory leaks or hanging processes.
   - `reset()`: Recycles the worker pool by terminating current workers and spinning up new ones. Automatically triggered in case of per-job timeouts to recover from native execution hangs.
