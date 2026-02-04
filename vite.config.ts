import { defineConfig } from 'vite';

export default defineConfig({
  server: {
    fs: {
      allow: ['.', './dist']
    }
  },
  optimizeDeps: {
    // Prevent Vite from trying to optimize the Emscripten output
    exclude: ['../dist/wasm/main.js']
  },
  build: {
    commonjsOptions: {
      include: [/dist\/wasm\/main\.js/],
    }
  }
});