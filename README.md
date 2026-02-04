# Satoru Wasm: High-Performance HTML to SVG Engine

https://sorakumo001.github.io/satoru/

**Satoru** is a portable, WebAssembly-powered HTML rendering engine. It combines the **Skia Graphics Engine** and **litehtml** to provide high-quality, pixel-perfect SVG generation entirely within WebAssembly.

## 🚀 Project Status: High-Fidelity Rendering Ready

The engine has moved beyond basic shapes and now supports full text layout with custom outline fonts and complex CSS styling.

### Key Capabilities

- **Pure Wasm Pipeline**: Performs all layout and drawing operations inside Wasm. Zero dependencies on browser DOM or `<canvas>` for rendering.
- **Dynamic Font Loading**: Supports loading `.ttf` / `.woff2` files at runtime. Text is rendered as vector paths or `<text>` elements in the resulting SVG.
- **Accurate Text Metrics**: Integrated **FreeType** allowing for precise character positioning and multi-line layout.
- **Advanced CSS Support**:
  - Box model (margin, padding, border).
  - **Border Radius**: Smooth rounded corners via Skia's `SkRRect`.
  - **Font Family**: Intelligent font matching with automated name cleaning (handles quotes and case-sensitivity).
- **Optimized Build Architecture**:
  - **Decoupled Assets**: Generates a lightweight `satoru.js` loader and a `satoru.wasm` binary in the `public/` folder, ensuring fast Vite HMR and efficient browser streaming.
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

# Start dev environment
npm run dev
```

## 🧩 Solved Challenges

- **The saveSetjmp Problem**: Resolved the `ReferenceError: saveSetjmp` by implementing robust C++ stubs and ensuring consistent exception handling flags across all modules.
- **Font Name Mismatch**: Implemented a "cleaning" layer that normalizes CSS font-family strings (removing nested quotes) to match Wasm-loaded font keys.
- **SVG Viewport Logic**: Implemented 2-pass rendering where the content height is first calculated, then the SVG canvas is sized exactly to the content.

## 📅 Roadmap

- [x] Dynamic Custom Font Support.
- [x] Border Radius & Advanced Box Styling.
- [ ] Optimized Asset Separation (JS/Wasm).
- [ ] Image (`<img>`) support via Wasm-side decoders.
- [ ] Complex CSS Flexbox/Grid support.
