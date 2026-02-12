import { initWorker } from "worker-lib";
import { Satoru, type RenderOptions, LogLevel } from "./index.js";

let satoru: Satoru | undefined;

const sendLog = (level: LogLevel, message: string) => {
  const payload = { __satoru_log: true, level, message };
  if (typeof self !== "undefined" && typeof self.postMessage === "function") {
    self.postMessage(payload);
  } else {
    try {
      import("worker_threads").then((mod) => {
        mod.parentPort?.postMessage(payload);
      });
    } catch (e) {
      // ignore
    }
  }
};
//
const getSatoru = async () => {
  if (!satoru) {
    satoru = await Satoru.init(
      undefined,
      {
        onLog: (level: LogLevel, message: string) => {
          sendLog(level, message);
        },
      },
      LogLevel.Debug,
    ); // Set a high enough default log level to allow forwarding
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

    // Override onLog to ensure it's captured by the parent's render callback.
    // In workers, we always forward logs via postMessage.
    options.onLog = (level: LogLevel, message: string) => {
      sendLog(level, message);
    };

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

const map = initWorker(actions);
export type SatoruWorker = typeof map;
