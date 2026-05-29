---
sidebar_position: 2
title: Next.js Route Handler
---

# Next.js Route Handler で OGP 画像を返す

Next.js の App Router では、Route Handler から Satoru の PNG をそのまま `Response` として返せます。React component を HTML に変換してから render するため、OGP 画像や lightweight な PDF endpoint に向いています。

## Recipe

```tsx
// app/api/og/route.tsx
import { render } from "satoru-render/single";
import { toHtml } from "satoru-render/react";

export const runtime = "nodejs";

export async function GET(request: Request) {
  const url = new URL(request.url);
  const title = url.searchParams.get("title") ?? "Satoru";

  const html = toHtml(
    <main
      style={{
        width: 1200,
        height: 630,
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
        background: "#101418",
        color: "#f8fafc",
        fontFamily: "Inter, sans-serif",
        padding: 64,
      }}
    >
      <h1 style={{ fontSize: 72, lineHeight: 1.1 }}>{title}</h1>
    </main>,
  );

  const png = await render({
    value: html,
    width: 1200,
    height: 630,
    format: "png",
    limits: {
      timeoutMs: 5000,
      maxResourceCount: 20,
      allowedProtocols: ["https:"],
    },
  });

  return new Response(png, {
    headers: {
      "Content-Type": "image/png",
      "Cache-Control": "public, max-age=31536000, immutable",
    },
  });
}
```

## 注意点

- `runtime = "nodejs"` を明示します。Edge Runtime ではなく Node.js runtime で `satoru-render/single` を使うのが扱いやすいです。
- User input を HTML に含める場合は、React component の props として扱い、手書きの HTML 連結を避けます。
- 外部画像や font を読む場合は、`limits.allowedProtocols` や `limits.allowedHosts` を設定して SSRF を避けます。
- 同じ画像を長く返せる endpoint では `Cache-Control` を強めに設定すると、Vercel/CDN 側の cache が効きます。
