# worker-lib

[![](https://img.shields.io/npm/l/worker-lib)](https://www.npmjs.com/package/worker-lib)
[![](https://img.shields.io/npm/v/worker-lib)](https://www.npmjs.com/package/worker-lib)
[![](https://img.shields.io/npm/dw/worker-lib)](https://www.npmjs.com/package/worker-lib)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/SoraKumo001/worker-lib)

## Overview

`worker-lib` is a lightweight, type-safe library designed to make **Web Workers** (Browser) and **worker_threads** (Node.js) as easy to use as standard asynchronous functions.

## Features

- **🚀 Universal API**: Supports both Browser and Node.js with a single unified entry point.
- **🛡️ Type-Safe**: Full TypeScript support with automatic type inference for worker functions.
- **⚡ Parallelism**: Built-in worker pool management with configurable concurrency limits.
- **🔄 Deep Proxying**: Automatically converts functions within objects or arrays into bidirectional proxies. Binary data (`Uint8Array`, `ArrayBuffer`, `TypedArray`, etc.) are correctly identified and transferred as data. No more `DataCloneError`.
- **🔄 Bidirectional Callbacks**: Supports passing callbacks from main to worker, and vice versa, with full `async/await` support.
- **📦 Flexible Instantiation**: Pass a `Worker` instance, a file path (string), or a `URL`.
- **📦 Zero Config**: Minimal setup required to get started.

## Installation

```bash
npm install worker-lib
# or
pnpm add worker-lib
```

## Basic Usage

### 1. Define Worker (worker.ts)

Register your functions using `initWorker`. You can now pass complex objects containing functions.

```ts
import { initWorker } from "worker-lib";

interface RenderOptions {
  html: string;
  resolveResource: (url: string, fallback: (url: string) => Promise<Uint8Array>) => Promise<Uint8Array>;
}

const render = async (options: RenderOptions) => {
  // options.resolveResource is a proxy to the main thread!
  // It even receives a second argument 'fallback' which is a proxy back to the worker!
  const data = await options.resolveResource("logo.png", async (url) => {
    return new Uint8Array([1, 2, 3]); // Worker-side fallback logic
  });
  return `Rendered with ${data.length} bytes`;
};

const workerMap = initWorker({ render });
export type MyWorker = typeof workerMap;
```

### 2. Use Worker in Main Thread

#### Unified Usage (Browser & Node.js)

Since `worker-lib` uses deep proxying, you can pass nested functions directly.

```ts
import { createWorker } from "worker-lib";
import type { MyWorker } from "./worker";

const { execute } = createWorker<MyWorker>(
  () => new URL("./worker.ts", import.meta.url),
  4
);

const result = await execute("render", {
  html: "<div>Hello</div>",
  resolveResource: async (url, fallback) => {
    if (url === "special.png") return await fallback(url); // Call back to worker!
    const resp = await fetch(url);
    return new Uint8Array(await resp.arrayBuffer());
  }
});
console.log(result);
```

#### Node.js Specific (Optional)

If you need to use `node:worker_threads` features explicitly:

```ts
import { Worker } from "worker-lib"; // Automatically uses node:worker_threads in Node.js
import { createWorker } from "worker-lib";
import path from "node:path";

const { execute } = createWorker(
  () => new Worker(path.resolve(__dirname, "./worker.js")),
  2
);
```

## API Reference

### `initWorker(workerProcess)`
Initializes the worker side.
- `workerProcess`: An object containing the functions to be exposed.

### `createWorker(builder, limit?)`
Creates a worker pool.
- `builder`: A function that returns a `Worker`, `string` (path), or `URL`.
- `limit`: (Optional) Maximum number of concurrent workers. Default is `4`.

Returns an object with:
- `execute(name, ...args)`: Executes a worker function.
- `waitAll()`: Waits for all running tasks to complete.
- `waitReady(retryTime?)`: Waits for an available worker slot.
- `launchWorker()`: Pre-launches all workers in the pool.
- `setLimit(limit)`: Dynamically changes the worker pool size.
- `close()`: Terminates all workers.

### `execute(name, ...args)`
Executes a worker function.
- `name`: The name of the function to execute.
- `args`: Arguments to pass to the function (supports callbacks).

### `waitAll()`
Returns a promise that resolves when all currently running tasks are complete.

### `waitReady(retryTime?)`
Returns a promise that resolves when there is an available slot in the worker pool.
- `retryTime`: (Optional) Milliseconds to wait between checks.

### `launchWorker()`
Forcefully initializes all workers up to the `limit`. By default, workers are created lazily.

### `setLimit(limit)`
Changes the maximum number of concurrent workers. This will terminate existing workers and reset the pool.

### `close()`
Terminates all workers in the pool immediately.

### `Worker`
The environment-specific Worker class (Web Worker in browser, `worker_threads` in Node.js).

## Examples

For more detailed examples, check the [samples repository](https://github.com/SoraKumo001/worker-lib-samples/).
