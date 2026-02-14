import { Satoru as BaseSatoru, LogLevel } from "./index.js";

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
  private constructor(factory: any) {
    super(factory);
  }

  /**
   * Create Satoru instance with embedded WASM.
   */
  static async create(): Promise<Satoru> {
    const { default: createSatoruModuleSingle } =
      // @ts-ignore
      await import("../dist/satoru-single.js");
    return BaseSatoru.create(createSatoruModuleSingle) as Promise<Satoru>;
  }

  /** @deprecated Use Satoru.create */
  static async init(): Promise<Satoru> {
    return this.create();
  }
}
