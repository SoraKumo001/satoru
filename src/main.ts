import createModule from '../dist/wasm/main.js';

async function init() {
    console.log('Initializing Wasm module with ThorVG Backend...');
    try {
        const Module = await createModule();
        (Module as any)._init_engine();
        console.log('ThorVG Engine initialized.');
        
        const app = document.getElementById('app');
        if (app) {
            app.innerHTML = `
                <div style="padding: 20px; font-family: 'Segoe UI', sans-serif; max-width: 1000px; margin: 0 auto; background: #fafafa; min-height: 100vh;">
                    <h2 style="color: #E91E63; border-bottom: 2px solid #E91E63; padding-bottom: 10px;">HTML to SVG (ThorVG Wasm)</h2>
                    
                    <div style="display: flex; gap: 20px; margin-bottom: 20px;">
                        <fieldset style="flex: 1; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background: white;">
                            <legend style="font-weight: bold; color: #666;">Output Settings</legend>
                            <label>Width: <input type="number" id="canvasWidth" value="800" style="width: 80px; padding: 4px;"></label>
                            <label style="margin-left: 10px;">Height: <input type="number" id="canvasHeight" value="500" style="width: 80px; padding: 4px;"></label>
                        </fieldset>
                    </div>

                    <h3 style="color: #444;">HTML Input:</h3>
                    <textarea id="htmlInput" style="width: 100%; height: 120px; font-family: 'Consolas', monospace; padding: 12px; border-radius: 4px; border: 1px solid #ccc; box-sizing: border-box; margin-bottom: 15px; box-shadow: inset 0 1px 3px rgba(0,0,0,0.1);">
<div style="padding: 40px; background-color: #fce4ec; border: 4px solid #ad1457; border-radius: 20px;">
  <h1 style="color: #880e4f; text-align: center;">ThorVG + Wasm</h1>
  <p style="font-size: 18px; color: #4a148c; line-height: 1.6;">
    This rendering is performed inside <b>WebAssembly</b> using the ThorVG vector engine.
    It does not rely on the browser's Canvas API for the final output generation.
  </p>
</div>
                    </textarea>
                    
                    <button id="convertBtn" style="padding: 15px 30px; font-size: 18px; cursor: pointer; background: #E91E63; color: white; border: none; border-radius: 4px; width: 100%; font-weight: bold; margin-bottom: 25px; box-shadow: 0 4px 6px rgba(233, 30, 99, 0.3); transition: transform 0.1s;">
                        Generate SVG via ThorVG
                    </button>
                    
                    <div style="display: grid; grid-template-columns: 1fr; gap: 25px;">
                        <div>
                            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px;">
                                <h3 style="margin: 0; color: #444;">Live Preview:</h3>
                                <button id="downloadBtn" style="display: none; background: #4CAF50; color: white; border: none; padding: 6px 15px; cursor: pointer; border-radius: 4px; font-weight: bold;">Download .svg</button>
                            </div>
                            <div id="svgContainer" style="border: 1px solid #ddd; background: #eee; padding: 20px; border-radius: 8px; min-height: 300px; display: flex; justify-content: center; overflow: auto; box-shadow: 0 2px 8px rgba(0,0,0,0.05);">
                                <div style="color: #999; margin-top: 120px;">Your SVG will appear here</div>
                            </div>
                        </div>
                        <div>
                            <h3 style="color: #444;">SVG Source Code:</h3>
                            <textarea id="svgSource" style="width: 100%; height: 250px; font-family: 'Consolas', monospace; padding: 12px; border-radius: 4px; border: 1px solid #ccc; box-sizing: border-box; background: #fdfdfd;" readonly></textarea>
                        </div>
                    </div>
                </div>
            `;

            const convertBtn = document.getElementById('convertBtn');
            const downloadBtn = document.getElementById('downloadBtn');
            const htmlInput = document.getElementById('htmlInput') as HTMLTextAreaElement;
            const svgContainer = document.getElementById('svgContainer');
            const svgSource = document.getElementById('svgSource') as HTMLTextAreaElement;
            const canvasWidthInput = document.getElementById('canvasWidth') as HTMLInputElement;
            const canvasHeightInput = document.getElementById('canvasHeight') as HTMLInputElement;

            convertBtn?.addEventListener('click', () => {
                const htmlStr = htmlInput.value;
                const width = parseInt(canvasWidthInput.value) || 800;
                const height = parseInt(canvasHeightInput.value) || 500;

                console.log('Generating SVG with ThorVG Wasm...');
                try {
                    // Call the Wasm function
                    const svgResult = (Module as any).ccall(
                        'html_to_svg', 
                        'string', 
                        ['string', 'number', 'number'], 
                        [htmlStr, width, height]
                    );
                    
                    if (svgResult) {
                        if (svgContainer) {
                            svgContainer.innerHTML = svgResult;
                            svgContainer.style.background = "#fff";
                        }
                        if (svgSource) svgSource.value = svgResult;
                        if (downloadBtn) downloadBtn.style.display = 'block';
                        console.log('SVG Generation Successful.');
                    } else {
                        console.error('SVG Generation returned empty string.');
                    }
                } catch (err) {
                    console.error('Error during Wasm call:', err);
                }
            });

            downloadBtn?.addEventListener('click', () => {
                const blob = new Blob([svgSource.value], { type: 'image/svg+xml' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = 'thorvg_render.svg';
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