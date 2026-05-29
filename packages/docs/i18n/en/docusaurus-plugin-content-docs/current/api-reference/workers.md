---
sidebar_position: 5
title: Worker Pool
---

# Worker Pool

Use `satoru-render/workers` for high-throughput Node.js workloads.

```typescript
import { createSatoruWorker } from "satoru-render/workers";

const satoru = createSatoruWorker({
  maxParallel: 4,
  timeoutMs: 5000,
});

try {
  const png = await satoru.render({
    value: html,
    width: 1200,
    height: 630,
    format: "png",
  });
} finally {
  await satoru.waitAll();
  satoru.close();
}
```

| API | Description |
| --- | --- |
| `createSatoruWorker({ maxParallel, timeoutMs, worker, onWorkerLog })` | Creates a worker pool. |
| `satoru.render(options)` | Renders in a worker. |
| `satoru.waitAll()` | Waits for all dispatched jobs to finish. |
| `satoru.waitReady(retryTime?)` | Waits until a worker slot is available. |
| `satoru.close()` | Terminates workers. |
| `satoru.reset()` | Recreates the worker pool and resets stats. |
| `satoru.getStats()` | Returns `workerCount`, `activeJobs`, `queuedJobs`, `completedJobs`, and related stats. |
