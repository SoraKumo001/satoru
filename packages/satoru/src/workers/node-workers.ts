import { initWorker } from "worker-lib";
import { Satoru } from "../single.js";
import path from "path";
import fs from "fs";

let satoru: Satoru | undefined;
let globalAssetsDir: string | undefined;

const getSatoru = async () => {
  if (!satoru) {
    satoru = await Satoru.init();
  }
  return satoru;
};

/**
 * Worker-side implementation for Satoru.
 * Exposes Satoru methods via worker-lib.
 */
const actions = {
  async setAssetsDir(dir: string) {
    globalAssetsDir = dir;
  },

  async render(options: any) {
    const s = await getSatoru();
    const { baseUrl, ...renderOptions } = options;

    // Provide a default resource resolver if baseUrl or assetsDir is present
    if (!renderOptions.resolveResource && (baseUrl || globalAssetsDir)) {
      renderOptions.resolveResource = async (r: {
        type: string;
        url: string;
      }) => {
        try {
          if (
            globalAssetsDir &&
            !r.url.startsWith("http") &&
            !r.url.startsWith("data:")
          ) {
            const filePath = path.join(globalAssetsDir, r.url);
            if (fs.existsSync(filePath)) {
              return new Uint8Array(fs.readFileSync(filePath));
            }
          }

          const url =
            r.url.startsWith("http") || r.url.startsWith("data:")
              ? r.url
              : baseUrl
                ? new URL(r.url, baseUrl).href
                : undefined;

          if (!url) return null;

          const resp = await fetch(url);
          if (!resp.ok) return null;

          const buf = await resp.arrayBuffer();
          return new Uint8Array(buf);
        } catch (e) {
          console.warn(`[Satoru Worker] Failed to fetch ${r.url}`, e);
          return null;
        }
      };
    }

    return s.render(renderOptions);
  },

  async toSvg(html: string, width: number, height: number = 0) {
    const s = await getSatoru();
    return s.toSvg(html, width, height);
  },

  async toPng(html: string, width: number, height: number = 0) {
    const s = await getSatoru();
    return s.toPng(html, width, height);
  },

  async toPdf(html: string, width: number, height: number = 0) {
    const s = await getSatoru();
    return s.toPdf(html, width, height);
  },

  async loadFont(name: string, data: Uint8Array) {
    const s = await getSatoru();
    return s.loadFont(name, data);
  },

  async clearFonts() {
    const s = await getSatoru();
    return s.clearFonts();
  },

  async loadImage(
    name: string,
    dataUrl: string,
    width: number,
    height: number,
  ) {
    const s = await getSatoru();
    return s.loadImage(name, dataUrl, width, height);
  },

  async clearImages() {
    const s = await getSatoru();
    return s.clearImages();
  },

  async clearCss() {
    const s = await getSatoru();
    return s.clearCss();
  },

  async clearAll() {
    const s = await getSatoru();
    return s.clearAll();
  },
};

// Initialization process to make it usable in Worker.
const map = initWorker(actions);
// Export only the type for createWorker<SatoruWorker>
export type SatoruWorker = typeof map;
