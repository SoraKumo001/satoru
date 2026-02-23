import { createWorker, Worker } from "worker-lib";
import type { SatoruWorker } from "./child-workers.js";
export type { SatoruWorker } from "./child-workers.js";
import { type RenderOptions } from "./index.js";
export { Satoru } from "./index.js";
export * from "./index.js";
export * from "./log-level.js";

/**
 * Create a Satoru worker proxy using worker-lib.
 * @param params Initialization parameters
 * @param params.worker Optional: Path to the worker file, a URL, or a factory function.
 *                      Defaults to the bundled workers.js in the same directory.
 * @param params.maxParallel Maximum number of parallel workers
 */
export const createSatoruWorker = (params?: {
  worker?: string | URL | (() => Worker | string | URL);
  maxParallel?: number;
}) => {
  const { worker, maxParallel = 4 } = params ?? {};

  const factory = () => {
    let w: any;
    if (worker) {
      w = typeof worker === "function" ? worker() : worker;
    } else {
      const workerUrl =
        typeof window !== "undefined"
          ? new URL("./web-workers.js", import.meta.url)
          : new URL("./child-workers.js", import.meta.url);

      if (typeof Worker !== "undefined") {
        w = new Worker(workerUrl, { type: "module" });
      }
    }

    if (!w) throw new Error("Worker is not supported in this environment.");

    return w;
  };

  const workerInstance = createWorker<SatoruWorker>(factory, maxParallel);

  const proxy = new Proxy(workerInstance, {
    get(target, prop, receiver) {
      if (prop === "render") {
        return async (options: RenderOptions) => {
          return await target.execute("render", options as any);
        };
      }

      if (prop in target) {
        return Reflect.get(target, prop, receiver);
      }
      return (...args: any[]) => target.execute(prop as any, ...args);
    },
  }) as unknown as Omit<typeof workerInstance, "execute"> & SatoruWorker;

  return proxy;
};
