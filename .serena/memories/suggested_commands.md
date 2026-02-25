# Suggested Commands for Satoru

## Build Commands
- `pnpm wasm:configure`: Configure CMake for WebAssembly build using Ninja.
- `pnpm wasm:build`: Compile C++ code to WebAssembly.
- `pnpm build`: Perform a full monorepo build (Wasm + TS).

## Quality & Maintenance
- `pnpm format`: Format C++ files using `clang-format` and TS files.
- `pnpm test:visual`: Run visual regression tests to verify rendering accuracy.

## Development & Debugging
- `pnpm --filter visual-test convert-assets:log`: Run asset conversion with verbose logging to inspect layout/rendering steps.
- `pnpm dev:playground`: Start the web-based playground for real-time testing.

## System Utilities (Windows)
- `Get-ChildItem`: List files/directories (similar to `ls`).
- `Select-String`: Search for patterns in files (similar to `grep`).
- `git status`, `git diff`: Gather git context.
