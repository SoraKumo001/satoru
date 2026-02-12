import { createWorker } from "worker-lib";
import type { SatoruWorker } from "./child-workers.js";
export type { SatoruWorker } from "./child-workers.js";

import { LogLevel } from "./index.js";
export { LogLevel } from "./index.js";
export type {
  SatoruOptions,
  SatoruModule,
  RequiredResource,
  ResourceResolver,
  RenderOptions,
} from "./index.js";

/**
 * Create a Satoru worker proxy using worker-lib.
 * @param params Initialization parameters
 * @param params.worker Optional: Path to the worker file, a URL, or a factory function.
 *                      Defaults to the bundled workers.js in the same directory.
 * @param params.maxParallel Maximum number of parallel workers
 * @param params.onLog Optional: Callback for logs forwarded from workers
 */
export const createSatoruWorker = (params: {
  worker?: string | URL | (() => Worker | string | URL);
  maxParallel?: number;
  onLog?: (level: LogLevel, message: string) => void;
}) => {
  const { worker, maxParallel = 4, onLog } = params;

  let NodeWorker: any;
  let isNode =
    typeof process !== "undefined" && process.versions && process.versions.node;

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
            "Worker is not initialized. Please wait for a moment or check your environment.",
          );
        }
        w = new NodeWorker(workerUrl, {
          execArgv: ["--import", "tsx"],
        });
      }
    }

    if (!w) throw new Error("Worker is not supported in this environment.");

    // Handle forwarded logs
    const logHandler = (msg: any) => {
      if (msg && msg.__satoru_log && onLog) {
        onLog(msg.level, msg.message);
      }
    };

    if (w.on) {
      // Node.js worker_threads
      w.on("message", logHandler);
    } else if ("addEventListener" in w) {
      // Web Worker
      w.addEventListener("message", (e: MessageEvent) => logHandler(e.data));
    }

    return w;
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
