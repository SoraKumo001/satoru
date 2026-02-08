import { describe, it, expect } from 'vitest';
import { Satoru } from 'satoru';

describe('Clipping', () => {
    it('should render something even with overflow: hidden', async () => {
        const satoru = await Satoru.init();
        const html = `
            <div style="width: 100px; height: 100px; overflow: hidden; background-color: red;">
                Test
            </div>
        `;
        const svg = await satoru.render(html, 500, { format: 'svg' }) as string;
        console.log(svg);
        expect(svg).toContain('Test');
    });
});