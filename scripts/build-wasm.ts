import { execSync } from "child_process";
import * as fs from "fs";
import * as path from "path";

// Load .env file
if (fs.existsSync(".env")) {
  const envContent = fs.readFileSync(".env", "utf-8");
  envContent.split(/\r?\n/).forEach((line) => {
    const trimmedLine = line.trim();
    if (trimmedLine && !trimmedLine.startsWith("#")) {
      const [key, ...values] = trimmedLine.split("=");
      if (key && values.length > 0) {
        const value = values.join("=").trim().replace(/^["']|["']$/g, "");
        process.env[key.trim()] = value;
      }
    }
  });
}

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
let ninjaPath = "";
try {
  const result = execSync(isWin ? "where ninja" : "which ninja").toString().trim().split(/\r?\n/)[0];
  if (result) {
    ninjaPath = result;
    execSync(`"${ninjaPath}" --version`, { stdio: "ignore" });
    useNinja = true;
  }
} catch (e) {
  // Ninja not found
}

// Fix for broken patch.exe (e.g. BusyBox shim in Scoop)
if (isWin) {
  try {
    execSync("patch --version", { stdio: "ignore" });
  } catch (e) {
    const commonPaths = [
      "C:\\Program Files\\Git\\usr\\bin\\patch.exe",
      path.join(process.env.USERPROFILE || "", "AppData\\Local\\Programs\\Git\\usr\\bin\\patch.exe"),
    ];
    for (const p of commonPaths) {
      if (fs.existsSync(p)) {
        console.log(`Using Git patch: ${p}`);
        process.env.PATH = `${path.dirname(p)}${path.delimiter}${process.env.PATH}`;
        break;
      }
    }
  }
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

const buildType = "Release";
const buildDir = "build";

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
    `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` +
    (useNinja && ninjaPath ? ` -DCMAKE_MAKE_PROGRAM="${ninjaPath.replace(/\\/g, "/")}"` : "");

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
  console.error("Usage: tsx scripts/build-wasm.ts [configure|build] [--force]");
  process.exit(1);
}
