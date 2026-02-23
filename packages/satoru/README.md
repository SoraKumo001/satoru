# Satoru Wasm: High-Performance HTML to SVG/PNG/PDF Engine

[![Playground](https://img.shields.io/badge/Demo-Playground-blueviolet)](https://sorakumo001.github.io/satoru/)

**Satoru** is a portable, WebAssembly-powered HTML rendering engine. It combines the **Skia Graphics Engine** and **litehtml** to provide high-quality, pixel-perfect SVG, PNG, and PDF generation entirely within WebAssembly.

## 🚀 Project Status: High-Fidelity Rendering & Edge Ready

The engine supports full text layout with custom fonts, complex CSS styling, and efficient binary data transfer. It is now compatible with **Cloudflare Workers (workerd)**, allowing for serverless, edge-side image and document generation.

### Key Capabilities

- **Pure Wasm Pipeline**: Performs all layout and drawing operations inside Wasm. Zero dependencies on browser DOM or `<canvas>`.
- **Edge Native**: Specialized wrapper for Cloudflare Workers ensures smooth execution in restricted environments.
- **Triple Output Modes**:
  - **SVG**: Generates lean, vector-based Pure SVG strings with post-processed effects (Filters, Gradients).
  - **PNG**: Generates high-quality raster images via Skia, transferred as binary data for maximum performance.
  - **PDF**: Generates high-fidelity vector documents via Skia's PDF backend, including native support for text, gradients, images, and **multi-page output**.
- **High-Level TS Wrapper**: Includes a `Satoru` class that abstracts Wasm memory management and provides a clean async API.
- **Dynamic Font Loading**: Supports loading `.ttf` / `.woff2` / `.ttc` files at runtime with automatic weight/style inference.
- **Japanese Support**: Full support for Japanese rendering with multi-font fallback logic.
- **Image Format Support**: Native support for **PNG**, **JPEG**, **WebP**, **AVIF**, **GIF**, **BMP**, and **ICO** image formats.
- **Advanced CSS Support**:
  - **Box Model**: Margin, padding, border, and accurate **Border Radius**.
  - **Box Shadow**: High-quality **Outer** and **Inset** shadows using advanced SVG filters (SVG) or Skia blurs (PNG/PDF).
  - **Gradients**: Linear, **Elliptical Radial**, and **Conic** (Sweep) gradient support.
  - **Standard Tags**: Full support for `<b>`, `<strong>`, `<i>`, `<u>`, and `<h1>`-`<h6>` via integrated master CSS.
  - **Text Decoration**: Supports `underline`, `line-through`, `overline` with `solid`, `dotted`, and `dashed` styles.
  - **Text Shadow**: Multiple shadows with blur, offset, and color support (PNG/SVG/PDF).

## 🛠️ Usage (TypeScript)

### Standard Environment (Node.js / Browser)

Satoru provides a high-level `render` function for converting HTML to various formats. It automatically handles WASM instantiation and resource resolution.

#### Basic Rendering (Automatic Resource Resolution)

The `render` function supports automated multi-pass resource resolution. It identifies missing fonts, images, and external CSS and requests them via the `resolveResource` callback.

```typescript
import { render, LogLevel } from "satoru-render";

const html = `
  <style>
    @font-face {
      font-family: 'Roboto';
      src: url('https://fonts.gstatic.com/s/roboto/v30/KFOmCnqEu92Fr1Mu4mxK.woff2');
    }
  </style>
  <div style="font-family: 'Roboto'; color: #2196F3; font-size: 40px;">
    Hello Satoru!
    <img src="logo.png" style="width: 50px;">
  </div>
`;

// Render to PDF with automatic resource resolution from HTML string
const pdf = await render({
  value: html,
  width: 600,
  format: "pdf",
  baseUrl: "https://example.com/assets/", // Optional: resolve relative URLs
  logLevel: LogLevel.Info,
  resolveResource: async (resource, defaultResolver) => {
    if (resource.url.startsWith("custom://")) {
      return myCustomData;
    }
    return defaultResolver(resource);
  },
});

// Render from a URL directly
const png = await render({
  url: "https://example.com/page.html",
  width: 1024,
  format: "png",
});
```

#### Render Options

| Option            | Type                                | Description                                                                                                                                                                                             |
| ----------------- | ----------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `value`           | `string \| string[]`                | HTML string or array of HTML strings. (One of `value` or `url` is required)                                                                                                                             |
| `url`             | `string`                            | URL to fetch HTML from. (One of `value` or `url` is required)                                                                                                                                           |
| `width`           | `number`                            | **Required.** Width of the output in pixels.                                                                                                                                                            |
| `height`          | `number`                            | Height of the output in pixels. Default is `0` (automatic height).                                                                                                                                      |
| `format`          | `"svg" \| "png" \| "webp" \| "pdf"` | Output format. Default is `"svg"`.                                                                                                                                                                      |
| `textToPaths`     | `boolean`                           | Whether to convert SVG text to paths. Default is `true`.                                                                                                                                                |
| `resolveResource` | `ResourceResolver`                  | Async callback to fetch missing fonts, images, or CSS. Returns `Uint8Array`, `ArrayBufferView` or `null`. It receives a second argument `defaultResolver` to fallback to the standard resolution logic. |
| `fonts`           | `Object[]`                          | Array of `{ name, data: Uint8Array }` to pre-load fonts.                                                                                                                                                |
| `images`          | `Object[]`                          | Array of `{ name, url, width?, height? }` to pre-load images.                                                                                                                                           |
| `css`             | `string`                            | Extra CSS to inject into the rendering process.                                                                                                                                                         |
| `baseUrl`         | `string`                            | Base URL used to resolve relative URLs in fonts, images, and links.                                                                                                                                     |
| `userAgent`       | `string`                            | User-Agent header for fetching resources (Node.js environment).                                                                                                                                         |
| `logLevel`        | `LogLevel`                          | Logging verbosity (`None`, `Error`, `Warning`, `Info`, `Debug`).                                                                                                                                        |
| `onLog`           | `(level, msg) => void`              | Custom callback for receiving log messages.                                                                                                                                                             |

### 💻 CLI Usage

Satoru includes a command-line interface for easy conversion.

```bash
# Convert a local HTML file to PNG
npx satoru-render input.html -o output.png

# Convert a URL to PDF
npx satoru-render https://example.com -o example.pdf -w 1280

# Convert with custom options
npx satoru-render input.html -w 1024 -f webp --verbose
```

#### CLI Options

- `<input>`: Input file path or URL (**Required**)
- `-o, --output <path>`: Output file path
- `-w, --width <number>`: Viewport width (default: 800)
- `-h, --height <number>`: Viewport height (default: 0, auto-calculate)
- `-f, --format <format>`: Output format: `svg`, `png`, `webp`, `pdf`
- `--verbose`: Enable detailed logging
- `--help`: Show help message

### 📄 Multi-page PDF Generation

You can generate a multi-page PDF by passing an array of HTML strings to the `value` property. Each string in the array will be rendered as a new page.

```typescript
const pdf = await satoru.render({
  value: [
    "<h1>Page 1</h1><p>First page content.</p>",
    "<h1>Page 2</h1><p>Second page content.</p>",
    "<h1>Page 3</h1><p>Third page content.</p>",
  ],
  width: 600,
  format: "pdf",
});
```

### ☁️ Cloudflare Workers (Edge)

Satoru is optimized for Cloudflare Workers. Use the `workerd` specific export which handles the specific WASM instantiation requirements of the environment.

```typescript
import { render } from "satoru-render";

export default {
  async fetch(request) {
    const pdf = await render({
      value: "<h1>Edge Rendered</h1>",
      width: 800,
      format: "pdf",
      baseUrl: "https://example.com/",
    });

    return new Response(pdf, {
      headers: { "Content-Type": "application/pdf" },
    });
  },
};
```

### 📦 Single-file (Embedded WASM)

For environments where deploying a separate `.wasm` file is difficult, use the `single` export which includes the WASM binary embedded.

```typescript
import { render } from "satoru-render";

const png = await render({
  value: "<div>Embedded WASM!</div>",
  width: 600,
  format: "png",
});
```

### 🧵 Multi-threaded Rendering (Worker Proxy)

For high-throughput applications, the Worker proxy distributes rendering tasks across multiple threads. You can configure all resources in a single `render` call for stateless operation.

```typescript
import { createSatoruWorker, LogLevel } from "satoru-render/workers";

// Create a worker proxy with up to 4 parallel instances
const satoru = createSatoruWorker({ maxParallel: 4 });

// Render with full configuration in one go
const png = await satoru.render({
  value: "<h1>Parallel Rendering</h1><img src='icon.png'>",
  width: 800,
  format: "png",
  baseUrl: "https://example.com/assets/",
  logLevel: LogLevel.Debug, // Enable debug logs for this task
  fonts: [{ name: "CustomFont", data: fontData }], // Pre-load fonts
  css: "h1 { color: red; }", // Inject extra CSS
});
```

## 📜 License

MIT License
