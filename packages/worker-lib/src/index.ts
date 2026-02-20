import type { Worker as WorkerNode } from "node:worker_threads";

const w = Worker;
export { w as Worker };

type WorkerType = { [key: string]: (...args: any) => any };

type WorkerRecvEvent<T extends WorkerType> =
  | {
      type: "function";
      payload: { id: number; name: keyof T; args: unknown };
    }
  | {
      type: "callback_result";
      payload: { id: string | number; result: unknown };
    }
  | {
      type: "callback_call";
      payload: { id: number; callbackId: string; args: unknown; callId: string };
    };

type WorkerSendEvent<T extends WorkerType> =
  | {
      type: "result";
      payload: { id: number; result: unknown };
    }
  | {
      type: "error";
      payload: { id: number; error: unknown };
    }
  | {
      type: "callback_call";
      payload: { id: number; callbackId: string; args: unknown; callId: string };
    }
  | {
      type: "callback_result";
      payload: { id: string | number; result: unknown };
    };

const FUNCTION_PLACEHOLDER = "__worker_lib_function__";

interface FunctionPlaceholder {
  [FUNCTION_PLACEHOLDER]: string;
}

const isPlaceholder = (v: any): v is FunctionPlaceholder =>
  v && typeof v === "object" && FUNCTION_PLACEHOLDER in v;

const callbacks = new Map<string, Function>();
const callbackProxies = new Map<string, Function>();

const registerCallback = (requestId: number, fn: Function) => {
  const id = `${requestId}:${Math.random().toString(36).slice(2)}`;
  callbacks.set(id, fn);
  return id;
};

const clearCallbacks = (requestId: number) => {
  for (const key of callbacks.keys()) {
    if (key.startsWith(`${requestId}:`)) {
      callbacks.delete(key);
    }
  }
  for (const key of callbackProxies.keys()) {
    if (key.startsWith(`${requestId}:`)) {
      callbackProxies.delete(key);
    }
  }
};

const isPlainObject = (v: any): boolean => {
  return (
    v &&
    typeof v === "object" &&
    Object.prototype.toString.call(v) === "[object Object]" &&
    !(v instanceof Uint8Array) &&
    !(v instanceof ArrayBuffer) &&
    !ArrayBuffer.isView(v)
  );
};

const transformArgs = (requestId: number, args: any): any => {
  if (typeof args === "function") {
    return { [FUNCTION_PLACEHOLDER]: registerCallback(requestId, args) };
  }
  if (Array.isArray(args)) {
    return args.map((v) => transformArgs(requestId, v));
  }
  if (isPlainObject(args)) {
    const result: any = {};
    for (const key in args) {
      result[key] = transformArgs(requestId, args[key]);
    }
    return result;
  }
  return args;
};

const resolveArgs = (
  requestId: number,
  args: any,
  createProxy: (callbackId: string) => Function,
): any => {
  if (isPlaceholder(args)) {
    return createProxy(args[FUNCTION_PLACEHOLDER]);
  }
  if (Array.isArray(args)) {
    return args.map((v) => resolveArgs(requestId, v, createProxy));
  }
  if (isPlainObject(args)) {
    const result: any = {};
    for (const key in args) {
      result[key] = resolveArgs(requestId, args[key], createProxy);
    }
    return result;
  }
  return args;
};

const init = (worker: Worker): Promise<Worker> => {
  return new Promise((resolve) => {
    const handler = (e: MessageEvent) => {
      if (e.data === "ready") {
        worker.removeEventListener("message", handler);
        resolve(worker);
      }
    };
    worker.addEventListener("message", handler);
  });
};

let requestIdCounter = 0;

