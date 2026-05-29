---
sidebar_position: 4
title: CLI Batch Conversion
---

# Batch Convert HTML and URLs with the CLI

`satoru-render` includes a CLI. It can convert HTML files and URLs to PNG, WebP, PDF, or SVG.

## Single Conversion

```bash
npx satoru-render ./input.html -o output.png -w 1200
npx satoru-render https://example.com -o page.pdf -w 1280 -f pdf
npx satoru-render ./input.html -o output.webp -f webp --verbose
```

URLs and HTML files go through JSDOM hydration by default. Use `--no-jsdom` when you want to render static HTML directly.

```bash
npx satoru-render ./static.html -o static.png --no-jsdom
```

## Convert Multiple Files

PowerShell:

```powershell
Get-ChildItem .\pages -Filter *.html | ForEach-Object {
  npx satoru-render $_.FullName -o ".\out\$($_.BaseName).png" -w 1200 --no-jsdom
}
```

Bash:

```bash
mkdir -p out
for file in pages/*.html; do
  name="$(basename "$file" .html)"
  npx satoru-render "$file" -o "out/$name.png" -w 1200 --no-jsdom
done
```

## Diagnostics and Safety Limits

When converting external URLs, set timeouts and resource limits. `--json-report` writes failed resources and diagnostics to a file.

```bash
npx satoru-render https://example.com \
  -o example.png \
  -w 1200 \
  --timeout 5000 \
  --max-resource-count 50 \
  --max-resource-bytes 5000000 \
  --max-total-resource-bytes 20000000 \
  --allowed-protocols https: \
  --json-report example.report.json
```

## Common Options

| Option | Purpose |
| --- | --- |
| `-o, --output` | Output file path. |
| `-w, --width` / `-h, --height` | Layout viewport width and height. |
| `-f, --format` | Select `svg`, `png`, `webp`, or `pdf`. |
| `--no-jsdom` | Disable JSDOM hydration. |
| `--media print` | Use `@media print`. |
| `--json-report` | Save diagnostics as JSON. |
| `--timeout` | Render timeout. |
| `--allowed-protocols` | Comma-separated list of allowed protocols. |
| `--allowed-hosts` / `--blocked-hosts` | Host allow/block lists. |
