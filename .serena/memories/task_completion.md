# Task Completion & Verification Protocol

Before considering a task complete, follow these steps:

1. **Build Check**: Ensure `pnpm wasm:build` and `pnpm build` complete without errors if code was changed.
2. **Verification**:
   - For rendering changes: Use `pnpm --filter visual-test convert-assets` on relevant HTML files.
   - Inspect SVG output for correct structure if applicable.
   - Perform a visual audit of generated images in `packages/visual-test/temp`.
3. **Logs**: Use `--verbose` with `convert-assets` to check `satoru_log` outputs for internal state consistency.
4. **Standards**: Run `pnpm format` to ensure code style compliance.
5. **Tests**: Run `pnpm --filter visual-test test` to ensure no regressions were introduced.
