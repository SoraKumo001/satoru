# Satoru Project Onboarding

## Purpose
Satoru is a high-fidelity HTML/CSS to SVG/PNG/PDF/WebP converter running in WebAssembly (Emscripten). It uses `litehtml` for layout and `Skia` for rendering.

## Tech Stack
- **C++**: Core engine (`src/cpp`).
- **WebAssembly**: Compiled using Emscripten.
- **TypeScript/Node.js**: Wrappers, CLI, and build scripts.
- **PNPM**: Monorepo management.
- **LiteHTML**: HTML/CSS layout engine.
- **Skia**: Graphics engine for rendering.
- **Vitest/Playwright**: Visual regression testing.

## Codebase Structure
- `src/cpp/api`: Emscripten API interface.
- `src/cpp/core`: Core logic (fonts, resources, context).
- `src/cpp/core/text`: Advanced text stack (Unicode, Layout, Rendering).
- `src/cpp/libs`: Vendor libraries (`litehtml`, `skia`).
- `src/cpp/renderers`: Specific output backend (PNG, SVG, PDF, WebP).
- `packages/satoru`: TS core library.
- `packages/visual-test`: Visual regression tests.
- `scripts`: Utility scripts for building and formatting.

## Style and Conventions
- **C++**: Follows `.clang-format` (LLVM-like).
- **TypeScript**: Uses standard TypeScript conventions with `tsconfig.json`.
- **Naming**: C++ uses `snake_case` for functions/variables and `PascalCase` for classes. TS follows standard camelCase/PascalCase.

## Development Workflow
1. **Configure Wasm**: `pnpm wasm:configure`
2. **Build Wasm**: `pnpm wasm:build`
3. **Build JS**: `pnpm build`
4. **Format**: `pnpm format`
5. **Test**: `pnpm test:visual` (runs visual tests)
