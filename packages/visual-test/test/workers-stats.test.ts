import { describe, it, expect, beforeAll, afterAll } from "vitest";
import { createSatoruWorker } from "satoru-render/workers";

describe("Worker Pool Stats", () => {
  const worker = createSatoruWorker({ maxParallel: 2 });

  afterAll(() => {
    worker.close();
  });

  it("should track job completion and timings", async () => {
    const stats0 = worker.getStats();
    expect(stats0.completedJobs).toBe(0);

    await worker.render({
      value: "<div>Test</div>",
      width: 400,
    });

    const stats1 = worker.getStats();
    expect(stats1.completedJobs).toBe(1);
    expect(stats1.avgJobTimeMs).toBeGreaterThan(0);
  });

  it("should track concurrent jobs", async () => {
    // Start multiple renders without awaiting them immediately
    const p1 = worker.render({ value: "<div>1</div>", width: 400 });
    const p2 = worker.render({ value: "<div>2</div>", width: 400 });
    const p3 = worker.render({ value: "<div>3</div>", width: 400 });

    // Jobs may complete before getStats() is called, so either they're
    // still in-flight or already accounted for in completedJobs.
    const stats = worker.getStats();
    const accounted = stats.completedJobs + stats.activeJobs + stats.queuedJobs;
    expect(accounted).toBeGreaterThanOrEqual(3);

    await Promise.all([p1, p2, p3]);

    const statsEnd = worker.getStats();
    expect(statsEnd.activeJobs).toBe(0);
    expect(statsEnd.queuedJobs).toBe(0);
    expect(statsEnd.completedJobs).toBe(4); // 1 from previous test + 3
  });

  it("should reset stats and workers", async () => {
    worker.reset();
    const stats = worker.getStats();
    expect(stats.completedJobs).toBe(0);
    expect(stats.activeJobs).toBe(0);
  });
});
