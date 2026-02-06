import { Satoru } from "satoru";

export interface Env {}

// Cache for font data to avoid re-fetching on every request
let cachedFont: Uint8Array | null = null;
const NOTO_JP_400 = "https://cdn.jsdelivr.net/npm/@fontsource/noto-sans-jp/files/noto-sans-jp-japanese-400-normal.woff2";

export default {
  async fetch(
    request: Request,
    env: Env,
    ctx: ExecutionContext,
  ): Promise<Response> {
    const url = new URL(request.url);
    const html =
      url.searchParams.get("html") || "<h1>こんにちは！ Satoruからの挨拶です。</h1><p>日本語のレンダリングテストです。</p>";
    const width = parseInt(url.searchParams.get("width") || "800");

    try {
      // Fetch font if not cached
      if (!cachedFont) {
        console.log("Fetching Noto Sans JP...");
        const response = await fetch(NOTO_JP_400);
        if (response.ok) {
          cachedFont = new Uint8Array(await response.arrayBuffer());
        }
      }

      const satoru = new Satoru();
      await satoru.init();

      // Load font if we have it
      if (cachedFont) {
        satoru.loadFont("Noto Sans JP", cachedFont);
        satoru.loadFont("sans-serif", cachedFont);
      }

      const pngDataUrl = satoru.toPngDataUrl(html, width);
      return new Response(
        `<html><body><img src="${pngDataUrl}" /></body></html>`,
        {
          headers: {
            "Content-Type": "text/html",
          },
        },
      );
    } catch (e: any) {
      return new Response(e.stack || e.toString(), { status: 500 });
    }
  },
};
