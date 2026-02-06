import { Satoru } from "satoru";
// @ts-ignore
import createSatoruModule from "../../satoru/dist/satoru.js";
// @ts-ignore
import wasm from "../../satoru/dist/satoru.wasm";

export interface Env {}

export default {
  async fetch(
    request: Request,
    env: Env,
    ctx: ExecutionContext,
  ): Promise<Response> {
    const url = new URL(request.url);
    const html =
      url.searchParams.get("html") || "<h1>Hello from Cloudflare Workers!</h1>";
    const width = parseInt(url.searchParams.get("width") || "800");

    try {
      // Satoru class now uses bundled createSatoruModule by default
      const satoru = new Satoru(createSatoruModule);

      await satoru.init({
        wasmBinary: wasm,
        locateFile: (path: string) => path,
      });

      const svg = satoru.toSvg(html, width);

      return new Response(svg, {
        headers: {
          "Content-Type": "image/svg+xml",
        },
      });
    } catch (e: any) {
      return new Response(e.stack || e.toString(), { status: 500 });
    }
  },
};
