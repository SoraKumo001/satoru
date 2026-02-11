import {
  Satoru as BaseSatoru,
  createSatoruModule,
  SatoruOptions,
  SatoruModule,
} from "./index.js";
// @ts-ignore
import satoruWasm from "../dist/satoru.wasm";

/**
 * Cloudflare Workers (workerd) specialized wrapper for Satoru.
 * Handles the specific WASM instantiation requirements for the environment.
 */
export class Satoru extends BaseSatoru {
  // Use protected constructor from base
  private constructor(mod: SatoruModule, instancePtr: number) {
    super(mod, instancePtr);
  }

  /**
   * Initialize Satoru for Cloudflare Workers.
   * @param wasm The compiled WASM module (defaults to the bundled one)
   * @param options Additional Satoru options
   */
  static async init(
    wasm: WebAssembly.Module = satoruWasm,
    options?: SatoruOptions,
  ): Promise<Satoru> {
    const mod = await createSatoruModule({
      ...options,
      instantiateWasm: (imports: any, successCallback: any) => {
        // Cloudflare Workers requires using the pre-compiled WebAssembly.Module
        WebAssembly.instantiate(wasm, imports)
          .then((instance) => {
            successCallback(instance, wasm);
          })
          .catch((e) => {
            console.error("Satoru [workerd]: Wasm instantiation failed:", e);
          });
        return {}; // Return empty object as emscripten expects
      },
    });

    const instancePtr = mod._create_instance();
    return new Satoru(mod, instancePtr);
  }
}
