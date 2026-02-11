import { Satoru as BaseSatoru, SatoruOptions, SatoruModule } from "./index.js";
import { createWorker } from "worker-lib";
import type { SatoruWorker } from "./workers/node-workers.js";

// @ts-ignore
import createSatoruModuleSingle from "../dist/satoru-single.js";

export { LogLevel } from "./index.js";
export type {
  SatoruOptions,
  SatoruModule,
  RequiredResource,
  ResourceResolver,
} from "./index.js";
export type { SatoruWorker } from "./workers/node-workers.js";

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

/**
 * Create a Satoru worker proxy using worker-lib.
 * @param params Initialization parameters
 * @param params.worker Optional: Path to the worker file, a URL, or a factory function.
 *                      Defaults to the bundled workers.js in the same directory.
 * @param params.maxParallel Maximum number of parallel workers
 */
export const createSatoruWorker = (params: {
  worker?: string | URL | (() => Worker | string | URL);
  maxParallel?: number;
}) => {
  const { worker, maxParallel = 4 } = params;

  let NodeWorker: any;
  let isNode =
    typeof process !== "undefined" && process.versions && process.versions.node;

  const factory = () => {
    if (worker) {
      return typeof worker === "function" ? worker() : worker;
    }
    const workerUrl =
      typeof window !== "undefined"
        ? new URL("./workers/web-workers.js", import.meta.url)
        : new URL("./workers/node-workers.js", import.meta.url);

    if (typeof Worker !== "undefined") {
      return new Worker(workerUrl, { type: "module" });
    } else if (isNode) {
      if (!NodeWorker) {
        throw new Error(
          "Worker is not initialized. Please wait for a moment or check your environment.",
        );
      }
      return new NodeWorker(workerUrl, {
        execArgv: ["--import", "tsx"],
      });
    }
    throw new Error("Worker is not supported in this environment.");
  };

  const workerInstance = createWorker<SatoruWorker>(
    factory as any,
    maxParallel,
  );

  const proxy = new Proxy(workerInstance, {
    get(target, prop, receiver) {
      if (prop in target) {
        return Reflect.get(target, prop, receiver);
      }
      return (...args: any[]) => target.execute(prop as any, ...args);
    },
  }) as unknown as Omit<typeof workerInstance, "execute"> & SatoruWorker;

  if (isNode && !worker) {
    import("worker_threads").then((mod) => {
      NodeWorker = mod.Worker;
    });
  }

  return proxy;
};