const exec = <T extends WorkerType>(
  worker: Worker,
  name: keyof T,
  ...args: Parameters<T[keyof T]>
): Promise<ReturnType<T[keyof T]>> => {
  const requestId = requestIdCounter++;
  return new Promise((resolve, reject) => {
    const createProxy = (callbackId: string): Function => {
      const key = `${requestId}:${callbackId}`;
      if (callbackProxies.has(key)) return callbackProxies.get(key)!;
      const proxy = (...proxyArgs: any[]) => {
        const callId = Math.random().toString(36).slice(2);
        return new Promise((res) => {
          const handler = (e: MessageEvent<WorkerSendEvent<T>>) => {
            if (
              e.data.type === "callback_result" &&
              e.data.payload.id === callId
            ) {
              worker.removeEventListener("message", handler);
              res(resolveArgs(requestId, e.data.payload.result, createProxy));
            }
          };
          worker.addEventListener("message", handler);
          worker.postMessage({
            type: "callback_call",
            payload: {
              id: requestId,
              callbackId,
              args: transformArgs(requestId, proxyArgs),
              callId,
            },
          });
        });
      };
      callbackProxies.set(key, proxy);
      return proxy;
    };

    const messageHandler = async (e: MessageEvent<WorkerSendEvent<T>>) => {
      const data = e.data;
      if (!data || typeof data !== "object") return;

      const payload = (data as any).payload;
      if (payload?.id !== requestId) return; // Ignore messages for other requests

      if (data.type === "result") {
        worker.removeEventListener("message", messageHandler);
        const result = resolveArgs(requestId, payload.result, createProxy);
        clearCallbacks(requestId);
        resolve(result);
      } else if (data.type === "error") {
        worker.removeEventListener("message", messageHandler);
        clearCallbacks(requestId);
        reject(payload.error);
      } else if (data.type === "callback_call") {
        const { callbackId, args: callArgs, callId } = payload;
        const fn = callbacks.get(callbackId);
        if (fn) {
          try {
            const result = await fn(
              ...resolveArgs(requestId, callArgs, createProxy),
            );
            worker.postMessage({
              type: "callback_result",
              payload: { id: callId, result: transformArgs(requestId, result) },
            });
          } catch (e) {
            console.error("[worker-lib] Callback execution failed:", e);
          }
        }
      }
    };

    worker.addEventListener("message", messageHandler);
    worker.postMessage({
      type: "function",
      payload: {
        id: requestId,
        name,
        args: transformArgs(requestId, args),
      },
    });
  });
};

/**
 * Creates a worker pool with a specified limit of concurrent workers.
 *
 * @template T - The type of the worker.
 * @param {() => Worker | WorkerNode} builder - A function that returns a new Worker or WorkerNode instance.
 * @param {number} [limit=4] - The maximum number of concurrent workers.
 * @returns {{
 *   execute: <K extends keyof T>(name: K, ...value: Parameters<T[K]>) => Promise<Awaited<ReturnType<T[K]>>>,
 *   waitAll: () => Promise<void>,
 *   waitReady: (retryTime?: number) => Promise<void>,
 *   close: () => void,
 *   setLimit: (limit: number) => void,
 *   launchWorker: () => Promise<void[]>
 * }} An object containing methods to interact with the worker pool.
 */
