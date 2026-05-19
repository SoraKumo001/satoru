import { createWorker, Worker } from "worker-lib";
import type { SatoruWorker } from "./child-workers.js";
export type { SatoruWorker } from "./child-workers.js";
import { type RenderOptions, type WorkerPoolStats } from "./core.js";
export { type RenderOptions } from "./core.js";
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

  let totalPendingJobs = 0;
  let completedJobs = 0;
  let failedJobs = 0;
  let totalJobTimeMs = 0;

  const getStats = (): WorkerPoolStats => ({
    workerCount: maxParallel,
    activeJobs: Math.min(totalPendingJobs, maxParallel),
    queuedJobs: Math.max(0, totalPendingJobs - maxParallel),
    completedJobs,
    failedJobs,
    avgJobTimeMs: completedJobs > 0 ? totalJobTimeMs / completedJobs : 0,
  });

  const reset = () => {
    workerInstance.setLimit(0);
    workerInstance.setLimit(maxParallel);
    totalPendingJobs = 0;
    completedJobs = 0;
    failedJobs = 0;
    totalJobTimeMs = 0;
  };

  const proxy = new Proxy(workerInstance, {
    get(target, prop, receiver) {
      if (prop === "getStats") {
        return getStats;
      }
      if (prop === "reset") {
        return reset;
      }
      if (prop === "render") {
        return async (options: RenderOptions) => {
          totalPendingJobs++;
          const startTime = Date.now();
          try {
            const result = await target.execute("render", options as any);
            completedJobs++;
            totalJobTimeMs += Date.now() - startTime;
            return result;
          } catch (e) {
            failedJobs++;
            throw e;
          } finally {
            totalPendingJobs--;
          }
        };
      }

      if (prop in target) {
        return Reflect.get(target, prop, receiver);
      }
      return async (...args: any[]) => {
        totalPendingJobs++;
        try {
          return await target.execute(prop as any, ...args);
        } finally {
          totalPendingJobs--;
        }
      };
    },
  }) as unknown as Omit<typeof workerInstance, "execute"> &
    SatoruWorker & { getStats: () => WorkerPoolStats; reset: () => void };

  return proxy;
};

const defaultWorker = createSatoruWorker({ maxParallel: 1 });

export const { close, render, launchWorker, setLimit, waitAll, waitReady } =
  defaultWorker;

export const reset = () => defaultWorker.reset();

export const getStats = () => defaultWorker.getStats();
