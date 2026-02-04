import createModule from '../dist/wasm/main.js';

async function downloadFont(url: string): Promise<Uint8Array> {
    const response = await fetch(url);
    if (!response.ok) throw new Error(`Failed to fetch font from ${url}`);
    const buffer = await response.arrayBuffer();
    return new Uint8Array(buffer);
}

const DEFAULT_FONTS = [
    { name: 'Roboto', url: 'https://fonts.gstatic.com/s/roboto/v30/KFOmCnqEu92Fr1Mu4mxK.woff2' },
    { name: 'NotoSansJP', url: 'https://fonts.gstatic.com/s/notosansjp/v52/-Ky47oWBlWRGaRnwSalyx60S2y6GBtM.woff2' },
    { name: 'PlayfairDisplay', url: 'https://fonts.gstatic.com/s/playfairdisplay/v37/6nuTXNuLToSl4wbtwb-Km13JO5dU1ve7Qa6xcxarOcA.woff2' }
];

async function init() {
    console.log('Initializing Wasm module...');
    try {
        const Module = await createModule();
        
        const app = document.getElementById('app');
        if (app) {
            app.innerHTML = `
                <div style="padding: 20px; font-family: sans-serif; max-width: 900px; margin: 0 auto;">
                    <h2>HTML to SVG Converter</h2>
                    <div id="status" style="margin-bottom: 10px; color: blue; font-weight: bold; padding: 10px; background: #eef;">Wasm ready.</div>
                    
                    <fieldset style="margin-bottom: 20px; padding: 15px; border: 1px solid #ccc; border-radius: 8px;">
                        <legend>Font Management</legend>
                        <div id="fontList"></div>
                        <div style="margin-top: 10px;">
                            <button id="addFontFieldBtn">+ Add Font Field</button>
                            <button id="loadAllFontsBtn" style="background-color: #4CAF50; color: white; border: none; padding: 8px 15px; cursor: pointer; border-radius: 4px; font-weight: bold;">Load All Fonts to Wasm</button>
                            <button id="clearFontsBtn" style="background-color: #f44336; color: white; border: none; padding: 8px 15px; cursor: pointer; border-radius: 4px;">Clear Wasm Cache</button>
                        </div>
                    </fieldset>

                    <textarea id="htmlInput" style="width: 100%; height: 150px; font-family: monospace; padding: 10px; border-radius: 4px; border: 1px solid #aaa;">
<div style="font-family: 'PlayfairDisplay'; font-size: 40px; color: darkblue;">
  Classic Serif
</div>
<div style="font-family: 'NotoSansJP'; font-size: 24px; color: green;">
  こんにちは、世界 (Japanese Test)
</div>
<div style="font-family: 'Roboto'; font-size: 18px; color: #333;">
  Modern Sans-Serif rendering via Wasm.
</div>
                    </textarea>
                    <br><br>
                    <button id="convertBtn" style="padding: 12px 30px; font-size: 18px; cursor: pointer; background: #2196F3; color: white; border: none; border-radius: 4px; width: 100%;">Convert to SVG</button>
                    <hr>
                    <h3>Result (SVG Output):</h3>
                    <div id="svgContainer" style="border: 1px solid #ccc; padding: 10px; min-height: 200px; background: white; border-radius: 4px; box-shadow: inset 0 2px 4px rgba(0,0,0,0.1);"></div>
                </div>
            `;

            const status = document.getElementById('status');
            const fontList = document.getElementById('fontList');
            const addFontFieldBtn = document.getElementById('addFontFieldBtn');
            const loadAllFontsBtn = document.getElementById('loadAllFontsBtn');
            const clearFontsBtn = document.getElementById('clearFontsBtn');
            const convertBtn = document.getElementById('convertBtn');
            const htmlInput = document.getElementById('htmlInput') as HTMLTextAreaElement;
            const svgContainer = document.getElementById('svgContainer');

            const createFontEntry = (name = '', url = '') => {
                const div = document.createElement('div');
                div.className = 'font-entry';
                div.style.marginBottom = '8px';
                div.style.display = 'flex';
                div.style.gap = '10px';
                div.innerHTML = `
                    <input type="text" placeholder="Name" class="font-name" style="width: 150px; padding: 5px;" value="${name}">
                    <input type="text" placeholder="URL" class="font-url" style="flex-grow: 1; padding: 5px;" value="${url}">
                    <button class="remove-font-btn" style="background: none; border: 1px solid #ccc; cursor: pointer; border-radius: 4px;">&times;</button>
                `;
                div.querySelector('.remove-font-btn')?.addEventListener('click', () => div.remove());
                fontList?.appendChild(div);
            };

            // Initialize with default fonts
            DEFAULT_FONTS.forEach(f => createFontEntry(f.name, f.url));

            // Add dynamic font field
            addFontFieldBtn?.addEventListener('click', () => createFontEntry());

            // Load All Fonts
            loadAllFontsBtn?.addEventListener('click', async () => {
                const entries = document.querySelectorAll('.font-entry');
                if (status) {
                    status.innerText = 'Downloading and loading fonts...';
                    status.style.color = 'orange';
                }
                
                let successCount = 0;
                for (const entry of Array.from(entries)) {
                    const name = (entry.querySelector('.font-name') as HTMLInputElement).value;
                    const url = (entry.querySelector('.font-url') as HTMLInputElement).value;
                    
                    if (!name || !url) continue;

                    try {
                        const fontData = await downloadFont(url);
                        const dataPtr = (Module as any)._malloc(fontData.length);
                        (Module as any).HEAPU8.set(fontData, dataPtr);
                        
                        (Module as any).ccall(
                            'load_font',
                            null,
                            ['string', 'number', 'number'],
                            [name, dataPtr, fontData.length]
                        );
                        
                        (Module as any)._free(dataPtr);
                        successCount++;
                    } catch (err) {
                        console.error(`Failed to load font ${name}:`, err);
                    }
                }
                if (status) {
                    status.innerText = `Successfully loaded ${successCount} fonts to Wasm memory.`;
                    status.style.color = 'green';
                }
            });

            // Clear Fonts
            clearFontsBtn?.addEventListener('click', () => {
                (Module as any)._clear_fonts();
                if (status) {
                    status.innerText = 'Wasm font cache cleared.';
                    status.style.color = 'blue';
                }
            });

            // Convert Logic
            convertBtn?.addEventListener('click', () => {
                const htmlStr = htmlInput.value;
                try {
                    const svgResult = (Module as any).ccall('html_to_svg', 'string', ['string'], [htmlStr]);
                    if (svgContainer) svgContainer.innerHTML = svgResult;
                } catch (err) {
                    console.error('Error during Wasm call:', err);
                }
            });
        }
    } catch (e) {
        console.error('Failed to load Wasm module:', e);
    }
}

init();
