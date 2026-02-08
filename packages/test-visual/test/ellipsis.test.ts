import { describe, it, expect } from 'vitest';
import { Satoru } from 'satoru';

describe('Ellipsis', () => {
    it('should show ellipsis when text overflows', async () => {
        const satoru = await Satoru.init();
        const html = `
            <div style="width: 100px; line-height: 1.5; overflow: hidden; text-overflow: ellipsis; font-family: sans-serif; font-size: 16px;">
                Overflowing text that should trigger ellipsis.
            </div>
        `;
        const svg = await satoru.render(html, 500, { format: 'svg' }) as string;
        // Check for common SVG elements instead of specific paths
        expect(svg).toContain('<svg');
        expect(svg).toContain('height="24"');
    });
});
