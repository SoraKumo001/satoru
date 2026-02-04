// Declare the global factory function provided by satoru.js
declare function createSatoruModule(): Promise<any>;

async function init() {
    console.log('Initializing Satoru Engine (Skia + Wasm)...');
    try {
        const Module = await createSatoruModule();
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
                <div style="padding: 20px; font-family: sans-serif; max-width: 1000px; margin: 0 auto; background: #fafafa; min-height: 100vh;">
                    <h2 style="color: #2196F3; border-bottom: 2px solid #2196F3; padding-bottom: 10px;">Satoru Engine: HTML to Pure SVG</h2>
                    
                    <div style="display: flex; gap: 20px; margin-bottom: 20px;">
                        <fieldset style="flex: 1; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background: white;">
                            <legend style="font-weight: bold;">Canvas & Fonts</legend>
                            <label>Width: <input type="number" id="canvasWidth" value="800" style="width: 80px;"></label>
                            <button id="loadFontBtn" style="margin-left: 20px; padding: 5px 10px; cursor: pointer; background: #4CAF50; color: white; border: none; border-radius: 4px;">Load Roboto Font</button>
                        </fieldset>
                    </div>

                    <h3>HTML Input (with CSS Styles):</h3>
                    <textarea id="htmlInput" style="width: 100%; height: 150px; font-family: monospace; padding: 10px; border-radius: 4px; border: 1px solid #ccc; box-sizing: border-box; margin-bottom: 15px;">
<div style="padding: 40px; background-color: #e3f2fd; border: 5px solid #2196f3; border-radius: 24px;">
  <h1 style="color: #0d47a1; font-family: 'Roboto'; font-size: 48px; text-align: center; margin-bottom: 10px;">Outline Fonts in Wasm</h1>
  <p style="font-size: 22px; color: #1565c0; line-height: 1.5; font-family: 'Roboto';">
    This SVG is rendered using <b>Skia's SVG Canvas</b>. 
    The text is precisely measured and positioned using <b>FreeType</b> metrics.
  </p>
  <div style="margin-top: 20px; border-top: 2px dashed #90caf9; padding-top: 10px; font-size: 18px; color: #64b5f6; font-family: 'Roboto';">
    Vector perfect output ready for any backend.
  </div>
</div>
                    </textarea>
                    <button id="convertBtn" style="padding: 15px 30px; font-size: 20px; cursor: pointer; background: #2196F3; color: white; border: none; border-radius: 4px; width: 100%; font-weight: bold; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(33, 150, 243, 0.3);">Generate Vector SVG</button>
                    
                    <div style="display: grid; grid-template-columns: 1fr; gap: 20px;">
                        <div>
                            <div style="display: flex; justify-content: space-between; align-items: center;">
                                <h3>Preview:</h3>
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

                    <div style="margin-top: 40px; padding: 20px; border-top: 3px solid #ccc;">
                        <h3>Stress Test Result (from temp/stress_test.svg)</h3>
                        <div style="margin-bottom: 10px;">
                            <button id="loadStressTestBtn" style="padding: 10px 20px; background: #673ab7; color: white; border: none; border-radius: 4px; cursor: pointer;">
                                Load Generated SVG
                            </button>
                        </div>
                        <div id="stressTestContainer" style="border: 2px dashed #999; padding: 20px; background: #fff; overflow: auto; min-height: 200px;">
                            <p style="color: #666; text-align: center;">Click button to load generated SVG</p>
                        </div>
                    </div>
                </div>
            `;

            const convertBtn = document.getElementById('convertBtn');
            const loadFontBtn = document.getElementById('loadFontBtn');
            const downloadBtn = document.getElementById('downloadBtn');
            const htmlInput = document.getElementById('htmlInput') as HTMLTextAreaElement;
            const svgContainer = document.getElementById('svgContainer');
            const svgSource = document.getElementById('svgSource') as HTMLTextAreaElement;
            const canvasWidthInput = document.getElementById('canvasWidth') as HTMLInputElement;
            const loadStressTestBtn = document.getElementById('loadStressTestBtn');
            const stressTestContainer = document.getElementById('stressTestContainer');

            loadFontBtn?.addEventListener('click', async () => {
                loadFontBtn.innerText = 'Loading...';
                loadFontBtn.setAttribute('disabled', 'true');
                try {
                    await loadFont('Roboto', 'https://fonts.gstatic.com/s/roboto/v30/KFOmCnqEu92Fr1Mu4mxK.woff2');
                    loadFontBtn.innerText = 'Roboto Loaded \u2713';
                } catch (e) {
                    console.error('Font load failed:', e);
                    loadFontBtn.innerText = 'Load Failed';
                    loadFontBtn.removeAttribute('disabled');
                }
            });

            convertBtn?.addEventListener('click', () => {
                const htmlStr = htmlInput.value;
                const width = parseInt(canvasWidthInput.value) || 800;

                try {
                    const svgResult = Module.ccall(
                        'html_to_svg', 
                        'string', 
                        ['string', 'number', 'number'], 
                        [htmlStr, width, 0]
                    );
                    
                    if (svgContainer) {
                        svgContainer.innerHTML = svgResult;
                        svgContainer.style.background = "#fff";
                    }
                    if (svgSource) svgSource.value = svgResult;
                    if (downloadBtn) downloadBtn.style.display = 'block';
                } catch (err) {
                    console.error('Error during Wasm call:', err);
                }
            });

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

            loadStressTestBtn?.addEventListener('click', async () => {
                try {
                    // Fetch the SVG through the symlink created in public/temp
                    const response = await fetch('/temp/stress_test.svg?t=' + Date.now());
                    if (!response.ok) throw new Error('Failed to load SVG');
                    const text = await response.text();
                    if (stressTestContainer) {
                        stressTestContainer.innerHTML = text;
                    }
                } catch (e) {
                    console.error('Error loading stress test SVG:', e);
                    if (stressTestContainer) {
                        stressTestContainer.innerHTML = '<p style="color: red;">Failed to load stress_test.svg. Make sure the symlink exists and the file was generated.</p>';
                    }
                }
            });
        }
    } catch (e) {
        console.error('Failed to load Wasm module:', e);
    }
}

init();
