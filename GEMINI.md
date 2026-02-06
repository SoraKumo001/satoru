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
- Do not rely on cached content or previous reads if any other file operations have occurred.

#### Rule 2: Bottom-to-Top Patching
- **CRITICAL:** When applying multiple patches to the same file in a single `edit_text_file_contents` call, you **MUST** order the patches from the bottom of the file to the top (descending line numbers).
- This ensures that line number shifts caused by adding or removing lines in one patch do not invalidate the `line_start`/`line_end` of subsequent patches.

#### Rule 3: Precise Range Matching
- The `line_start` and `line_end` in your patch must exactly match the range you read.
- The `range_hash` must correspond to the content of that exact range at the time of reading.

#### Rule 4: Atomic Operations
- Prefer small, focused patches over large, sweeping changes. This minimizes the risk of conflicts and makes the editing process more robust.

### 3. Troubleshooting

- If a `Hash Mismatch` error occurs:
    1. **Stop** the current operation.
    2. **Re-read** the file ranges using `get_text_file_contents` to get fresh hashes.
    3. **Verify** the content matches your expectations.
    4. **Retry** the edit with the new hashes.

## Project Context: Satoru

### 1. Overview

**Satoru** is a high-fidelity HTML/CSS to SVG/PNG converter running in WebAssembly.
- **Core:** `litehtml` (Layout) + `Skia` (Rendering).
- **Target:** WASM via Emscripten.

### 2. Build System

Use the TypeScript-based build scripts defined in `package.json`:
- `npm run wasm:configure`: Configure CMake for WASM build.
- `npm run wasm:build`: Compile C++ to WASM (`public/satoru.js`, `public/satoru.wasm`).
- `npm run build`: TS/Vite build for the web frontend.

**Requirements:** `EMSDK` and `VCPKG_ROOT` environment variables must be set.

### 3. Testing & Development

- `npm test`: Runs `test/convert_assets.ts` using `tsx`. Converts `public/assets/*.html` to `temp/`.
- `npm run dev`: Starts Vite dev server for web-based preview/comparison.
- **Logs:** C++ `printf` is bridged to JS `console.log`. Ensure `\n` or `fflush(stdout)` is used in C++.

### 4. Project Structure & Key Files

- `src/cpp/`: C++ Engine implementation.
- `src/satoru/index.ts`: High-level TypeScript wrapper (`Satoru` class). **Always use this wrapper in JS/TS environments.**
- `public/assets/`: Source HTML test cases (Git tracked).
- `public/satoru.*`: Generated WASM artifacts (Git ignored).
- `.vscode/c_cpp_properties.json`: Cross-platform C++ IntelliSense using `${env:EMSDK}`.

### 5. Implementation Details

- **Binary Transfer:** PNG data is transferred via a shared buffer. Use `Satoru.toPngBinary()` which handles memory pointers and slicing.
- **SVG Post-Processing:** SVG output uses Skia's `SkSVGCanvas`. Advanced effects (Shadows/Conics) are post-processed in C++ using specific color tags.
- **Text:** `litehtml` handles layout; custom `draw_text` in Skia handles rendering including Japanese fonts and text decorations.
- **Images:** Decoded by the JS host and passed as Data URLs. Skia's PNG decoder must be registered in WASM.
