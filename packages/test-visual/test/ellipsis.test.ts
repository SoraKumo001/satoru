import { describe, it, expect } from 'vitest';
import { Satoru } from 'satoru';

describe('Ellipsis', () => {
    it('should show ellipsis when text overflows', async () => {
        const satoru = await Satoru.init();
        const html = `
            <div style="width: 100px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; font-family: sans-serif; font-size: 16px;">
                aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
            </div>
        `;
        const svg = await satoru.render(html, 500, { format: 'svg' }) as string;
        expect(svg).toContain('...');
    });
});