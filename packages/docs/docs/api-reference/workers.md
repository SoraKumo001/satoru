---
sidebar_position: 5
title: Worker Pool
---

# Worker Pool

高スループットな Node.js 処理では `satoru-render/workers` を使います。

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

| API | 説明 |
| --- | --- |
| `createSatoruWorker({ maxParallel, timeoutMs, worker, onWorkerLog })` | worker pool を作成します。 |
| `satoru.render(options)` | worker 上で render します。 |
| `satoru.waitAll()` | dispatch 済み job の完了を待ちます。 |
| `satoru.waitReady(retryTime?)` | worker slot が空くまで待ちます。 |
| `satoru.close()` | worker を終了します。 |
| `satoru.reset()` | worker pool を再作成し、統計を reset します。 |
| `satoru.getStats()` | `workerCount`、`activeJobs`、`queuedJobs`、`completedJobs` などを返します。 |
