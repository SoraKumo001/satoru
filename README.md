# Satoru Wasm: High-Performance HTML to SVG Engine

https://sorakumo001.github.io/satoru/

**Satoru** is a portable, WebAssembly-powered HTML rendering engine. It combines the **Skia Graphics Engine** and **litehtml** to provide high-quality, pixel-perfect SVG generation entirely within WebAssembly.

## 🚀 Project Status: High-Fidelity Rendering Ready

The engine supports full text layout with custom fonts, complex CSS styling, and efficient image embedding.

### Key Capabilities

- **Pure Wasm Pipeline**: Performs all layout and drawing operations inside Wasm. Zero dependencies on browser DOM or `<canvas>` for rendering.
- **Dynamic Font Loading**: Supports loading `.ttf` / `.woff2` / `.ttc` files at runtime. Text is rendered as vector paths in the resulting SVG.
- **Japanese Support**: Full support for Japanese rendering with fallback font logic (Noto Sans CJK, MS Gothic).
- **Advanced CSS Support**:
  - Box model (margin, padding, border).
  - **Border Radius**: Accurate rounded corners using SVG arc commands.
  - **Box Shadow**: High-quality shadows using SVG filters and Gaussian blur.
  - **Font Family**: Intelligent font matching with automated name cleaning.
- **Efficient Image Handling**:
  - **Host-side Decoding**: Image decoding is handled by the JavaScript environment (browser/Node.js), keeping the Wasm binary lean.
  - **SVG Embedding**: Automatically embeds images as Data URLs with proper clipping paths.
- **Optimized Build Architecture**:
  - **Lightweight Wasm**: Removed redundant decoders (PNG/ZLIB) from the Wasm module to reduce binary size.
  - **High-Speed Compilation**: CMake optimized for parallel building (`-j16`).

## 🛠 Tech Stack

- **Graphics**: [Skia](https://skia.org/) (Subset compiled from source for Wasm)
- **HTML Layout**: [litehtml](https://github.com/litehtml/litehtml)
- **HTML Parser**: [gumbo](https://github.com/google/gumbo-parser)
- **Font Backend**: [FreeType](https://www.freetype.org/)
- **Linker Logic**: Emscripten with SJLJ (Longjmp) and Exception support.

## 🏗 Build & Run

### Prerequisites

- [emsdk](https://github.com/emscripten-core/emsdk) (Targeting `latest`)
- [vcpkg](https://vcpkg.io/) (Wasm32-emscripten triplet)

### Commands

```bash
# Full clean rebuild (Deletes build folder and re-compiles everything)
npm run compile-wasm

# Incremental build (Fast C++ changes)
npm run cmake-build

# Run batch conversion test (Converts HTML assets to SVG)
npm test

# Start dev environment (Vite)
npm run dev
```

## 🧩 Solved Challenges

- **Invalid SVG Structure**: Fixed a regression where `<defs>` were inserted outside the root `<svg>` tag, ensuring standard-compliant XML output.
- **Stable Handling of Large Data URLs**: Refactored the SVG post-processing scanner to handle massive image Data URLs using safe string concatenation and index-based scanning instead of fragile regex.
- **Precision Rendering**: Standardized on `%.2f` precision for SVG coordinates and arc parameters to ensure perfect alignment and visual consistency.

## 📅 Roadmap

- [x] Dynamic Custom Font Support.
- [x] Border Radius & Advanced Box Styling.
- [x] Box Shadow support.
- [x] Optimized Asset Separation (JS/Wasm).
- [x] Efficient Image (`<img>`) embedding (JS-side decoding).
- [x] Japanese Language Rendering.
- [ ] CSS Gradient support (Linear/Radial).
- [ ] Optional SVG `<text>` element output (Text-to-Path is current default).
- [ ] Complex CSS Flexbox/Grid support.