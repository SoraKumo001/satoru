// @ts-ignore
import createSatoruModule from "../dist/satoru.js";
import { Satoru as BaseSatoru, SatoruOptions, SatoruModule } from "./index.js";
// @ts-ignore
import satoruWasm from "../dist/satoru.wasm";

export type {
  SatoruOptions,
  SatoruModule,
  RequiredResource,
  ResourceResolver,
  RenderOptions,
} from "./index.js";

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
    options: SatoruOptions = {},
  ): Promise<Satoru> {
    return BaseSatoru.init(
      async (o: any) => createSatoruModule({ ...o, ...options }),
      {
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
      },
    ) as Promise<Satoru>;
  }
}
