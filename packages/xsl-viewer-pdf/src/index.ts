import { Satoru, LogLevel } from "satoru";
import { Xslt, xmlParse } from "xslt-processor";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import JSZip from "jszip";

/**
 * Extract the stylesheet path from an XML string.
 */
function getStylesheetPath(xmlContent: string): string | null {
  const match = xmlContent.match(
    /<\?xml-stylesheet[^>]+href=["']([^"']+)["'][^>]*\?>/,
  );
  return match ? match[1] : null;
}

/**
 * Interface for a file entry (either from disk or ZIP)
 */
interface FileEntry {
  name: string;
  content: string;
}

/**
 * Convert XSL/XML files to PDF.
 * Supports single XML file or a ZIP file containing XML/XSL files.
 */
export async function convertToPdf(
  inputPath: string,
  outPdfPath: string,
  options: { width?: number; logLevel?: LogLevel } = {},
) {
  const { width = 1122, logLevel = LogLevel.None } = options;

  if (!fs.existsSync(inputPath)) {
    throw new Error(`Input file not found: ${inputPath}`);
  }

  const fileMap = new Map<string, string>();
  const xmlFiles: string[] = [];

  if (inputPath.toLowerCase().endsWith(".zip")) {
    const zipData = fs.readFileSync(inputPath);
    const zip = await JSZip.loadAsync(zipData);
    for (const [name, file] of Object.entries(zip.files)) {
      if (!file.dir) {
        const content = await file.async("string");
        fileMap.set(name, content);
        if (name.toLowerCase().endsWith(".xml")) {
          xmlFiles.push(name);
        }
      }
    }
  } else if (inputPath.toLowerCase().endsWith(".xml")) {
    const content = fs.readFileSync(inputPath, "utf-8");
    const name = path.basename(inputPath);
    fileMap.set(name, content);
    xmlFiles.push(name);

    // Also try to load nearby XSL files if any
    const xmlDir = path.dirname(inputPath);
    const xslHref = getStylesheetPath(content);
    if (xslHref) {
      const xslPath = path.resolve(xmlDir, xslHref);
      if (fs.existsSync(xslPath)) {
        fileMap.set(xslHref, fs.readFileSync(xslPath, "utf-8"));
      }
    }
  } else {
    throw new Error(
      "Unsupported file type. Please provide a .xml or .zip file.",
    );
  }

  if (xmlFiles.length === 0) {
    throw new Error("No XML files found in the input.");
  }

  const xslt = new Xslt();
  const htmlPages: string[] = [];

  for (const xmlName of xmlFiles) {
    const xmlContent = fileMap.get(xmlName)!;
    const xslHref = getStylesheetPath(xmlContent);

    if (!xslHref) {
      if (logLevel >= LogLevel.Warning) {
        console.warn(`Skipping ${xmlName}: No xml-stylesheet found.`);
      }
      continue;
    }

    // Resolve XSL path relative to XML if it's in a ZIP (simple filename match)
    const xslName = xslHref.split("/").pop()!;
    let xslContent = fileMap.get(xslName);

    // Try exact match if not found by filename
    if (!xslContent) {
      xslContent = fileMap.get(xslHref);
    }

    if (!xslContent) {
      if (logLevel >= LogLevel.Warning) {
        console.warn(`Skipping ${xmlName}: Stylesheet ${xslHref} not found.`);
      }
      continue;
    }

    try {
      const xmlDoc = xmlParse(xmlContent);
      const xslDoc = xmlParse(xslContent);
      let html = xslt.xsltProcess(xmlDoc, xslDoc);

      // Add default styles from xsl-viewer-html
      const extraStyle = `
        <style>
          @import url('https://fonts.googleapis.com/css2?family=Noto+Sans+JP&display=swap');
          body { font-family: 'Noto Sans JP', sans-serif; }
          body > div { white-space: normal; }
          pre.oshirase { text-wrap-mode: wrap; }
          pre.normal { text-wrap: auto; }
          @media print {
            body::-webkit-scrollbar { display: none; }
          }
        </style>
      `;
      if (html.includes("</head>")) {
        html = html.replace("</head>", `${extraStyle}</head>`);
      } else if (html.includes("<body>")) {
        html = html.replace("<body>", `<body>${extraStyle}`);
      } else {
        html = extraStyle + html;
      }
      console.log(html);

      htmlPages.push(html);
    } catch (err: any) {
      if (logLevel >= LogLevel.Error) {
        console.error(`Error processing ${xmlName}: ${err.message}`);
      }
    }
  }

  if (htmlPages.length === 0) {
    throw new Error("No documents were successfully transformed.");
  }

  const satoru = await Satoru.init({
    onLog: (level, message) => {
      if (level <= logLevel) {
        console.log(`[Satoru] ${message}`);
      }
    },
  });

  try {
    const pdf = await satoru.render({
      html: htmlPages,
      width,
      format: "pdf",
      removeDefaultMargin: true,
      baseUrl: "about:blank",
    });

    fs.writeFileSync(outPdfPath, pdf);
  } finally {
    satoru.destroy();
  }
}

// CLI implementation
const isMain =
  process.argv[1] &&
  path.resolve(process.argv[1]) ===
    path.resolve(fileURLToPath(import.meta.url));
if (isMain) {
  const inputFile = process.argv[2];
  const outFile =
    process.argv[3] ||
    (inputFile ? inputFile.replace(/\.(xml|zip)$/i, ".pdf") : "output.pdf");

  if (!inputFile) {
    console.log("Usage: node dist/index.js <input.xml|input.zip> [output.pdf]");
    process.exit(1);
  }

  convertToPdf(inputFile, outFile, { logLevel: LogLevel.Debug })
    .then(() =>
      console.log(`Successfully converted ${inputFile} to ${outFile}`),
    )
    .catch((err) => {
      console.error("Conversion failed:", err.message);
      process.exit(1);
    });
}
