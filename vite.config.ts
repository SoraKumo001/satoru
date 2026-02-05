import { defineConfig } from "vite";

export default defineConfig({
  base: "./",
  server: {
    port: 3000,
    open: true,
    watch: {
      ignored: ["**/!(src|public)/**"],
    },
  },
  build: {
    outDir: "dist",
    assetsDir: "assets",
  },
});
