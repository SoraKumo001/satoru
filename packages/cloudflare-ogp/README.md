# Cloudflare OGP Generator (Satoru)

A high-performance OGP image generator running on **Cloudflare Workers**, powered by **Satoru Wasm**.

This package demonstrates how to use Satoru to generate dynamic social media images (OGP) at the edge using React (JSX) and Tailwind-like inline styles.

---

## Sample Image

![](./document/image.png)

## 🚀 Features

- **Edge-Side Rendering**: Generates PNG images directly on Cloudflare Workers using WebAssembly.
- **React Integration**: Define your OGP layouts using familiar JSX syntax via `satoru-render/react` or `satoru-render/preact`.
- **Tailwind CSS Support**: Generate styles dynamically from class names using `satoru-render/tailwind`.
- **Automatic Font Loading**: Uses Google Fonts (Noto Sans JP) with automatic resolution.
- **Modern UI**: Dark-themed layout with decorative elements and image support.
- **High Performance**: Optimized Wasm binary for fast cold starts and execution.
- **Dynamic Content**: Accepts query parameters to customize titles, subtitles, and images.

---

## 🛠 Configuration for JSX & Tailwind

### 1. Dependencies

Ensure you have the following dependencies installed for JSX (via Preact) and Tailwind (via UnoCSS) support:

```bash
pnpm add preact preact-render-to-string @unocss/preset-wind4 satoru-render
```

### 2. TypeScript Configuration (`tsconfig.json`)

To use JSX in your worker, configure `tsconfig.json` to handle JSX. If you are using Preact, the following settings are recommended:

```json
{
  "compilerOptions": {
    "jsx": "react-jsx",
    "jsxImportSource": "preact",
    "types": ["@cloudflare/workers-types", "preact/jsx-runtime"]
  }
}
```

_Note: In this project, `jsx: preserve` is used to let the bundler handle the transformation. When using this setting, you should include `/\*\* @jsx h _/`at the top of your`.tsx` files if not otherwise configured in your bundler.\*

### 3. Usage in Code (JSX & Tailwind)

Depending on your preference, you can use either React or Preact.

#### Using Preact (Recommended for Workers)

```tsx
/** @jsx h */
import { h, toHtml } from "satoru-render/preact";
import { createCSS } from "satoru-render/tailwind";
import { render } from "satoru-render";

// 1. Define your layout with Tailwind classes
const html = toHtml(
  <div className="w-[1200px] h-[630px] flex items-center justify-center bg-slate-900">
    <h1 className="text-6xl text-white font-bold">Hello World</h1>
  </div>,
);

// 2. Generate CSS from the HTML
const css = await createCSS(html);

// 3. Render to PNG
const png = await render({
  value: html,
  css,
  width: 1200,
  height: 630,
  format: "png",
});
```

#### Using React

```tsx
import { toHtml } from "satoru-render/react";
import { createCSS } from "satoru-render/tailwind";
import { render } from "satoru-render";

const html = toHtml(
  <div className="flex bg-blue-500">
    <h1>React OGP</h1>
  </div>,
);
const css = await createCSS(html);
// ... render as usual
```

---

## 🛠️ Getting Started

### 1. Installation

Ensure you have installed the dependencies:

```bash
pnpm install
```

### 2. Local Development

Start the local development server using `wrangler`:

```bash
pnpm dev
```

The worker will be available at `http://localhost:8787`. You can test it by visiting:
`http://localhost:8787/?title=Hello+Satoru&subtitle=Ultra-fast+image+generation+on+Cloudflare+Workers`

### 3. Deployment

Deploy the worker to your Cloudflare account:

```bash
pnpm deploy
```

---

## 📖 API Reference

### `GET /`

Generates an OGP image based on the provided query parameters.

| Parameter  | Type     | Default                    | Description                   |
| :--------- | :------- | :------------------------- | :---------------------------- |
| `title`    | `string` | `"Title"`                  | The main title text.          |
| `subtitle` | `string` | `"subtitle"`               | The subtitle text.            |
| `image`    | `string` | (A sample landscape image) | URL for the decorative image. |

**Example Request:**
`https://your-worker.workers.dev/?title=My+Awesome+Post&subtitle=Read+more+on+my+blog&image=https://example.com/image.jpg`

---

## 🧩 How it Works

The generator uses a combination of **React**, and **Satoru**:

1.  **Request Handling**: Parses query parameters from the URL.
2.  **React (JSX)**: Defines the visual layout and styles using standard React components.
3.  **satoru-render/preact** (or `/react`): Converts the JSX elements into an HTML string, including Google Fonts integration.
4.  **Satoru**: Renders the HTML string into a PNG buffer using the Skia graphics engine compiled to Wasm.
5.  **Caching**: Utilizes Cloudflare Workers KV/Cache API for performance.
6.  **Response**: Returns the PNG buffer with `Content-Type: image/png` and cache headers.

```tsx
// src/index.tsx snippet
const html = toHtml(
  <html style={{ margin: 0, padding: 0 }}>
    <head>
      <link
        href="https://fonts.googleapis.com/css2?family=Noto+Sans+JP:wght@100..900&display=swap"
        rel="stylesheet"
      />
    </head>
    <body style={{ margin: 0, padding: 0 }}>
      <div
        style={{
          width: "1200px",
          height: "630px",
          background: "#0a0a0c",
          display: "flex",
        }}
      >
        {/* Your OGP Design */}
        <div style={{ fontSize: "80px", color: "#ffffff", display: "flex" }}>
          {title}
        </div>
      </div>
    </body>
  </html>,
);

const png = await render({
  value: html,
  width: 1200,
  height: 630,
  format: "png",
});
```

---

## 📜 License

MIT License - SoraKumo <info@croud.jp>
