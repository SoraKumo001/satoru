import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    pool: "threads",
    maxWorkers: "100%",
    testTimeout: 60000,
  },
});
