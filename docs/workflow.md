# Development & Verification Workflow

## 4.1 Build Commands

- `pnpm wasm:configure`: Configure CMake with Ninja.
- `pnpm wasm:build`: Compile C++ to Wasm (uses Thin LTO and -O3 in production).
- `pnpm build`: Full monorepo build.

## 4.2 Verification Protocol

1. **Build Check**: Ensure Wasm compilation success.
2. **Visual Audit**: Run `pnpm --filter visual-test convert-assets` and inspect outputs in `temp/`.
3. **Log Analysis**: Use `--verbose` to inspect layout passes and cache hits/misses.
