# GEMINI.md

## 1. Operational Guidelines (Agent Specific)

This section defines the core rules for an agent's behavior within this project.

### 1.1 Shell Environment: Windows PowerShell
- **Command Chaining**: Use `;` for sequential execution (do NOT use `&&` or `||`).
- **Constraint**: Do not execute multiple shell commands in parallel.

### 1.2 Communication & Troubleshooting
- **Progress Reporting**: Always report your current status in the user's language concisely before and during each step.
- **Hash Mismatch**: If encountered, immediately re-read the file with `get_text_file_contents` to synchronize. If the issue persists, overwrite the entire file using `write_file`.

### 1.3 Git Usage
- **No Write Operations**: Do not perform `git add`, `commit`, `push`, etc., unless explicitly and specifically instructed by the user.
- **Read-Only Access**: `status`, `diff`, and `log` are permitted and recommended for gathering context.

---

## 2. Project Overview: Satoru

A high-fidelity HTML/CSS to SVG/PNG/PDF converter running in WebAssembly (Emscripten).

- **Core Technologies**: `litehtml` (Layout) + `Skia` (Rendering) + `spdlog` (Logging).
- **Structure**: pnpm workspaces monorepo.
  - `packages/satoru`: Core library & TS wrappers.
  - `packages/visual-test`: Visual regression testing suite.
  - `packages/playground`: Web-based demo application.
  - `packages/cloudflare-ogp`: OGP image generation using Cloudflare Workers.

---

## 3. Technical Specifications & Architecture

### 3.1 Directory Structure
- `src/cpp/api`: Emscripten API implementation (`satoru_api.cpp`).
- `src/cpp/core`: Layout/Rendering core, resource management, and master CSS.
- `src/cpp/core/text`: Modular text processing stack.
  - `UnicodeService`: Encapsulates Unicode logic (utf8proc, libunibreak, BiDi).
  - `TextLayout`: High-level layout logic (measurement, ellipsizing, splitting).
  - `TextRenderer`: Skia-based rendering, decorations, and tagging.
- `src/cpp/renderers`: PNG, SVG, PDF, and WebP specific renderers.
- `src/cpp/utils`: Skia utilities, Base64 helpers, SkUnicode implementation, and logging macros (`logging.h`).
- `src/cpp/libs`: External libraries (`litehtml`, `skia`).

### 3.2 Rendering Pipelines
- **SVG (2-Pass)**: Measurement -> Drawing to `SkSVGCanvas` -> Regex Post-Processing.
  - **Text Handling**: Defaults to retaining `<text>` elements (`textToPaths: false`). Use `--outline` in CLI to force paths.
- **PNG/WebP**: Render to `SkSurface` -> `SkImage` -> Encode.
- **PDF (2-Pass)**: Measurement -> Drawing to `SkPDFDocument`.

### 3.3 CSS Engine (litehtml Customizations)
- **Cascade Priority**: Correct W3C order (`Important` > `Layer` > `Specificity`).
- **International Text**: Integrated `HarfBuzz`, `libunibreak`, and `utf8proc` for complex shaping and line breaking.
- **Decoration**: Supports `wavy` style for underlines using `SkPathBuilder` and `quadTo` curves.

### 3.4 Logging Infrastructure
- **Integration**: `spdlog` is used as the primary logging engine.
- **Custom Sink**: `emscripten_sink` bridges `spdlog` logs to the JS `onLog` callback.
- **Macros**: Use `SATORU_LOG_INFO`, `SATORU_LOG_ERROR`, etc., from `utils/logging.h` for formatted output.
- **Control**: Log levels are controllable from JS via `Satoru.render({ logLevel: ... })` or `api_set_log_level`.

### 3.5 Resource Management
- **Resolution**: `render` options support `resolveResource` callback.
- **Default Resolver**: The JS wrapper provides a default resource resolver that handles local file system (Node.js) and HTTP `fetch`.
- **Interception**: Users can intercept resource requests and fallback to the default resolver via the second argument of the callback.

---

## 4. Development & Verification Workflow

### 4.1 Build Commands
- `pnpm wasm:configure`: Configure CMake with Ninja generator.
- `pnpm wasm:build`: Compile C++ to Wasm.
- `pnpm build`: Full build of both WASM and TS wrappers.

### 4.2 Verification Protocol (For Features & Fixes)
1.  **Build Check**: Verify `pnpm wasm:build` completes without errors.
2.  **Debug Logs**: Run `convert-assets --verbose` and inspect `satoru_log` outputs for internal state.
3.  **SVG Inspection**: For rendering changes, check the generated SVG source for correct tag structure and attributes.
4.  **Visual Audit**: Manually inspect generated images in `packages/visual-test/temp`.

### 4.3 Maintenance
- **Formatting**: Always run `pnpm format` (`clang-format` for C++, `Prettier` for TS).
- **Asset Conversion**: `pnpm --filter visual-test convert-assets [file.html]`
  - Flags: `--outline` (Force SVG paths), `--no-outline` (Disable paths), `--verbose` (Enable logs).
