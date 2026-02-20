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
- **🔄 Callback Support**: Pass functions as arguments to workers for progress updates or event handling.
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

Register your functions using `initWorker`. This function is universal and works in both Browser and Node.js.

```ts
import { initWorker } from "worker-lib";

const add = (a: number, b: number) => a + b;

const heavyTask = async (data: string, onProgress: (percent: number, status: string) => void) => {
  onProgress(10, "Starting...");
  // ... heavy computation ...
  onProgress(100, "Done");
  return `Processed: ${data}`;
};

const workerMap = initWorker({ add, heavyTask });
export type MyWorker = typeof workerMap;
```

### 2. Use Worker in Main Thread

#### Unified Usage (Browser & Node.js)

Since `worker-lib` uses conditional exports, you can use the same import for both environments. The library will automatically select the appropriate implementation.

```ts
import { createWorker } from "worker-lib";
import type { MyWorker } from "./worker";

// You can pass a string path, a URL, or a Worker instance
const { execute, close } = createWorker<MyWorker>(
  () => new URL("./worker.ts", import.meta.url), // or "./worker.js" in Node
  4 // Max parallel workers
);

const result = await execute("add", 10, 20);
console.log(result); // 30
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
