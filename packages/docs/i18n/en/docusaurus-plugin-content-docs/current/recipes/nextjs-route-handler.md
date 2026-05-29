---
sidebar_position: 2
title: Next.js Route Handler
---

# Return OGP Images from a Next.js Route Handler

With the Next.js App Router, a Route Handler can return a Satoru PNG directly as a `Response`. Convert a React component to HTML, then render it. This is a good fit for OGP image endpoints and lightweight PDF endpoints.

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

## Notes

- Set `runtime = "nodejs"`. `satoru-render/single` is easiest to use in the Node.js runtime rather than the Edge Runtime.
- If user input appears in the HTML, pass it through React props instead of concatenating raw HTML.
- When loading external images or fonts, set `limits.allowedProtocols` or `limits.allowedHosts` to avoid SSRF.
- For deterministic images, use a strong `Cache-Control` header so Vercel/CDN caching can do the heavy lifting.
