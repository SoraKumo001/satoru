import createModule from '../dist/wasm/main.js';

async function init() {
    try {
        const Module = await createModule();
        const result = (Module as any)._add(5, 7);
        console.log('Result from Wasm add(5, 7):', result);
        const app = document.getElementById('app');
        if (app) {
            app.innerHTML = `<h1>Wasm Result: ${result}</h1>`;
        }
    } catch (e) {
        console.error('Failed to load Wasm module:', e);
    }
}

init();