import { defineConfig } from "rolldown";

export default defineConfig({
  input: "src/workers/web-workers.ts",
  output: {
    file: "dist/workers/web-workers.js",
    format: "esm",
  },
  resolve: {
    extensionAlias: {
      ".js": [".ts", ".js"],
    },
  },
  plugins: [
    {
      name: "rewrite-satoru-single",
      renderChunk(code) {
        // Rolldownが計算した相対パスを、配布環境に合わせて書き換える
        return {
          code: code.replace("../../dist/satoru-single.js", "../satoru-single.js"),
        };
      },
    },
  ],
  external: [/satoru-single\.js$/],
});
