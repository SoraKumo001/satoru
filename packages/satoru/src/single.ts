import { Satoru as BaseSatoru, LogLevel, SatoruModule } from "./index.js";

export { LogLevel } from "./index.js";
export type {
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
   */
  static async init(): Promise<Satoru> {
    const { default: createSatoruModuleSingle } =
      // @ts-ignore
      await import("../dist/satoru-single.js");
    return BaseSatoru.init(createSatoruModuleSingle) as Promise<Satoru>;
  }
}
