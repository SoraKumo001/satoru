# GEMINI.md

## System Context & Operational Guidelines

This document defines the operational rules for Gemini when interacting with the local file system and shell environment.

### 1. Shell Environment: Windows PowerShell

You are operating in a **Windows PowerShell** environment (likely v5.1).

- **Constraint:** The standard PowerShell environment does not support the `&&` or `||` operators for command chaining.
- **Instruction:** You **MUST** use the semicolon `;` separator for sequential command execution.
- ❌ **Incorrect:** `mkdir test_dir && cd test_dir`
- ✅ **Correct:** `mkdir test_dir ; cd test_dir`

- **Alternative:** If strict error handling is needed (stop on failure), execute commands one by one or use `if ($?) { ... }`.

### 2. File Editing Protocol (mcp-text-editor)

To prevent `Hash Mismatch` errors when using `edit_file` or `write_file`, strictly adhere to the following sequence:

- **Rule 1: Read Before Write**
- ALWAYS execute `read_file` on the target file immediately before attempting an edit.
- Do not rely on your context window for file content; it may be outdated or hallucinated (especially whitespace).

- **Rule 2: Byte-Perfect `old_text**`
- When using `edit_file` (search & replace), the `old_text` field must be a **byte-for-byte exact match** of the file content.
- **Whitespace:** Do NOT change indentation (tabs vs spaces) or trim trailing spaces.
- **Line Endings:** Respect the existing line endings (CRLF vs LF). Do not normalize them in the `old_text`.

- **Rule 3: Atomic Edits**
- Prefer replacing smaller, unique blocks of code (e.g., a single function signature or a specific logic block) rather than large chunks. This reduces the risk of hash collisions.

### 3. Troubleshooting

- If you encounter a `Hash Mismatch` error:

1. Stop the current chain.
2. Re-read the file using `get_text_file_contents`.
3. Verify the exact content of the lines you intend to change.
4. Retry the edit with the newly confirmed content.

## Project Context: Satoru

### 1. Overview

**Satoru** is an HTML/CSS to SVG converter running in WebAssembly (WASM). It leverages `litehtml` for layout/rendering and `Skia` for drawing, outputting an SVG string.

### 2. Build System

- **Command:** `npm run cmake-build`
- **Mechanism:** Uses `scripts/build-wasm.ts` (executed via `tsx`) to handle build configuration and execution.
  - **Do NOT** write raw shell scripts for building; rely on this TypeScript abstraction to ensure cross-platform compatibility (Windows/Linux/macOS).
- **Environment:** Requires `EMSDK` (Emscripten) and `VCPKG_ROOT` environment variables.

### 3. Testing & Development

- **Test:** `npm test` converts HTML assets in `public/assets/` to SVGs in `temp/`.
- **UI:** `index.html` / `src/main.ts` provides a split-view comparison.
- **Logging:** C++ `printf` / `std::cout` is bridged to Node.js/Console via Emscripten's `print` hook. Use `fflush(stdout)` or `\n` to flush.

### 4. Implementation Details

- **Images:** Decoded in JavaScript/TypeScript host and passed to WASM as Data URLs via a preloading cache. WASM does not fetch images directly.
- **SVG Output:** Skia's `SkSVGCanvas` is used. We filter out empty paths (`<path d="" />`) in `main.cpp` to reduce bloat.
- **Gradients:**
  - **Linear/Radial:** Implemented via standard Skia shaders.
  - **Conic:** Implemented via `SkShaders::SweepGradient` with rotation adjustments.
  - **Elliptical Radial:** Handled via `SkMatrix` scaling on a unit circle gradient.
- **Text Decoration:** Custom handling in `draw_text` to support `dotted`, `dashed`, `wavy` (TODO), and correct skipping of whitespace for continuous lines.
