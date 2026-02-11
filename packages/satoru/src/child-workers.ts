import { initWorker } from "worker-lib";
import { Satoru, type RenderOptions } from "./single.js";

let satoru: Satoru | undefined;

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
  async render(options: RenderOptions) {
    const s = await getSatoru();
    return s.render(options);
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
