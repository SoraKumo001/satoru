import { ReactNode } from "react";
import { renderToStaticMarkup } from "react-dom/server";

/**
 * Converts a React element to a static HTML string.
 * This is a thin wrapper around react-dom/server's renderToStaticMarkup.
 *
 * @param element - The React element to convert.
 * @returns The resulting HTML string.
 */
export function toHtml(element: ReactNode): string {
  return renderToStaticMarkup(element);
}
