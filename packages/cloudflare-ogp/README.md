# Cloudflare OGP Generator (Satoru)

A high-performance OGP image generator running on **Cloudflare Workers**, powered by **Satoru Wasm**.

This package demonstrates how to use Satoru to generate dynamic social media images (OGP) at the edge using React (JSX) and Tailwind-like inline styles.

---

## 🚀 Features

- **Edge-Side Rendering**: Generates PNG images directly on Cloudflare Workers using WebAssembly.
- **React Integration**: Define your OGP layouts using familiar JSX syntax via `satoru/react`.
- **Automatic Font Loading**: Uses Google Fonts (Noto Sans JP) with automatic resolution.
- **High Performance**: Optimized Wasm binary for fast cold starts and execution.
- **Dynamic Content**: Accepts query parameters to customize titles and subtitles.

---

## 🛠️ Getting Started

### 1. Installation

Ensure you have installed the dependencies for the entire monorepo:

```bash
pnpm install
```

### 2. Local Development

Start the local development server using `wrangler`:

```bash
pnpm dev
```

The worker will be available at `http://localhost:8787`. You can test it by visiting:
`http://localhost:8787/?title=Hello+World&subtitle=Generated+at+the+Edge`

### 3. Deployment

Deploy the worker to your Cloudflare account:

```bash
pnpm deploy
```

---

## 📖 API Reference

### `GET /`

Generates an OGP image based on the provided query parameters.

| Parameter | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `title` | `string` | `"こんにちは Satoru"` | The main title text. |
| `subtitle` | `string` | `"Cloudflare Workersで爆速画像生成"` | The subtitle text. |

**Example Request:**
`https://your-worker.workers.dev/?title=My+Awesome+Post&subtitle=Read+more+on+my+blog`

---

## 🧩 How it Works

The generator uses a combination of **Hono**, **React**, and **Satoru**:

1.  **Hono**: Handles the incoming HTTP request and query parameters.
2.  **React (JSX)**: Defines the visual layout and styles.
3.  **satoru/react**: Converts the JSX elements into a standard HTML string.
4.  **Satoru**: Renders the HTML string into a PNG buffer using the Skia graphics engine compiled to Wasm.
5.  **Response**: Returns the PNG buffer with the correct `Content-Type: image/png` header.

```tsx
// src/index.tsx snippet
const html = toHtml(
  <div style={{ display: 'flex', color: 'white', background: 'blue' }}>
    {title}
  </div>
);

const png = await render({
  value: html,
  width: 1200,
  height: 630,
  format: "png",
  resolveResource: async (resource) => {
    const res = await fetch(resource.url);
    return new Uint8Array(await res.arrayBuffer());
  },
});
```

---

## 📜 License

MIT License - SoraKumo <info@croud.jp>
