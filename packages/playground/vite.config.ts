import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import path from "path";
import fs from "fs";
import { fileURLToPath } from "url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

export default defineConfig({
  base: "./",
  server: {
    port: 3000,
    open: true,
    fs: {
      allow: ["..", "../../assets", "../satoru"],
    },
  },
  resolve: {
    alias: {
      "/assets": path.resolve(__dirname, "../../assets"),
    },
  },
  plugins: [
    react(),
    {
      name: "handle-satoru-artifacts",
      // 開発サーバーでの配信
      configureServer(server) {
        server.middlewares.use((req, res, next) => {
          if (req.url === "/satoru.js" || req.url === "/satoru.wasm") {
            const artifactPath = path.resolve(
              __dirname,
              "../satoru/dist",
              req.url.substring(1),
            );
            if (fs.existsSync(artifactPath)) {
              res.setHeader(
                "Content-Type",
                req.url.endsWith(".js")
                  ? "application/javascript"
                  : "application/wasm",
              );
              res.end(fs.readFileSync(artifactPath));
              return;
            }
          }

          if (req.url?.startsWith("/assets/")) {
            const urlPath = req.url.split("?")[0];
            const assetPath = path.resolve(
              __dirname,
              "../../",
              urlPath.substring(1),
            );
            if (fs.existsSync(assetPath) && fs.lstatSync(assetPath).isFile()) {
              res.setHeader("Content-Type", getMimeType(assetPath));
              res.end(fs.readFileSync(assetPath));
              return;
            }
          }
          next();
        });
      },
      // ビルド時にファイルをコピー
      closeBundle() {
        const distDir = path.resolve(__dirname, "dist");
        if (!fs.existsSync(distDir)) return;

        // satoru artifacts
        ["satoru.js", "satoru.wasm"].forEach((file) => {
          const src = path.resolve(__dirname, "../satoru/dist", file);
          const dest = path.resolve(distDir, file);
          if (fs.existsSync(src)) {
            fs.copyFileSync(src, dest);
            console.log(`Copied ${file} to dist/`);
          }
        });

        // assets directory
        const srcAssets = path.resolve(__dirname, "../../assets");
        const destAssets = path.resolve(distDir, "assets");
        if (fs.existsSync(srcAssets)) {
          copyRecursiveSync(srcAssets, destAssets);
          console.log(`Copied assets/ to dist/assets/`);
        }
      },
    },
    {
      name: "watch-external-assets",
      configureServer(server) {
        const assetsPath = path.resolve(__dirname, "../../assets");
        server.watcher.add(assetsPath);
        server.watcher.on("change", (file) => {
          if (file.startsWith(assetsPath)) {
            server.ws.send({ type: "full-reload" });
          }
        });
      },
    },
  ],
  build: {
    outDir: "dist",
    assetsDir: "assets",
  },
});

function copyRecursiveSync(src: string, dest: string) {
  const exists = fs.existsSync(src);
  const stats = exists && fs.statSync(src);
  const isDirectory = exists && stats && stats.isDirectory();
  if (isDirectory) {
    if (!fs.existsSync(dest)) fs.mkdirSync(dest, { recursive: true });
    fs.readdirSync(src).forEach((childItemName) => {
      copyRecursiveSync(
        path.join(src, childItemName),
        path.join(dest, childItemName),
      );
    });
  } else {
    fs.copyFileSync(src, dest);
  }
}

function getMimeType(filePath: string): string {
  const ext = path.extname(filePath).toLowerCase();
  const mimes: Record<string, string> = {
    ".html": "text/html",
    ".css": "text/css",
    ".js": "application/javascript",
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
    ".gif": "image/gif",
    ".svg": "image/svg+xml",
    ".wasm": "application/wasm",
  };
  return mimes[ext] || "application/octet-stream";
}
