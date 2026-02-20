import { initWorker } from "worker-lib";
import { Satoru, type RenderOptions, LogLevel } from "./single.js";

let satoru: Satoru | undefined;

const getSatoru = async () => {
  if (!satoru) {
    satoru = await Satoru.create();
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
    return await s.render(options);
  },
};

const map = initWorker(actions);
export type SatoruWorker = typeof map;
