import type { Worker as WorkerNode } from "node:worker_threads";

const w = Worker;
export { w as Worker };

type WorkerType = { [key: string]: (...args: any) => any };
type CallbackValue<T extends WorkerType> = Parameters<
  Extract<Parameters<T[keyof T]>[number], (...args: any) => any>
>;
type WorkerRecvEvent<T extends WorkerType> =
  | {
      type: "function";
      payload: { name: keyof T; callback: boolean[]; value: unknown[] };
    }
  | { type: "callback_result"; payload: { id: number; result: unknown } };
type WorkerSendEvent<T extends WorkerType> =
  | {
      type: "callback";
      payload: {
        id: number;
        result: unknown;
        index: number;
        value: CallbackValue<T>;
      };
    }
  | {
      type: "result";
      payload: ReturnType<T[keyof T]>;
    }
  | {
      type: "error";
      payload: unknown;
    };

const init = (worker: Worker): Promise<Worker> => {
  return new Promise((resolve) => {
    worker.addEventListener(
      "message",
      () => {
        resolve(worker);
      },
      { once: true },
    );
  });
};
const exec = <T extends WorkerType>(
  worker: Worker,
  name: keyof T,
  ...value: Parameters<T[keyof T]>
): Promise<ReturnType<T[keyof T]>> => {
  return new Promise((resolve, reject) => {
    const p = async (result: MessageEvent<WorkerSendEvent<T>>) => {
      const { data } = result;
      switch (data.type) {
        case "callback":
          const r = value[data.payload.index](...data.payload.value);
          worker.postMessage({
            type: "callback_result",
            payload: { id: data.payload.id, result: await r },
          });
          break;
        case "result":
          worker.removeEventListener("message", p);
          resolve(data.payload);
          break;
        case "error":
          worker.removeEventListener("message", p);
          reject(data.payload);
          break;
      }
    };
    worker.addEventListener("message", p);
    worker.postMessage({
      type: "function",
      payload: {
        name,
        value: value.map((v: unknown) => !(typeof v === "function") && v),
        callback: value.map((v: unknown) => typeof v === "function"),
      },
    } as WorkerRecvEvent<T>);
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
    for (const { worker } of workers) {
      worker?.terminate();
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
  worker.addEventListener("message", async (e: MessageEvent) => {
    const data = e.data as WorkerRecvEvent<T>;
    if (data.type === "function") {
      const {
        name,
        value,
        callback,
      }: {
        name: keyof T;
        value: unknown[];
        callback: boolean[];
      } = data.payload;
      const proc = WorkerProc[name];
      if (proc) {
        try {
          const params = value.map((v, index) =>
            callback[index]
              ? (...params: CallbackValue<T>) =>
                  callbackProc<T>(worker, index, params)
              : v,
          );
          worker.postMessage({
            type: "result",
            payload: await proc(...params),
          });
        } catch (e) {
          worker.postMessage({ type: "error", payload: String(e) });
        }
      }
    }
  });
  worker.postMessage(undefined);
  return WorkerProc;
};

const callbackProc = <T extends WorkerType>(
  worker: Worker,
  index: number,
  params: CallbackValue<T>,
) => {
  const id = WorkerValue.id++;
  return new Promise((resolve) => {
    worker.addEventListener(
      "message",
      (e: MessageEvent) => {
        const data = e.data as WorkerRecvEvent<T>;
        if (data.type === "callback_result" && data.payload.id === id) {
          resolve(data.payload.result);
        }
      },
      { once: true },
    );
    worker.postMessage({
      type: "callback",
      payload: { id, index, value: params },
    });
  });
};

const WorkerValue = { id: 0, promises: {} } as {
  id: number;
  promises: { [key: number]: Promise<unknown> };
};
