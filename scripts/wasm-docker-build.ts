import { execSync } from "child_process";
import * as path from "path";
import * as fs from "fs";

const projectRoot = process.cwd();
const distPath = path.join(projectRoot, "packages/satoru/dist");

if (!fs.existsSync(distPath)) {
  fs.mkdirSync(distPath, { recursive: true });
}

console.log("Building Docker image...");
try {
  // Dockerfile.wasm is now located in the docker directory
  execSync("docker build -f docker/Dockerfile.wasm -t satoru-wasm-build .", {
    stdio: "inherit",
  });

  console.log("Extracting build artifacts...");
  const absoluteDistPath = path.resolve(distPath);

  // For Windows, ensure path is compatible with Docker volume mount
  const dockerPath = absoluteDistPath.replace(/\\/g, "/");

  execSync(`docker run --rm -v "${dockerPath}:/output" satoru-wasm-build`, {
    stdio: "inherit",
  });

  console.log("Wasm build via Docker completed successfully!");
} catch (error) {
  console.error("Error during Docker Wasm build:", error);
  process.exit(1);
}