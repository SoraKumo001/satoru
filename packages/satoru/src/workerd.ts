// @ts-ignore
import createSatoruModule from "../dist/satoru.js";
import { Satoru as BaseSatoru } from "./index.js";
// @ts-ignore
import satoruWasm from "../dist/satoru.wasm";

export type {
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
  private constructor(factory: any) {
    super(factory);
  }

  /**
   * Create Satoru instance for Cloudflare Workers.
   * @param wasm The compiled WASM module (defaults to the bundled one)
   */
  static async create(wasm: WebAssembly.Module = satoruWasm): Promise<Satoru> {
    const factory = async (o: any) =>
      createSatoruModule({
        ...o,
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

    return BaseSatoru.create(factory) as Promise<Satoru>;
  }
}
