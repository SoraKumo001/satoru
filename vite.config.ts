import { defineConfig } from 'vite';

export default defineConfig({
  base: './',
  server: {
    port: 3000,
    open: true,
    watch: {
      ignored: ['**/external/**']
    },
    fs: {
      allow: ['.'],
      deny: ['.??*', '**/external/**']
    }
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
  }
});
