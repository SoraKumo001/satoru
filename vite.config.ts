import { defineConfig } from "vite";

export default defineConfig({
  base: "./",
  server: {
    port: 3000,
    open: true,
    watch: {
      ignored: [
        "**/node_modules/**",
        "**/external/**",
        "**/build-wasm/**",
        "**/vcpkg_installed/**",
        "**/temp/**",
        "**/dist/**",
      ],
    },
  },
  build: {
    outDir: "dist",
    assetsDir: "assets",
  },
});
