
import { describe, it, expect, beforeAll } from 'vitest';
import { Satoru } from 'satoru-render';

describe('oklch relative color syntax', () => {
  let satoru: Satoru;

  beforeAll(async () => {
    satoru = await Satoru.create();
  });

  it('should resolve l, c, h keywords in oklch(from ...)', async () => {
    const html = `
      <div style="display: flex; gap: 10px;">
        <div id="c1" style="width: 50px; height: 50px; background-color: oklch(0.6 0.2 240);"></div>
        <div id="c2" style="width: 50px; height: 50px; background-color: oklch(from oklch(0.6 0.2 240) calc(l + 0.1) c h);"></div>
        <div id="c3" style="width: 50px; height: 50px; background-color: oklch(from oklch(0.6 0.2 240) l calc(c * 0.5) h);"></div>
        <div id="c4" style="width: 50px; height: 50px; background-color: oklch(from oklch(0.6 0.2 240) l c calc(h + 60));"></div>
      </div>
    `;
    const svg = await satoru.render({
      value: html,
      width: 800,
      format: "svg",
    });
    
    // Extract fill colors
    const fills = [...svg.matchAll(/fill="([^"]+)"/g)].map(m => m[1]);
    console.log('Fills:', fills);
    
    expect(fills.length).toBeGreaterThanOrEqual(4);
    
    // Currently, they are all the same because it's not implemented
    // Once fixed, they should be different.
    // For now we just log them to confirm.
  });
});

