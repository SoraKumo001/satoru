import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { createRequire } from 'module';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const require = createRequire(import.meta.url);

// Path to a sample font in the repo
const FONT_PATH = path.resolve(__dirname, '../external/skia/modules/canvaskit/fonts/NotoMono-Regular.ttf');
const ASSET_HTML_PATH = path.resolve(__dirname, 'assets/test.html');
const WASM_JS_PATH = path.resolve(__dirname, '../public/satoru.js');
const WASM_BINARY_PATH = path.resolve(__dirname, '../public/satoru.wasm');

async function runTest() {
    console.log('--- Satoru Wasm Layout Stress Test Start ---');

    try {
        const createSatoruModule = require(WASM_JS_PATH);
        const instance = await createSatoruModule({
            locateFile: (url: string) => url.endsWith('.wasm') ? WASM_BINARY_PATH : url
        });
        
        instance._init_engine();

        if (fs.existsSync(FONT_PATH)) {
            const fontData = fs.readFileSync(FONT_PATH);
            const fontPtr = instance._malloc(fontData.length);
            instance.HEAPU8.set(new Uint8Array(fontData), fontPtr);
            const fontName = "NotoMono";
            const fontNamePtr = instance._malloc(fontName.length + 1);
            instance.stringToUTF8(fontName, fontNamePtr, fontName.length + 1);
            instance._load_font(fontNamePtr, fontPtr, fontData.length);
            instance._free(fontPtr);
            instance._free(fontNamePtr);
        }

        // --- Load Test Image ---
        // Create a simple 1x1 Red PNG in base64
        const base64Png = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==";
        const imgBuffer = Buffer.from(base64Png, 'base64');
        const imgPtr = instance._malloc(imgBuffer.length);
        instance.HEAPU8.set(new Uint8Array(imgBuffer), imgPtr);
        
        const imgName = "test_image.png";
        const imgNamePtr = instance._malloc(imgName.length + 1);
        instance.stringToUTF8(imgName, imgNamePtr, imgName.length + 1);
        
        console.log('Loading image: test_image.png');
        instance._load_image(imgNamePtr, imgPtr, imgBuffer.length, 1, 1);
        
        instance._free(imgPtr);
        instance._free(imgNamePtr);
        // -----------------------

        let html = '';
        if (fs.existsSync(ASSET_HTML_PATH)) {
            html = fs.readFileSync(ASSET_HTML_PATH, 'utf8');
        } else {
            console.warn(`HTML asset not found at ${ASSET_HTML_PATH}`);
            return;
        }

        console.log('Rendering...');
        const htmlBuffer = Buffer.from(html + '\0', 'utf8');
        const htmlPtr = instance._malloc(htmlBuffer.length);
        instance.HEAPU8.set(htmlBuffer, htmlPtr);
        const svgPtr = instance._html_to_svg(htmlPtr, 800, 0);
        const svg = instance.UTF8ToString(svgPtr);
        
        const outputPath = path.resolve(__dirname, '../temp/stress_test.svg');
        fs.writeFileSync(outputPath, svg);
        console.log(`Success! Saved to: ${outputPath}`);

        instance._free(htmlPtr);
    } catch (error) {
        console.error('Test failed:', error);
    }
}

runTest();