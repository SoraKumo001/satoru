import {
  Satoru as BaseSatoru,
  LogLevel,
  SatoruOptions,
  SatoruModule,
} from "./index.js";

export { LogLevel } from "./index.js";
export type {
  SatoruOptions,
  SatoruModule,
  RequiredResource,
  ResourceResolver,
  RenderOptions,
} from "./index.js";
export { createSatoruWorker } from "./workers.js";
export type { SatoruWorker } from "./child-workers.js";

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
    const { default: createSatoruModuleSingle } =
      // @ts-ignore
      await import("../dist/satoru-single.js");
    const mod = (await createSatoruModuleSingle(options)) as SatoruModule;
    const instancePtr = mod._create_instance();
    return new Satoru(mod, instancePtr);
  }
}
