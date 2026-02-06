import { Satoru as s, createSatoruModule, SatoruOptions } from "./index.js";
// @ts-ignore
import satoruWasm from "../dist/satoru.wasm";

/**
 * Cloudflare Workers (workerd) specialized wrapper for Satoru.
 * Handles the specific WASM instantiation requirements for the environment.
 */
export class Satoru extends s {
  constructor() {
    super(createSatoruModule);
  }

  /**
   * Initialize Satoru for Cloudflare Workers.
   * @param wasm The compiled WASM module (defaults to the bundled one)
   * @param options Additional Satoru options
   */
  async init(wasm: WebAssembly.Module = satoruWasm, options?: SatoruOptions) {
    await super.init({
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
        return {}; // Emscripten: returning {} indicates async instantiation
      },
    });
  }
}
