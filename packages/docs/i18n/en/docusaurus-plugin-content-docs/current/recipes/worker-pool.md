---
sidebar_position: 7
title: Worker Pool
---

# Process Many Jobs with a Worker Pool

Use `satoru-render/workers` when generating many OGP images or thumbnails. The worker pool lets you run render jobs in parallel.

## Recipe

```typescript
import { createSatoruWorker } from "satoru-render/workers";

const satoru = createSatoruWorker({
  maxParallel: 4,
  timeoutMs: 10_000,
  onWorkerLog: (level, message) => {
    console.log(`[satoru:${level}] ${message}`);
  },
});

export async function renderMany(items: { id: string; html: string }[]) {
  try {
    const jobs = items.map(async (item) => {
      const png = await satoru.render({
        value: item.html,
        width: 1200,
        height: 630,
        format: "png",
        limits: {
          timeoutMs: 5000,
          maxResourceCount: 20,
          allowedProtocols: ["https:"],
        },
      });

      return {
        id: item.id,
        png,
      };
    });

    return await Promise.all(jobs);
  } finally {
    await satoru.waitAll();
  }
}

process.once("SIGTERM", async () => {
  await satoru.waitAll();
  satoru.close();
});
```

## Inspect Queue State

For long-running services, export `getStats()` to your metrics system so you can spot queue buildup and timeouts.

```typescript
setInterval(() => {
  const stats = satoru.getStats();
  console.log({
    active: stats.activeJobs,
    queued: stats.queuedJobs,
    completed: stats.completedJobs,
    failed: stats.failedJobs,
  });
}, 10_000);
```

## Notes

- Tune `maxParallel` to your CPU core count and memory budget. Too much parallelism can increase memory pressure.
- On shutdown, call `waitAll()` for running jobs, then `close()`.
- Set `limits.timeoutMs` for each job to avoid queue stalls.
- Pair the worker pool with resource caching when many jobs reuse the same fonts or images.
