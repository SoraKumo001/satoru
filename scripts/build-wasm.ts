import { execSync } from 'child_process';
import * as fs from 'fs';
import * as path from 'path';

const isWin = process.platform === 'win32';
const action = process.argv[2]; // 'configure' or 'build'

const EMSDK = process.env.EMSDK;
const VCPKG_ROOT = process.env.VCPKG_ROOT;

if (!EMSDK || !VCPKG_ROOT) {
    console.error('Error: EMSDK and VCPKG_ROOT environment variables must be set.');
    process.exit(1);
}

// OSごとの設定
const emscriptenCmake = path.join(EMSDK, 'upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake').replace(/\\/g, '/');
const vcpkgCmake = path.join(VCPKG_ROOT, 'scripts/buildsystems/vcpkg.cmake').replace(/\\/g, '/');
const emsdkEnv = isWin ? path.join(EMSDK, 'emsdk_env.bat') : `. ${path.join(EMSDK, 'emsdk_env.sh')}`;
const shell = isWin ? 'cmd.exe' : '/bin/sh';

function run(cmd: string, cwd?: string) {
    // emsdkを有効化してからコマンドを実行
    const fullCmd = isWin 
        ? `call "${emsdkEnv}" && emsdk activate latest && ${cmd}`
        : `${emsdkEnv} && emsdk activate latest && ${cmd}`;
    
    console.log(`> ${cmd}`);
    execSync(fullCmd, { stdio: 'inherit', shell, cwd });
}

if (action === 'configure') {
    if (fs.existsSync('build-wasm')) {
        fs.rmSync('build-wasm', { recursive: true, force: true });
    }
    fs.mkdirSync('build-wasm');
    
    const cmakeCmd = `cmake .. -G "Unix Makefiles" ` +
        `-DCMAKE_TOOLCHAIN_FILE="${vcpkgCmake}" ` +
        `-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="${emscriptenCmake}" ` +
        `-DVCPKG_TARGET_TRIPLET=wasm32-emscripten`;
    
    run(cmakeCmd, 'build-wasm');
} else if (action === 'build') {
    run('emmake make -j16', 'build-wasm');

    // Copy artifacts to test-web public for development
    const srcDir = path.resolve('packages/satoru/dist');
    const destDir = path.resolve('packages/test-web/public');
    
    if (fs.existsSync(srcDir)) {
        if (!fs.existsSync(destDir)) {
            fs.mkdirSync(destDir, { recursive: true });
        }
        ['satoru.js', 'satoru.wasm'].forEach(file => {
            const src = path.join(srcDir, file);
            const dest = path.join(destDir, file);
            if (fs.existsSync(src)) {
                fs.copyFileSync(src, dest);
                console.log(`Copied ${file} to ${destDir}`);
            }
        });
    }
} else {
    console.error('Usage: tsx scripts/build-wasm.ts [configure|build]');
    process.exit(1);
}