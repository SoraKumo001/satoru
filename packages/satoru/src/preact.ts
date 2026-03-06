import { h, type VNode } from "preact";
import render from "preact-render-to-string";
export { h };

/**
 * Converts a React element to a static HTML string.
 * This is a thin wrapper around react-dom/server's renderToStaticMarkup.
 *
 * @param element - The React element to convert.
 * @returns The resulting HTML string.
 */
export function toHtml(element: VNode) {
  return render(element);
}
