import { execSync } from "child_process";
import * as fs from "fs";
import * as path from "path";

const isWin = process.platform === "win32";
const action = process.argv[2];

const EMSDK = process.env.EMSDK;
const VCPKG_ROOT = process.env.VCPKG_ROOT;

if (!EMSDK || !VCPKG_ROOT) {
  console.error(
    "Error: EMSDK and VCPKG_ROOT environment variables must be set.",
  );
  process.exit(1);
}

const emscriptenCmake = path
  .join(EMSDK, "upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
  .replace(/\\/g, "/");
const vcpkgCmake = path
  .join(VCPKG_ROOT, "scripts/buildsystems/vcpkg.cmake")
  .replace(/\\/g, "/");
const emsdkEnv = isWin
  ? path.join(EMSDK, "emsdk_env.bat")
  : `. ${path.join(EMSDK, "emsdk_env.sh")}`;
const shell = isWin ? "cmd.exe" : "/bin/sh";

// Check if ninja is available
let useNinja = false;
try {
  execSync("ninja --version", { stdio: "ignore" });
  useNinja = true;
} catch (e) {
  // Ninja not found
}

function run(cmd: string, cwd?: string) {
  const fullCmd = process.env.GITHUB_ACTIONS
    ? cmd
    : isWin
      ? `call "${emsdkEnv}" && emsdk activate latest && ${cmd}`
      : `. ${path.join(EMSDK!, "emsdk_env.sh")} && emsdk activate latest && ${cmd}`;

  console.log(`> ${cmd}`);
  execSync(fullCmd, { stdio: "inherit", shell, cwd });
}

const isDebug = process.argv.includes("--debug");
const buildType = isDebug ? "Debug" : "Release";
const buildDir = `build-wasm-${buildType.toLowerCase()}`;

if (action === "configure") {
  const force = process.argv.includes("--force");
  if (force && fs.existsSync(buildDir)) {
    try {
      fs.rmSync(buildDir, { recursive: true, force: true });
    } catch (e) {
      console.warn(`Warning: Could not remove ${buildDir} directory, attempting to continue.`);
    }
  }
  if (!fs.existsSync(buildDir)) {
    fs.mkdirSync(buildDir);
  }

  const generator = useNinja ? "Ninja" : "Unix Makefiles";
  const projectRoot = process.cwd().replace(/\\/g, "/");

  const cmakeCmd =
    `cmake .. -G "${generator}" ` +
    `-Wno-dev ` +
    `-DCMAKE_BUILD_TYPE=${buildType} ` +
    `-DCMAKE_TOOLCHAIN_FILE="${vcpkgCmake}" ` +
    `-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="${emscriptenCmake}" ` +
    `-DVCPKG_TARGET_TRIPLET=wasm32-emscripten-wasm-eh ` +
    `-DVCPKG_OVERLAY_TRIPLETS="${projectRoot}/triplets" ` +
    `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`;

  run(cmakeCmd, buildDir);
} else if (action === "build") {
  if (!fs.existsSync(buildDir)) {
    console.error(`Error: Build directory ${buildDir} does not exist. Run configure first.`);
    process.exit(1);
  }

  if (useNinja) {
    run("ninja", buildDir);
  } else {
    run("emmake make -j16", buildDir);
  }
} else {
  console.error("Usage: tsx scripts/build-wasm.ts [configure|build] [--debug] [--force]");
  process.exit(1);
}
