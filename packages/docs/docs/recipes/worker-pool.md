---
sidebar_position: 7
title: Worker Pool
---

# Worker Pool で大量ジョブを処理する

大量の OGP 画像や thumbnail を生成する場合は、`satoru-render/workers` の worker pool を使うと render job を並列化できます。

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

## Queue の状態を見る

長時間動く service では `getStats()` を metrics に出すと、詰まりや timeout を検知しやすくなります。

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

## 注意点

- `maxParallel` は CPU core 数や memory budget に合わせます。大きくしすぎると Wasm instance と resource loading で memory pressure が高くなります。
- Process 終了時は `waitAll()` で実行中 job を待ち、最後に `close()` します。
- 1 job ごとに `limits.timeoutMs` を設定し、全体の queue 停滞を避けます。
- 同じ font や画像を大量 job で使う場合は、resource cache も併用します。
