// Declare the global factory function provided by satoru.js
declare function createSatoruModule(options?: any): Promise<any>;

async function init() {
    console.log('Initializing Satoru Engine (Skia + Wasm)...');
    try {
        const Module = await createSatoruModule({
            locateFile: (path: string) => {
                if (path.endsWith('.wasm')) {
                    // Use relative path for better portability in dist
                    return './satoru.wasm';
                }
                return path;
            }
        });
        Module._init_engine();
        
        const loadFont = async (name: string, url: string) => {
            console.log(`Loading font: ${name} from ${url}`);
            const resp = await fetch(url);
            const buffer = await resp.arrayBuffer();
            const data = new Uint8Array(buffer);
            
            // Modern Emscripten string passing
            const nameLen = Module.lengthBytesUTF8(name) + 1;
            const namePtr = Module._malloc(nameLen);
            Module.stringToUTF8(name, namePtr, nameLen);
            
            const dataPtr = Module._malloc(data.length);
            Module.HEAPU8.set(data, dataPtr);
            
            Module._load_font(namePtr, dataPtr, data.length);
            
            Module._free(namePtr);
            Module._free(dataPtr);
            console.log(`Font ${name} load call finished.`);
        };

        const app = document.getElementById('app');
        if (app) {
            app.innerHTML = `
                <style>
                    .loading-overlay {
                        position: absolute;
                        top: 0; left: 0; right: 0; bottom: 0;
                        background: rgba(255, 255, 255, 0.7);
                        display: flex;
                        justify-content: center;
                        align-items: center;
                        z-index: 10;
                        border-radius: 8px;
                        font-weight: bold;
                        color: #2196F3;
                        backdrop-filter: blur(2px);
                    }
                    .spinner {
                        width: 40px;
                        height: 40px;
                        border: 4px solid #f3f3f3;
                        border-top: 4px solid #2196F3;
                        border-radius: 50%;
                        animation: spin 1s linear infinite;
                        margin-right: 15px;
                    }
                    @keyframes spin {
                        0% { transform: rotate(0deg); }
                        100% { transform: rotate(360deg); }
                    }
                    #svgContainer { position: relative; }
                </style>
                <div style="padding: 20px; font-family: sans-serif; max-width: 1000px; margin: 0 auto; background: #fafafa; min-height: 100vh;">
                    <h2 style="color: #2196F3; border-bottom: 2px solid #2196F3; padding-bottom: 10px;">Satoru Engine: HTML to Pure SVG</h2>
                    
                    <div style="display: flex; gap: 20px; margin-bottom: 20px;">
                        <fieldset style="flex: 1; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background: white;">
                            <legend style="font-weight: bold;">Canvas & Fonts</legend>
                            <label>Width: <input type="number" id="canvasWidth" value="800" style="width: 80px;"></label>
                            <button id="loadFontBtn" style="margin-left: 20px; padding: 5px 10px; cursor: pointer; background: #4CAF50; color: white; border: none; border-radius: 4px;">Loading Fonts...</button>
                        </fieldset>
                        <fieldset style="flex: 1; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background: white;">
                            <legend style="font-weight: bold;">Load Sample Assets</legend>
                            <select id="assetSelect" style="padding: 5px; border-radius: 4px; border: 1px solid #ccc; width: 100%;">
                                <option value="">-- Select Asset --</option>
                                <option value="01-basic-styling.html">01-basic-styling.html</option>
                                <option value="02-typography.html">02-typography.html</option>
                                <option value="03-flexbox-layout.html">03-flexbox-layout.html</option>
                                <option value="04-japanese-rendering.html">04-japanese-rendering.html</option>
                                <option value="05-ui-components.html">05-ui-components.html</option>
                                <option value="06-complex-layout.html">06-complex-layout.html</option>
                            </select>
                        </fieldset>
                    </div>

                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-bottom: 15px;">
                        <div>
                            <h3>HTML Input:</h3>
                            <textarea id="htmlInput" style="width: 100%; height: 300px; font-family: monospace; padding: 10px; border-radius: 4px; border: 1px solid #ccc; box-sizing: border-box;"></textarea>
                        </div>
                        <div>
                            <h3>HTML Live Preview (iframe):</h3>
                            <div style="border: 1px solid #ccc; border-radius: 4px; height: 300px; background: white; overflow: hidden;">
                                <iframe id="htmlPreview" style="width: 100%; height: 100%; border: none;"></iframe>
                            </div>
                        </div>
                    </div>

                    <button id="convertBtn" style="padding: 15px 30px; font-size: 20px; cursor: pointer; background: #2196F3; color: white; border: none; border-radius: 4px; width: 100%; font-weight: bold; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(33, 150, 243, 0.3);">Generate Vector SVG</button>
                    
                    <div style="display: grid; grid-template-columns: 1fr; gap: 20px;">
                        <div>
                            <div style="display: flex; justify-content: space-between; align-items: center;">
                                <h3>SVG Render Preview:</h3>
                                <button id="downloadBtn" style="display: none; background: #FF9800; color: white; border: none; padding: 6px 15px; cursor: pointer; border-radius: 4px;">Download .svg</button>
                            </div>
                            <div id="svgContainer" style="border: 1px solid #ddd; background: #eee; padding: 20px; border-radius: 8px; min-height: 300px; display: flex; justify-content: center; overflow: auto;">
                                <div style="color: #999; margin-top: 120px;">Result will appear here</div>
                            </div>
                        </div>
                        <div>
                            <h3>SVG Output Source:</h3>
                            <textarea id="svgSource" style="width: 100%; height: 250px; font-family: monospace; padding: 10px; border-radius: 4px; border: 1px solid #ccc; box-sizing: border-box; background: #fdfdfd;" readonly></textarea>
                        </div>
                    </div>
                </div>
            `;

            const convertBtn = document.getElementById('convertBtn') as HTMLButtonElement;
            const loadFontBtn = document.getElementById('loadFontBtn');
            const downloadBtn = document.getElementById('downloadBtn');
            const htmlInput = document.getElementById('htmlInput') as HTMLTextAreaElement;
            const htmlPreview = document.getElementById('htmlPreview') as HTMLIFrameElement;
            const svgContainer = document.getElementById('svgContainer') as HTMLDivElement;
            const svgSource = document.getElementById('svgSource') as HTMLTextAreaElement;
            const canvasWidthInput = document.getElementById('canvasWidth') as HTMLInputElement;
            const assetSelect = document.getElementById('assetSelect') as HTMLSelectElement;

            const updatePreview = () => {
                const doc = htmlPreview.contentDocument || htmlPreview.contentWindow?.document;
                if (doc) {
                    doc.open();
                    doc.write(htmlInput.value);
                    doc.close();
                }
            };

            const performConversion = async () => {
                const htmlStr = htmlInput.value;
                const width = parseInt(canvasWidthInput.value) || 800;

                // Show loading effect
                const loadingOverlay = document.createElement('div');
                loadingOverlay.className = 'loading-overlay';
                loadingOverlay.innerHTML = '<div class="spinner"></div> Converting...';
                svgContainer.appendChild(loadingOverlay);
                convertBtn.disabled = true;
                convertBtn.style.opacity = '0.7';

                // Small delay to ensure UI updates
                await new Promise(resolve => setTimeout(resolve, 50));

                try {
                    const svgResult = Module.ccall(
                        'html_to_svg', 
                        'string', 
                        ['string', 'number', 'number'], 
                        [htmlStr, width, 0]
                    );
                    
                    svgContainer.innerHTML = svgResult;
                    svgContainer.style.background = "#fff";
                    if (svgSource) svgSource.value = svgResult;
                    if (downloadBtn) downloadBtn.style.display = 'block';
                } catch (err) {
                    console.error('Error during Wasm call:', err);
                    svgContainer.innerHTML = '<div style="color: red; padding: 20px;">Conversion Failed</div>';
                } finally {
                    convertBtn.disabled = false;
                    convertBtn.style.opacity = '1.0';
                }
            };

            const NOTO_URL = 'https://cdn.jsdelivr.net/npm/@fontsource/noto-sans-jp/files/noto-sans-jp-japanese-400-normal.woff2';
            const ROBOTO_URL = 'https://fonts.gstatic.com/s/roboto/v30/KFOmCnqEu92Fr1Mu4mxK.woff2';

            // Auto-load default fonts (Roboto & Noto Sans JP)
            (async () => {
                try {
                    if (loadFontBtn) {
                        loadFontBtn.setAttribute('disabled', 'true');
                        loadFontBtn.innerText = 'Loading Fonts...';
                    }
                    await Promise.all([
                        loadFont('Roboto', ROBOTO_URL),
                        loadFont('Noto Sans JP', NOTO_URL)
                    ]);
                    if (loadFontBtn) loadFontBtn.innerText = 'Fonts Loaded \u2713';
                    performConversion();
                } catch (e) {
                    console.error('Failed to load default fonts:', e);
                    if (loadFontBtn) {
                        loadFontBtn.innerText = 'Font Load Failed';
                        loadFontBtn.removeAttribute('disabled');
                    }
                }
            })();

            // Initial content
            htmlInput.value = `
<div style="padding: 40px; background-color: #e3f2fd; border: 5px solid #2196f3; border-radius: 24px;">
  <h1 style="color: #0d47a1; font-family: 'Roboto'; font-size: 48px; text-align: center; margin-bottom: 10px;">Outline Fonts in Wasm</h1>
  <p style="font-size: 22px; color: #1565c0; line-height: 1.5; font-family: 'Roboto';">
    This SVG is rendered using <b>Skia's SVG Canvas</b>. 
    The text is precisely measured and positioned using <b>FreeType</b> metrics.
  </p>
  <p style="font-size: 20px; color: #0d47a1; font-family: 'Noto Sans JP'; margin-top: 20px;">
    日本語の表示も可能です（Noto Sans JPをロード済み）。
  </p>
</div>`;
            updatePreview();

            htmlInput.addEventListener('input', updatePreview);

            assetSelect?.addEventListener('change', async () => {
                const asset = assetSelect.value;
                if (!asset) return;
                try {
                    const resp = await fetch(`./assets/${asset}`);
                    if (!resp.ok) throw new Error('Failed to fetch asset');
                    const text = await resp.text();
                    htmlInput.value = text;
                    updatePreview();
                    performConversion();
                } catch (e) {
                    console.error('Error loading asset:', e);
                    alert('Failed to load asset');
                }
            });

            loadFontBtn?.addEventListener('click', async () => {
                loadFontBtn.innerText = 'Loading...';
                loadFontBtn.setAttribute('disabled', 'true');
                try {
                    await Promise.all([
                        loadFont('Roboto', ROBOTO_URL),
                        loadFont('Noto Sans JP', NOTO_URL)
                    ]);
                    loadFontBtn.innerText = 'Fonts Loaded \u2713';
                    performConversion();
                } catch (e) {
                    console.error('Font load failed:', e);
                    loadFontBtn.innerText = 'Load Failed';
                    loadFontBtn.removeAttribute('disabled');
                }
            });

            convertBtn?.addEventListener('click', performConversion);

            downloadBtn?.addEventListener('click', () => {
                const blob = new Blob([svgSource.value], { type: 'image/svg+xml' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = 'satoru_render.svg';
                document.body.appendChild(a);
                a.click();
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
            });
        }
    } catch (e) {
        console.error('Failed to load Wasm module:', e);
    }
}

init();