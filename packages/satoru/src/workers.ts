import { createWorker as createWorkerWeb } from "worker-lib";
import { createWorker as createWorkerNode } from "worker-lib/node";
import type { SatoruWorker } from "./child-workers.js";
export type { SatoruWorker } from "./child-workers.js";

import { LogLevel, type RenderOptions } from "./index.js";
export { LogLevel } from "./index.js";
export type {
  SatoruOptions,
  SatoruModule,
  RequiredResource,
  ResourceResolver,
  RenderOptions,
} from "./index.js";

// Top-level await for worker_threads in Node.js
let NodeWorker: any;
const isNode = typeof process !== "undefined" && process.versions && process.versions.node;

if (isNode) {
  try {
    const mod = await import("node:worker_threads");
    NodeWorker = mod.Worker;
  } catch (e) {
    // Ignore error
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
      } else if (isNode) {
        if (!NodeWorker) {
          throw new Error(
            "worker_threads.Worker is not available. Ensure you are running in a supported Node.js version.",
          );
        }
        w = new NodeWorker(workerUrl, {
          execArgv: ["--import", "tsx"],
        });
      }
    }

    if (!w) throw new Error("Worker is not supported in this environment.");

    return w;
  };

  const workerInstance = (isNode ? createWorkerNode : createWorkerWeb)<SatoruWorker>(
    factory as any,
    maxParallel,
  );

  const proxy = new Proxy(workerInstance, {
    get(target, prop, receiver) {
      if (prop === "render") {
        return async (options: RenderOptions) => {
          const { onLog, ...workerOptions } = options;
          return await target.execute("render", workerOptions as any, onLog);
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
