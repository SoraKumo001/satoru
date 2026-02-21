import { parentPort, Worker as WorkerNode } from "node:worker_threads";
export { Worker } from "node:worker_threads";

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
  if (ArrayBuffer.isView(args) || args instanceof ArrayBuffer) {
    return args;
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

const init = (worker: WorkerNode): Promise<WorkerNode> => {
  return new Promise((resolve) => {
    worker.once("message", () => {
      resolve(worker);
    });
  });
};

let requestIdCounter = 0;

const exec = <T extends WorkerType>(
  worker: WorkerNode,
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
          const handler = (data: WorkerSendEvent<T>) => {
            if (
              data.type === "callback_result" &&
              data.payload.id === callId
            ) {
              worker.removeListener("message", handler);
              res(resolveArgs(requestId, data.payload.result, createProxy));
            }
          };
          worker.addListener("message", handler);
          const transformedProxyArgs = transformArgs(requestId, proxyArgs);
          worker.postMessage({
            type: "callback_call",
            payload: {
              id: requestId,
              callbackId,
              args: transformedProxyArgs,
              callId,
            },
          });
        });
      };
      callbackProxies.set(key, proxy);
      return proxy;
    };

    const messageHandler = async (data: WorkerSendEvent<T>) => {
      if (!data || typeof data !== "object") return;

      const payload = (data as any).payload;
      if (payload?.id !== requestId) return;

      if (data.type === "result") {
        worker.removeListener("message", messageHandler);
        const result = resolveArgs(requestId, payload.result, createProxy);
        clearCallbacks(requestId);
        resolve(result);
      } else if (data.type === "error") {
        worker.removeListener("message", messageHandler);
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
            const transformedResult = transformArgs(requestId, result);
            worker.postMessage({
              type: "callback_result",
              payload: { id: callId, result: transformedResult },
            });
          } catch (e) {
            console.error("[worker-lib] Callback execution failed:", e);
          }
        }
      }
    };

    worker.addListener("message", messageHandler);
    const transformedArgs = transformArgs(requestId, args);
    worker.postMessage({
      type: "function",
      payload: {
        id: requestId,
        name,
        args: transformedArgs,
      },
    });
  });
};

/**
 * Creates a worker pool with a specified limit of concurrent workers.
 */
export const createWorker = <T extends WorkerType>(
  builder: () => WorkerNode | string | URL,
  limit = 4,
) => {
  let workers: {
    worker?: WorkerNode;
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
          const worker = result instanceof WorkerNode ? result : new WorkerNode(result);
          target.worker = await init(worker);
        }
        return target;
      }
      await Promise.race(
        workers.map(({ resultResolver }) => resultResolver?.promise),
      );
    }
  };

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

  const launchWorker = async () => {
    return Promise.all(
      workers.map(async (target) => {
        if (!target.worker) {
          const result = builder();
          const worker = result instanceof WorkerNode ? result : new WorkerNode(result);
          target.worker = await init(worker);
        }
      }),
    );
  };

  const waitAll = async () => {
    while (workers.find(({ resultResolver }) => resultResolver)) {
      await Promise.all(
        workers.flatMap(({ resultResolver }) =>
          resultResolver ? [resultResolver.promise] : [],
        ),
      );
    }
  };

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

  const close = () => {
    for (const { worker } of workers) {
      worker?.terminate();
    }
  };

  const setLimit = (limit: number) => {
    workers.forEach((w) => w.worker?.terminate());
    workers = Array(limit)
      .fill(undefined)
      .map(() => ({}));
  };
  return { execute, waitAll, waitReady, close, setLimit, launchWorker };
};

export const initWorker = <T extends WorkerType>(WorkerProc: T) => {
  const worker = parentPort;
  if (!worker) {
    throw new Error("This is not a worker thread");
  }

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

  const createProxy = (callbackId: string, requestId: number): Function => {
    const key = `${requestId}:${callbackId}`;
    if (callbackProxies.has(key)) return callbackProxies.get(key)!;
    const proxy = (...proxyArgs: any[]) => {
      const callId = Math.random().toString(36).slice(2);
      return new Promise((res) => {
        const handler = (data: WorkerRecvEvent<T>) => {
          if (
            data.type === "callback_result" &&
            data.payload.id === callId
          ) {
            worker.removeListener("message", handler);
            res(
              resolveArgs(requestId, data.payload.result, (cbId) =>
                createProxy(cbId, requestId),
              ),
            );
          }
        };
        worker.addListener("message", handler);
        const transformedArgs = transformArgs(requestId, proxyArgs);
        worker.postMessage({
          type: "callback_call",
          payload: {
            id: requestId,
            callbackId,
            args: transformedArgs,
            callId,
          },
        });
      });
    };
    callbackProxies.set(key, proxy);
    return proxy;
  };

  worker.on("message", async (data: WorkerRecvEvent<T>) => {
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
          const transformedResult = transformArgs(id, result);
          worker.postMessage({
            type: "result",
            payload: { id, result: transformedResult },
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
          const transformedResult = transformArgs(id, result);
          worker.postMessage({
            type: "callback_result",
            payload: { id: callId, result: transformedResult },
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
