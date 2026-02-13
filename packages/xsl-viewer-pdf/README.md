# xsl-viewer-pdf

Convert XML documents with XSL stylesheets to PDF using [Satoru](https://github.com/SoraKumo001/satoru).

This package is inspired by [xsl-viewer-html](https://github.com/SoraKumo001/xsl-viewer-html) and supports both single XML files and ZIP archives containing electronic documents.

## Installation

```bash
pnpm install
```

## Usage

### CLI

```bash
node dist/index.js <input.xml|input.zip> [output.pdf]
```

### Library

```typescript
import { convertToPdf } from "xsl-viewer-pdf";

await convertToPdf("document.zip", "output.pdf", { width: 1122 });
```

## Features

- XSLT 1.0 transformation via `xslt-processor`.
- ZIP file support (extracts all XML files and converts them into a single multi-page PDF).
- High-fidelity PDF rendering via Satoru (Skia + litehtml).
- Automatic detection of `<?xml-stylesheet ?>` processing instructions.
