# Suggested Commands

## Build
- `pnpm wasm:configure`: Configure CMake with Ninja.
- `pnpm wasm:build`: Compile C++ to WASM.
- `pnpm build`: Full build of both WASM and TS wrappers.

## Testing & Verification
- `pnpm --filter visual-test test`: Run visual regression tests.
- `pnpm --filter visual-test convert-assets [file.html]`: Convert a specific asset for inspection.
- `pnpm --filter visual-test convert-assets:log [file.html]`: Convert with verbose logging.

## Maintenance
- `pnpm format`: Run clang-format and Prettier across the codebase.

## Development
- `pnpm dev:playground`: Start the playground development server.
- `pnpm dev:cloudflare`: Start the Cloudflare OGP development server.