export const createWorker = <T extends WorkerType>(
  builder: () => Worker | WorkerNode | string | URL,
  limit = 4,
) => {
  let workers: {
    worker?: Worker;
    resultResolver?: PromiseWithResolvers<unknown>;
  }[] = Array(limit)
    .fill(undefined)
    .map(() => ({}));
  const emptyWaits: PromiseWithResolvers<void>[] = [];
  let isEmptyWait = false;

  const getResolver = async () => {
    while (true) {
      const target = workers.find(({ resultResolver }) => !resultResolver);
      if (target) {
        target.resultResolver = Promise.withResolvers<unknown>();
        if (!target.worker) {
          const result = builder();
          const worker =
            result instanceof Worker
              ? result
              : new Worker(result as string | URL);
          target.worker = await init(worker);
        }
        return target;
      }
      await Promise.race(
        workers.map(({ resultResolver }) => resultResolver?.promise),
      );
    }
  };

  /**
   * @method execute - Executes a method on a worker.
   * @template K - The key of the method to execute.
   * @param {K} name - The name of the method to execute.
   * @param {...Parameters<T[K]>} value - The arguments to pass to the method.
   * @returns {Promise<Awaited<ReturnType<T[K]>>>} A promise that resolves with the result of the method.
   */
  const execute = async <K extends keyof T>(
    name: K,
    ...value: Parameters<T[K]>
  ): Promise<Awaited<ReturnType<T[K]>>> => {
    const target = await getResolver();
    const { resultResolver } = target;
    if (!resultResolver) throw new Error("Unexpected error");
    exec(target.worker!, name as string, ...value)
      .then(resultResolver.resolve)
      .catch(resultResolver.reject)
      .finally(() => {
        target.resultResolver = undefined;
      });
    return resultResolver.promise as Promise<Awaited<ReturnType<T[K]>>>;
  };

  /**
   * @method launchWorker - Launches all workers in the pool.
   * @returns {Promise<void[]>} A promise that resolves when all workers have been launched.
   */
  const launchWorker = async () => {
    return Promise.all(
      workers.map(async (target) => {
        if (!target.worker) {
          const result = builder();
          const worker =
            result instanceof Worker
              ? result
              : new Worker(result as string | URL);
          target.worker = await init(worker);
        }
      }),
    );
  };

  /**
   * @method waitAll - Waits for all workers to complete their tasks.
   * @returns {Promise<void>} A promise that resolves when all workers have completed their tasks.
   */
  const waitAll = async () => {
    while (workers.find(({ resultResolver }) => resultResolver)) {
      await Promise.all(
        workers.flatMap(({ resultResolver }) =>
          resultResolver ? [resultResolver.promise] : [],
        ),
      );
    }
  };

  /**
   * @method waitReady - Waits for the worker pool to be ready.
   * @param {number} [retryTime=1] - The time to wait between retries in milliseconds.
   * @returns {Promise<void>} A promise that resolves when the worker pool is ready.
   */
  const waitReady = async (retryTime = 1) => {
    const p = Promise.withResolvers<void>();
    emptyWaits.push(p);
    (async () => {
      if (!isEmptyWait) {
        isEmptyWait = true;
        do {
          const actives = workers.flatMap(({ resultResolver }) =>
            resultResolver ? [resultResolver.promise] : [],
          );
          if (workers.length === actives.length) await Promise.race(actives);
          emptyWaits.shift()?.resolve();
          if (retryTime) await new Promise((r) => setTimeout(r, retryTime));
          else await Promise.resolve();
        } while (emptyWaits.length);
        isEmptyWait = false;
      }
    })();
    return p.promise;
  };

  /**
   * @method close - Terminates all workers in the pool.
   */
  const close = () => {
    for (const target of workers) {
      target.worker?.terminate();
      target.worker = undefined;
      target.resultResolver = undefined;
    }
  };

  /**
   * @method setLimit - Sets a new limit for the number of concurrent workers.
   * @param {number} limit - The new limit for the number of concurrent workers.
   */
  const setLimit = (limit: number) => {
    workers.forEach((w) => w.worker?.terminate());
    workers = Array(limit)
      .fill(undefined)
      .map(() => ({}));
  };
  return { execute, waitAll, waitReady, close, setLimit, launchWorker };
};

/**
 * Initializes a web worker with the provided worker process.
 *
 * @template T - The type of the worker process.
 * @param {T} WorkerProc - The worker process to initialize.
 * @returns {T} The initialized worker process.
 */
