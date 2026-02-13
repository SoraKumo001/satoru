import { initWorker } from "worker-lib";
import { Satoru, type RenderOptions, LogLevel } from "./single.js";

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
  async render(
    options: RenderOptions,
    onLog?: (level: LogLevel, message: string) => void,
  ) {
    const s = await getSatoru();
    if (onLog) {
      options.onLog = onLog;
    }
    return await s.render(options);
  },

  async toSvg(value: string, width: number, height: number = 0) {
    const s = await getSatoru();
    return s.toSvg(value, width, height);
  },

  async toPng(value: string, width: number, height: number = 0) {
    const s = await getSatoru();
    return s.toPng(value, width, height);
  },

  async toPdf(value: string, width: number, height: number = 0) {
    const s = await getSatoru();
    return s.toPdf(value, width, height);
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

const map = initWorker(actions);
export type SatoruWorker = typeof map;
