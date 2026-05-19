import { execSync } from "child_process";
import * as fs from "fs";
import * as path from "path";

const pkgPath = "packages/satoru/package.json";
const changelogPath = "packages/satoru/CHANGELOG.md";

console.log("🚀 Starting release check...");

// 1. Version Check
console.log("🔍 Checking version consistency...");
const pkg = JSON.parse(fs.readFileSync(pkgPath, "utf-8"));
const changelog = fs.readFileSync(changelogPath, "utf-8");
const latestVersionMatch = changelog.match(/## v(\d+\.\d+\.\d+)/);

if (!latestVersionMatch) {
  console.error("❌ Could not find latest version in CHANGELOG.md");
  process.exit(1);
}

if (latestVersionMatch[1] !== pkg.version) {
  console.error(`❌ Version mismatch! package.json: ${pkg.version}, CHANGELOG.md: ${latestVersionMatch[1]}`);
  process.exit(1);
}
console.log(`✅ Versions match: v${pkg.version}`);

// 2. Build Verification
console.log("🏗️ Running build...");
try {
  execSync("pnpm build", { stdio: "inherit" });
  console.log("✅ Build succeeded");
} catch (e) {
  console.error("❌ Build failed");
  process.exit(1);
}

// 3. Export Verification
console.log("📦 Verifying exported files...");
const exports = pkg.exports;
const missingFiles: string[] = [];

function checkFile(filePath: string) {
  // Handle objects with types/import/node keys
  if (typeof filePath !== "string") return;
  
  const absolutePath = path.join("packages/satoru", filePath);
  if (!fs.existsSync(absolutePath)) {
    missingFiles.push(filePath);
  }
}

function traverseExports(obj: any) {
  if (typeof obj === "string") {
    checkFile(obj);
  } else if (obj && typeof obj === "object") {
    for (const key in obj) {
      traverseExports(obj[key]);
    }
  }
}

traverseExports(exports);

// Also check bin
if (pkg.bin) {
  for (const key in pkg.bin) {
    checkFile(pkg.bin[key]);
  }
}

if (missingFiles.length > 0) {
  console.error("❌ Missing exported files:");
  missingFiles.forEach(f => console.error(`  - ${f}`));
  process.exit(1);
}
console.log("✅ All exported files exist");

// 4. Package Contents
console.log("📄 Checking required package files...");
const requiredFiles = ["README.md", "LICENSE", "CHANGELOG.md"];
for (const file of requiredFiles) {
  if (!fs.existsSync(path.join("packages/satoru", file))) {
    console.error(`❌ Missing required file: ${file}`);
    process.exit(1);
  }
}
console.log("✅ All required files present");

// 5. Test Verification
console.log("🧪 Running tests...");
try {
  execSync("pnpm -F visual-test test", { stdio: "inherit" });
  console.log("✅ Tests passed");
} catch (e) {
  console.error("❌ Tests failed");
  process.exit(1);
}

// 6. pnpm pack --dry-run
console.log("📦 Running pnpm pack --dry-run...");
try {
  execSync("pnpm pack --dry-run", { cwd: "packages/satoru", stdio: "inherit" });
  console.log("✅ pnpm pack --dry-run succeeded");
} catch (e) {
  console.error("❌ pnpm pack --dry-run failed");
  process.exit(1);
}

console.log("\n✨ Release check passed! The package is ready for publication.");
