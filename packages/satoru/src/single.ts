import { Satoru as BaseSatoru, RenderOptions } from "./index.js";

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
}

let instance: Satoru | null = null;

/**
 * High-level render function.
 * Automatically creates and reuses a Satoru instance with embedded WASM.
 */
export async function render(
  options: RenderOptions & { format: "png" | "webp" | "pdf" },
): Promise<Uint8Array>;
export async function render(
  options: RenderOptions & { format?: "svg" },
): Promise<string>;
export async function render(
  options: RenderOptions,
): Promise<string | Uint8Array>;
export async function render(
  options: RenderOptions,
): Promise<string | Uint8Array> {
  if (!instance) {
    instance = await Satoru.create();
  }
  return instance.render(options);
}
