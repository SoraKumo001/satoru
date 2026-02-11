import { Satoru as BaseSatoru, SatoruOptions, SatoruModule } from "./index.js";
import { createWorker } from "worker-lib";
import type { SatoruWorker } from "./workers.js";

// @ts-ignore
import createSatoruModuleSingle from "../dist/satoru-single.js";

export { LogLevel } from "./index.js";
export type {
  SatoruOptions,
  SatoruModule,
  RequiredResource,
  ResourceResolver,
} from "./index.js";
export type { SatoruWorker } from "./workers.js";

/**
 * Single-file specialized wrapper for Satoru.
 * Uses the satoru-single.js artifact with embedded WASM.
 */
export class Satoru extends BaseSatoru {
  // Use protected constructor from base
  private constructor(mod: SatoruModule, instancePtr: number) {
    super(mod, instancePtr);
  }

  /**
   * Initialize Satoru with embedded WASM.
   * @param options Additional Satoru options
   */
  static async init(options: SatoruOptions = {}): Promise<Satoru> {
    const mod = await createSatoruModuleSingle(options);
    const instancePtr = mod._create_instance();
    return new Satoru(mod, instancePtr);
  }
}

/**
 * Create a Satoru worker proxy using worker-lib.
 * @param worker Path to the worker file, a URL, or a factory function
 * @param maxParallel Maximum number of parallel workers
 */
export const createSatoruWorker = (
  worker: string | URL | (() => Worker),
  maxParallel?: number,
) => {
  return createWorker<SatoruWorker>(
    typeof worker === "function" ? worker : () => worker,
    maxParallel,
  );
};
