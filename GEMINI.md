# GEMINI.md

## System Context & Operational Guidelines

This document defines the operational rules for Gemini when interacting with the local file system and shell environment.

### 1. Shell Environment: Windows PowerShell

You are operating in a **Windows PowerShell** environment.

- **Constraint:** PowerShell does not support `&&` or `||` for command chaining.
- **Instruction:** Use the semicolon `;` separator for sequential execution.
- ✅ **Correct:** `mkdir test_dir ; cd test_dir`

### 2. File Editing Protocol (mcp-text-editor)

When using `get_text_file_contents` and `edit_text_file_contents`, strictly follow these rules to ensure data integrity and avoid `Hash Mismatch` errors.

#### Rule 1: Synchronized Reading
- Always use `get_text_file_contents` to obtain the current `file_hash` and `range_hash` for the specific lines you intend to edit.

#### Rule 2: Bottom-to-Top Patching
- **CRITICAL:** When applying multiple patches to the same file in a single `edit_text_file_contents` call, you **MUST** order the patches from the bottom of the file to the top (descending line numbers).

#### Rule 3: Avoid Double Escaping
- **CRITICAL:** When providing content for `edit_text_file_contents` or `write_file`, ensure that newlines and quotes are not double-escaped (e.g., `\n` or `\"`).

### 3. Troubleshooting

- If a `Hash Mismatch` error occurs: Re-read the file ranges and retry the edit with fresh hashes. If the error persists or becomes recurrent, rewrite the entire file using `write_file` to ensure consistency.

## Project Context: Satoru

### 1. Overview

**Satoru** is a high-fidelity HTML/CSS to SVG/PNG converter running in WebAssembly.
- **Monorepo:** Organized as pnpm workspaces.
- **Core:** `litehtml` (Layout) + `Skia` (Rendering).
- **Target:** WASM via Emscripten.

### 2. Build System

- `pnpm wasm:configure`: Configure CMake for WASM build.
- `pnpm wasm:build`: Compile C++ to WASM (`packages/satoru/dist/satoru.*`).
- `pnpm build`: Build all package wrappers.

### 3. Implementation Details

- **SVG Rendering (2-Pass):**
    1. **Pass 1 (Measurement):** Layout with a dummy container to determine exact content height.
    2. **Pass 2 (Drawing):** Render to `SkSVGCanvas` with the calculated dimensions.
- **Tag-Based Post-Processing:**
    - Advanced effects (Shadows, Images, Conics) are \"tagged\" during drawing with unique colors (e.g., `rgb(0,1,index)` for shadows).
    - The resulting SVG string is processed via **Regular Expressions** to replace these tagged elements with real SVG filters or `<image>` tags.
- **Box Shadow Logic:**
    - **Outer Shadow:** Uses `feGaussianBlur` + `feOffset` + `feFlood`.
    - **Inset Shadow:** Uses `feComposite` with `operator=\"out\"` to invert the shape, then `blur/offset`, and finally `operator=\"in\"` to clip the shadow within the original shape.
    - **Alpha Reference:** Tagged elements must maintain a fill (e.g., `fill=\"black\"`) so `SourceAlpha` works for filters, but `feMerge` controls visibility.
- **Font Handling:**
    - **2-Pass Loading:** Missing fonts are detected during layout and requested from the JS host.
    - **Metadata Inference:** The TS host infers `weight` and `style` from font URLs to generate correct `@font-face` blocks for WASM.
- **litehtml Types:**
    - Container overrides MUST return `litehtml::pixel_t` (float), not `int`, to match base class signatures.

### 4. Skia API & Release Notes

- **SkPath Immutability:** Use `SkPathBuilder` instead of direct `SkPath` modification methods.
- **Radial Gradients:** Circular by default. For **Elliptical** gradients, apply an `SkMatrix` scale transform (e.g., `ry/rx` on Y-axis) to the shader.
- **C++ Logs:** Bridge `printf` to JS `console.log`. Ensure `\\n` or `fflush(stdout)` is used.

### 5. Testing & Validation

- **Visual Regression Suite (`packages/test-visual`)**:
    - **Dual Validation**: Compares **Direct Skia PNG** and **SVG-rendered PNG** (via Playwright) against reference images.
    - **Numerical Diff**: Reports pixel difference percentage for both paths.
    - **Stabilization**: Uses `flattenAlpha` (blending with white) and white-pixel padding to handle dimension mismatches and transparency flakiness.
- **Output Validation**:
    - **Crucial**: To verify generated output files (PNG/SVG) for all assets, always use the `convert-assets` command.
    - Command: `pnpm --filter test-visual convert-assets`
    - Output: Files are generated in `packages/test-visual/temp/`. Use these files to manually inspect the rendering quality and correctness.
- **Performance Optimizations**:
    - **Reference Generation (`tools/generate-reference.ts`)**: Uses Playwright Concurrency (Batch size/Concurrency: 4) to speed up reference image capture.
    - **Multi-threaded Asset Conversion (`tools/convert_assets.ts`)**: Uses `worker_threads` to spawn multiple WASM instances (one per core). Each worker carries its own WASM context for parallel conversion.
- **Tooling Paths**:
    - Reference generation: `packages/test-visual/tools/generate-reference.ts`.
    - Conversion worker: `packages/test-visual/tools/convert_worker.ts`.
    - Assets: `assets/*.html` (at project root).

### 6. GitHub Pages Deployment

To ensure the web-based test environments (e.g., `test-web`) work correctly on GitHub Pages (which often hosts projects in subdirectories):

- **Relative Paths in HTML**: Always use relative paths for scripts and links (e.g., `src/main.ts` instead of `/src/main.ts`).
- **Vite Configuration**: Set `base: \"./\"` in `vite.config.ts` to allow the application to be served from any base path.
- **Asset Resolution**: In TypeScript, resolve asset URLs relative to the deployment root (e.g., `assets/file.html`) rather than using local development relative paths (e.g., `../../assets/`).
- **Artifact Management**: Ensure Wasm (`satoru.wasm`) and JS (`satoru.js`) artifacts are copied to the `dist` directory during the build process, as they are typically located in the workspace's shared output directory.
