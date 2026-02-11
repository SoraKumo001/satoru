import { Satoru as BaseSatoru, SatoruOptions, SatoruModule } from "./index.js";
// @ts-ignore
import createSatoruModuleSingle from "../dist/satoru-single.js";

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
