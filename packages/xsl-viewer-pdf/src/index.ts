import { Satoru, LogLevel } from "satoru";
import { Xslt, XmlParser } from "xslt-processor";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import JSZip from "jszip";

/**
 * Extract the stylesheet path from an XML string.
 */
function getStylesheetPath(xmlContent: string): string | null {
  // Strip BOM if present
  const content =
    xmlContent.charCodeAt(0) === 0xfeff ? xmlContent.slice(1) : xmlContent;

  // Find all xml-stylesheet processing instructions
  const pis = content.match(/<\?xml-stylesheet\s+([^?>]+)\?>/g);
  if (!pis) return null;

  for (const pi of pis) {
    // Extract href and type attributes
    const hrefMatch = pi.match(/\bhref\s*=\s*["']([^"']+)["']/);
    const typeMatch = pi.match(/\btype\s*=\s*["']([^"']+)["']/);

    // If it's an XSL stylesheet (or no type specified, which usually defaults to it)
    if (
      hrefMatch &&
      (!typeMatch ||
        typeMatch[1] === "text/xsl" ||
        typeMatch[1] === "text/xml" ||
        typeMatch[1] === "application/xml")
    ) {
      return hrefMatch[1];
    }
  }
  return null;
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

  const xmlParser = new XmlParser();
  const htmlPages: string[] = [];
  for (const xmlName of xmlFiles) {
    const xslt = new Xslt();
    let xmlContent = fileMap.get(xmlName)!;

    // Remove BOM
    if (xmlContent.charCodeAt(0) === 0xfeff) xmlContent = xmlContent.slice(1);

    const xslHref = getStylesheetPath(xmlContent);

    if (!xslHref) {
      if (logLevel >= LogLevel.Warning) {
        console.warn(`Skipping ${xmlName}: No xml-stylesheet found.`);
      }
      continue;
    }

    // Resolve XSL path relative to XML
    const xmlDir = xmlName.includes("/")
      ? xmlName.substring(0, xmlName.lastIndexOf("/") + 1)
      : "";

    // 1. Try relative to XML
    let xslFullPath = xmlDir + xslHref;
    // Basic normalization for "./"
    if (xslFullPath.startsWith("./")) xslFullPath = xslFullPath.slice(2);
    // Handle "../" (very basic)
    while (xslFullPath.includes("/../")) {
      xslFullPath = xslFullPath.replace(/[^\/]+\/\.\.\//, "");
    }

    if (logLevel >= LogLevel.Debug) {
      console.log(`[XSLT] Processing ${xmlName} with stylesheet ${xslHref}`);
      console.log(`[XSLT] Resolved XSL path: ${xslFullPath}`);
    }

    let xslContent = fileMap.get(xslFullPath);

    // 2. Try exact match (for absolute-looking paths or paths already in fileMap)
    if (!xslContent) {
      xslContent = fileMap.get(xslHref);
    }

    // 3. Try filename match as fallback
    if (!xslContent) {
      const xslName = xslHref.split("/").pop()!;
      xslContent = fileMap.get(xslName);
    }

    if (!xslContent) {
      if (logLevel >= LogLevel.Warning) {
        console.warn(`Skipping ${xmlName}: Stylesheet ${xslHref} not found.`);
      }
      continue;
    }

    // Remove BOM from XSL
    if (xslContent.charCodeAt(0) === 0xfeff) xslContent = xslContent.slice(1);

    try {
      if (logLevel >= LogLevel.Debug) {
        console.log(`[XSLT] XML Content length: ${xmlContent.length}`);
        console.log(`[XSLT] XSL Content length: ${xslContent.length}`);
      }

      const xmlDoc = xmlParser.xmlParse(xmlContent);
      const xslDoc = xmlParser.xmlParse(xslContent);

      let html = await xslt.xsltProcess(xmlDoc, xslDoc);

      // Heuristic: If result is empty and there's no match="/", try matching from the root element.
      // xslt-processor seems to lack built-in template rules for the root node.
      if (html.trim().length === 0) {
        const docElement =
          (xmlDoc as any).documentElement ||
          xmlDoc.childNodes.find((c: any) => c.nodeType === 1);
        if (docElement) {
          if (logLevel >= LogLevel.Debug) {
            console.log(
              `[XSLT] First pass empty, retrying with root element: ${docElement.nodeName}`,
            );
          }
          html = await xslt.xsltProcess(docElement, xslDoc);
        }
      }

      if (html.length === 0) {
        throw new Error(
          `XSLT transformation produced empty result for ${xmlName}`,
        );
      }

      if (logLevel >= LogLevel.Debug) {
        console.log(`[XSLT] Transformed HTML length: ${html.length}`);
        // console.log(html); // Optional: log full HTML only in extreme debug
      }

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
      if (logLevel >= LogLevel.Debug) {
        console.log(
          `[XSLT] HTML (first 500 chars): ${html.substring(0, 500)}...`,
        );
      }
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
    logLevel,
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

const inputFile = process.argv[2];
const outFile =
  process.argv[3] ||
  (inputFile ? inputFile.replace(/\.(xml|zip)$/i, ".pdf") : "output.pdf");

if (!inputFile) {
  console.log("Usage: node dist/index.js <input.xml|input.zip> [output.pdf]");
  process.exit(1);
}

convertToPdf(inputFile, outFile)
  .then(() => console.log(`Successfully converted ${inputFile} to ${outFile}`))
  .catch((err) => {
    console.error("Conversion failed:", err.message);
    process.exit(1);
  });
