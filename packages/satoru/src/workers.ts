import { createWorker, Worker } from "worker-lib";
import type { SatoruWorker } from "./child-workers.js";
export type { SatoruWorker } from "./child-workers.js";
import { type RenderOptions, type WorkerPoolStats } from "./core.js";
export { type RenderOptions } from "./core.js";
export { Satoru } from "./index.js";
export * from "./index.js";
import { LogLevel } from "./log-level.js";
export * from "./log-level.js";

/**
 * Create a Satoru worker proxy using worker-lib.
 * @param params Initialization parameters
 * @param params.worker Optional: Path to the worker file, a URL, or a factory function.
 *                      Defaults to the bundled workers.js in the same directory.
 * @param params.maxParallel Maximum number of parallel workers
 */
/**
 * Create a Satoru worker proxy using worker-lib.
 * @param params Initialization parameters
 * @param params.worker Optional: Path to the worker file, a URL, or a factory function.
 *                      Defaults to the bundled workers.js in the same directory.
 * @param params.maxParallel Maximum number of parallel workers (default: 4)
 * @param params.timeoutMs Optional: Default timeout in milliseconds for each render job.
 *                         If a render job times out, the pool is reset to prevent hung workers.
 * @param params.onWorkerLog Optional: Global callback to intercept logs emitted by the worker threads.
 */
export const createSatoruWorker = (params?: {
  worker?: string | URL | (() => Worker | string | URL);
  maxParallel?: number;
  timeoutMs?: number;
  onWorkerLog?: (level: LogLevel, message: string) => void;
}) => {
  const { worker, maxParallel = 4, timeoutMs, onWorkerLog } = params ?? {};

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

  /**
   * Retrieve operational stats of the worker pool.
   */
  const getStats = (): WorkerPoolStats => ({
    workerCount: maxParallel,
    activeJobs: Math.min(totalPendingJobs, maxParallel),
    queuedJobs: Math.max(0, totalPendingJobs - maxParallel),
    completedJobs,
    failedJobs,
    avgJobTimeMs: completedJobs > 0 ? totalJobTimeMs / completedJobs : 0,
  });

  /**
   * Reset the worker pool by terminating all running workers and recreating them.
   * Useful to clear hung workers or reset statistics.
   */
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
          const jobTimeoutMs = options.limits?.timeoutMs ?? timeoutMs;

          // Merge the user's specific onLog callback with the global onWorkerLog callback
          const originalOnLog = options.onLog;
          const mergedOnLog = (level: LogLevel, message: string) => {
            if (originalOnLog) {
              originalOnLog(level, message);
            }
            if (onWorkerLog) {
              onWorkerLog(level, message);
            }
          };

          const mergedOptions = {
            ...options,
            onLog: mergedOnLog,
          };

          let timeoutId: any;
          let timeoutPromise: Promise<never> | undefined;

          if (jobTimeoutMs !== undefined && jobTimeoutMs > 0) {
            timeoutPromise = new Promise<never>((_, reject) => {
              timeoutId = setTimeout(() => {
                // If a job times out, terminate and recreate workers to recover from a potential hang
                reset();
                reject(new Error(`Render timed out after ${jobTimeoutMs}ms`));
              }, jobTimeoutMs);
            });
          }

          try {
            const executePromise = target.execute("render", mergedOptions as any);
            const result = await (timeoutPromise
              ? Promise.race([executePromise, timeoutPromise])
              : executePromise);

            completedJobs++;
            totalJobTimeMs += Date.now() - startTime;
            return result;
          } catch (e) {
            failedJobs++;
            throw e;
          } finally {
            if (timeoutId) {
              clearTimeout(timeoutId);
            }
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
    SatoruWorker & {
      /**
       * Retrieve operational stats of the worker pool.
       */
      getStats: () => WorkerPoolStats;
      /**
       * Reset the worker pool by terminating all running workers and recreating them.
       */
      reset: () => void;
      /**
       * Close and terminate all workers in the pool immediately.
       */
      close: () => void;
      /**
       * Wait for all running tasks in the pool to complete.
       */
      waitAll: () => Promise<void>;
      /**
       * Wait until there is an available worker slot in the pool.
       */
      waitReady: (retryTime?: number) => Promise<void>;
    };
  return proxy;
};

const defaultWorker = createSatoruWorker({ maxParallel: 1 });

export const { close, render, launchWorker, setLimit, waitAll, waitReady } =
  defaultWorker;

export const reset = () => defaultWorker.reset();

export const getStats = () => defaultWorker.getStats();
