# Post-Task Checklist

Before considering a task complete, ensure the following:

1. **Build Success**:
   - Run `pnpm wasm:build` if any C++ code was changed.
   - Run `pnpm build` to ensure TS wrappers and packages are still valid.
2. **Formatting**:
   - Run `pnpm format` to ensure code style consistency.
3. **Verification**:
   - Run `pnpm test:visual` if the changes affect rendering or layout.
   - For C++ changes, check the build logs for any new warnings.
4. **Documentation**:
   - Update `GEMINI.md` or relevant READMEs if new features were added or architectural changes made.
