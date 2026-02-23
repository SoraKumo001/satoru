import { Satoru as BaseSatoru, RenderOptions } from "satoru-render/index";

export * from "./log-level.js";
export * from "./core.js";
export type {
  SatoruModule,
  RequiredResource,
  ResourceResolver,
  RenderOptions,
} from "./index.js";

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
    return new Satoru(createSatoruModuleSingle);
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