export const initWorker = <T extends WorkerType>(WorkerProc: T) => {
  const worker = self as unknown as Worker;

  const callbacks = new Map<string, Function>();
  const callbackProxies = new Map<string, Function>();

  const registerCallback = (requestId: number, fn: Function) => {
    const id = `${requestId}:${Math.random().toString(36).slice(2)}`;
    callbacks.set(id, fn);
    return id;
  };

  const clearCallbacks = (requestId: number) => {
    for (const key of callbacks.keys()) {
      if (key.startsWith(`${requestId}:`)) {
        callbacks.delete(key);
      }
    }
    for (const key of callbackProxies.keys()) {
      if (key.startsWith(`${requestId}:`)) {
        callbackProxies.delete(key);
      }
    }
  };

  const transformArgs = (requestId: number, args: any): any => {
    if (typeof args === "function") {
      return { [FUNCTION_PLACEHOLDER]: registerCallback(requestId, args) };
    }
    if (Array.isArray(args)) {
      return args.map((v) => transformArgs(requestId, v));
    }
    if (isPlainObject(args)) {
      const result: any = {};
      for (const key in args) {
        result[key] = transformArgs(requestId, args[key]);
      }
      return result;
    }
    return args;
  };

  const resolveArgs = (
    requestId: number,
    args: any,
    createProxy: (callbackId: string) => Function,
  ): any => {
    if (isPlaceholder(args)) {
      return createProxy(args[FUNCTION_PLACEHOLDER]);
    }
    if (Array.isArray(args)) {
      return args.map((v) => resolveArgs(requestId, v, createProxy));
    }
    if (isPlainObject(args)) {
      const result: any = {};
      for (const key in args) {
        result[key] = resolveArgs(requestId, args[key], createProxy);
      }
      return result;
    }
    return args;
  };

  const createProxy = (callbackId: string, requestId: number): Function => {
    const key = `${requestId}:${callbackId}`;
    if (callbackProxies.has(key)) return callbackProxies.get(key)!;
    const proxy = (...proxyArgs: any[]) => {
      const callId = Math.random().toString(36).slice(2);
      return new Promise((res) => {
        const handler = (e: MessageEvent<WorkerRecvEvent<T>>) => {
          if (
            e.data.type === "callback_result" &&
            e.data.payload.id === callId
          ) {
            worker.removeEventListener("message", handler);
            res(
              resolveArgs(requestId, e.data.payload.result, (cbId) =>
                createProxy(cbId, requestId),
              ),
            );
          }
        };
        worker.addEventListener("message", handler);
        worker.postMessage({
          type: "callback_call",
          payload: {
            id: requestId,
            callbackId,
            args: transformArgs(requestId, proxyArgs),
            callId,
          },
        });
      });
    };
    callbackProxies.set(key, proxy);
    return proxy;
  };

  worker.addEventListener("message", async (e: MessageEvent) => {
    const data = e.data as WorkerRecvEvent<T>;
    if (!data) return;

    if (data.type === "function") {
      const { id, name, args } = data.payload;
      const proc = WorkerProc[name];
      if (proc) {
        try {
          const resolvedArgs = resolveArgs(id, args, (cbId) =>
            createProxy(cbId, id),
          );
          const result = await proc(...resolvedArgs);
          worker.postMessage({
            type: "result",
            payload: { id, result: transformArgs(id, result) },
          });
          clearCallbacks(id);
        } catch (error) {
          worker.postMessage({
            type: "error",
            payload: { id, error: String(error) },
          });
          clearCallbacks(id);
        }
      }
    } else if (data.type === "callback_call") {
      const { id, callbackId, args, callId } = data.payload;
      const fn = callbacks.get(callbackId);
      if (fn) {
        try {
          const result = await fn(
            ...resolveArgs(id, args, (cbId) => createProxy(cbId, id)),
          );
          worker.postMessage({
            type: "callback_result",
            payload: { id: callId, result: transformArgs(id, result) },
          });
        } catch (e) {
          console.error("[worker-lib] Worker-side callback failed:", e);
        }
      }
    }
  });

  worker.postMessage("ready");
  return WorkerProc;
};
