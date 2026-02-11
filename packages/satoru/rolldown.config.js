import { defineConfig } from "rolldown";

export default defineConfig({
  input: "src/workers/web-workers.ts",
  output: {
    file: "dist/workers/web-workers.js",
    format: "esm",
    codeSplitting: false,
  },
  resolve: {
    extensionAlias: {
      ".js": [".ts", ".js"],
    },
  },
});
