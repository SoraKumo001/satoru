import { defineConfig } from 'vite';

export default defineConfig({
  base: './',
  server: {
    port: 3000,
    open: true,
    fs: {
      allow: ['.']
    }
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
  }
});