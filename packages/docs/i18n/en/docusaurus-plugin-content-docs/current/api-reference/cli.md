---
sidebar_position: 6
title: CLI
---

# CLI

The `satoru-render` package also provides a CLI. It accepts a local HTML file or URL and generates SVG, PNG, WebP, or PDF output.

```bash
# Convert local HTML to PNG
npx satoru-render input.html -o output.png

# Convert a URL to PDF
npx satoru-render https://example.com -o site.pdf -w 1280 -f pdf

# Infer format from the output extension
npx satoru-render input.html -o output.webp

# Disable JSDOM hydration
npx satoru-render https://example.com --no-jsdom -o example.png

# Write diagnostics to JSON
npx satoru-render input.html -o output.png --json-report report.json
```

By default, `width` is `800`, `format` is `png`, and JSDOM hydration is enabled. If `-f` / `--format` is not provided and `--output` ends with `svg`, `png`, `webp`, or `pdf`, the CLI infers the format from the output extension.

## CLI Options

| Option | Description |
| --- | --- |
| `<input-file-or-url>` | Input HTML file or URL. Required. |
| `-o, --output <path>` | Output file path. If omitted, the CLI uses the input name or `output` with the format extension. |
| `-w, --width <number>` | Viewport width. Defaults to `800`. |
| `-h, --height <number>` | Viewport height. If omitted, content height is auto-calculated. |
| `-f, --format <format>` | Output format: `svg`, `png`, `webp`, or `pdf`. |
| `--json-report <path>` | Writes diagnostics to a JSON file. |
| `--no-jsdom` | Disables JSDOM hydration and renders the file content or URL directly. |
| `--media <type>` | CSS media type: `screen` or `print`. |
| `--timeout <ms>` | Render timeout. |
| `--max-resource-bytes <bytes>` | Maximum byte size for a single resource. |
| `--max-total-resource-bytes <bytes>` | Maximum total byte size for all resources. |
| `--max-resource-count <count>` | Maximum number of resources to load. |
| `--allowed-protocols <list>` | Comma-separated list of allowed protocols, such as `https:,data:`. |
| `--allowed-hosts <list>` | Comma-separated list of allowed hostnames. |
| `--blocked-hosts <list>` | Comma-separated list of blocked hostnames. |
| `--verbose` | Writes detailed logs to stderr. |
| `--help` | Shows help. |

## CLI and JSDOM

The CLI attempts JSDOM hydration by default. When the input is a URL or local HTML file with scripts, it passes the hydrated DOM to Satoru. If JSDOM is unavailable or hydration fails, use `--verbose` to inspect the reason. Use `--no-jsdom` when you want to render static HTML as-is.

```bash
npx satoru-render ./page.html --no-jsdom -o page.svg
```

## CLI Safety Limits

When rendering external URLs or user-generated HTML, set resource limits.

```bash
npx satoru-render https://example.com \
  -o output.png \
  --timeout 5000 \
  --allowed-protocols https: \
  --blocked-hosts localhost,127.0.0.1 \
  --max-resource-count 20 \
  --max-total-resource-bytes 20971520
```
