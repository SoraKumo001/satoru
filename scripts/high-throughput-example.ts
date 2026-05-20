import { createSatoruWorker } from "satoru-render/workers";
import { LogLevel } from "satoru-render";

async function run() {
  console.log("🚀 Starting High-Throughput Worker Pool Example...");

  // 1. Create a worker pool with 4 parallel worker threads, a job timeout, and a log listener
  const worker = createSatoruWorker({
    maxParallel: 4,
    timeoutMs: 5000, // 5 seconds per-job timeout limit
    onWorkerLog: (level, message) => {
      // Intercept worker diagnostics logs
      if (level === LogLevel.Info) {
        console.log(`[Worker Log: INFO] ${message.trim()}`);
      } else if (level === LogLevel.Error) {
        console.error(`[Worker Log: ERROR] ${message.trim()}`);
      }
    }
  });

  const totalJobs = 50;
  const promises: Promise<any>[] = [];

  console.log(`📦 Queueing ${totalJobs} render jobs...`);

  const startTime = Date.now();

  // 2. Launch stats polling in the background to observe queue health
  const statsInterval = setInterval(() => {
    const stats = worker.getStats();
    console.log(
      `📊 Pool Stats - Active: ${stats.activeJobs}, Queued: ${stats.queuedJobs}, Completed: ${stats.completedJobs}/${totalJobs}, Failed: ${stats.failedJobs}, AvgTime: ${stats.avgJobTimeMs.toFixed(1)}ms`
    );
  }, 100);

  // 3. Dispatch 50 render jobs concurrently
  for (let i = 0; i < totalJobs; i++) {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            body { font-family: sans-serif; background: #fafafa; padding: 20px; }
            .card { background: white; border-radius: 8px; padding: 15px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
            h1 { color: #333; font-size: 18px; margin: 0 0 10px 0; }
            p { color: #666; margin: 0; }
          </style>
        </head>
        <body>
          <div class="card">
            <h1>Card #${i + 1}</h1>
            <p>Generated dynamically in a multi-threaded worker pool environment.</p>
          </div>
        </body>
      </html>
    `;

    promises.push(
      worker.render({
        value: html,
        width: 400,
        height: 200,
        format: "svg",
        logLevel: LogLevel.Info, // This will trigger worker logging
      }).catch(err => {
        console.error(`❌ Job #${i + 1} failed: ${err.message}`);
        return null;
      })
    );
  }

  // 4. Wait for all jobs to complete
  await Promise.all(promises);

  clearInterval(statsInterval);

  const duration = Date.now() - startTime;
  const stats = worker.getStats();

  console.log("\n✨ All jobs finished!");
  console.log(`⏱️ Total Time: ${(duration / 1000).toFixed(2)} seconds`);
  console.log(`📈 Final Stats:`);
  console.log(`  - Concurrency (maxParallel): ${stats.workerCount}`);
  console.log(`  - Completed Jobs: ${stats.completedJobs}`);
  console.log(`  - Failed Jobs: ${stats.failedJobs}`);
  console.log(`  - Average Job Time: ${stats.avgJobTimeMs.toFixed(1)}ms`);

  // 5. Clean up and close worker threads
  console.log("🧹 Closing worker pool...");
  worker.close();
  console.log("✅ Worker pool closed.");
}

run().catch(console.error);
