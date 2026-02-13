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
  const content = stripBom(xmlContent);

  // Find all xml-stylesheet processing instructions
  const pis = content.match(/<\?xml-stylesheet\s+([^?>]+)\?>/g);
  if (!pis) return null;

  for (const pi of pis) {
    const hrefMatch = pi.match(/\bhref\s*=\s*["']([^"']+)["']/);
    const typeMatch = pi.match(/\btype\s*=\s*["']([^"']+)["']/);

    if (
      hrefMatch &&
      (!typeMatch ||
        ["text/xsl", "text/xml", "application/xml"].includes(typeMatch[1]))
    ) {
      return hrefMatch[1];
    }
  }
  return null;
}

function stripBom(content: string): string {
  return content.charCodeAt(0) === 0xfeff ? content.slice(1) : content;
}

/**
 * Resolve XSL path relative to XML.
 */
function resolveXslPath(xmlName: string, xslHref: string): string {
  const xmlDir = xmlName.includes("/")
    ? xmlName.substring(0, xmlName.lastIndexOf("/") + 1)
    : "";

  let fullPath = xmlDir + xslHref;
  if (fullPath.startsWith("./")) fullPath = fullPath.slice(2);

  // Basic normalization for "../"
  while (fullPath.includes("/../")) {
    fullPath = fullPath.replace(/[^\/]+\/\.\.\//, "");
  }
  return fullPath;
}

/**
 * Load input files into a map and identify XML files to process.
 */
async function loadInputFiles(inputPath: string): Promise<{
  fileMap: Map<string, string>;
  xmlFiles: string[];
}> {
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

    // Load nearby XSL if referenced
    const xslHref = getStylesheetPath(content);
    if (xslHref) {
      const xslPath = path.resolve(path.dirname(inputPath), xslHref);
      if (fs.existsSync(xslPath)) {
        fileMap.set(xslHref, fs.readFileSync(xslPath, "utf-8"));
      }
    }
  } else {
    throw new Error("Unsupported file type. Use .xml or .zip.");
  }

  return { fileMap, xmlFiles };
}

/**
 * Apply default styles for better Japanese text rendering.
 */
function applyDefaultStyles(html: string): string {
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
    return html.replace("</head>", `${extraStyle}</head>`);
  }
  if (html.includes("<body>")) {
    return html.replace("<body>", `<body>${extraStyle}`);
  }
  return extraStyle + html;
}

/**
 * Transform XML to HTML using XSLT.
 */
async function transformXml(
  xmlName: string,
  xmlContent: string,
  fileMap: Map<string, string>,
  logLevel: LogLevel,
): Promise<string | null> {
  const xmlParser = new XmlParser();
  const xslt = new Xslt({ escape: false });

  const xslHref = getStylesheetPath(xmlContent);
  if (!xslHref) {
    if (logLevel >= LogLevel.Warning)
      console.warn(`Skipping ${xmlName}: No stylesheet.`);
    return null;
  }

  const xslResolvedPath = resolveXslPath(xmlName, xslHref);
  let xslContent =
    fileMap.get(xslResolvedPath) ||
    fileMap.get(xslHref) ||
    fileMap.get(xslHref.split("/").pop()!);

  if (!xslContent) {
    if (logLevel >= LogLevel.Warning)
      console.warn(`Skipping ${xmlName}: Stylesheet ${xslHref} not found.`);
    return null;
  }

  const xmlDoc = xmlParser.xmlParse(stripBom(xmlContent));
  const xslDoc = xmlParser.xmlParse(stripBom(xslContent));

  let html = await xslt.xsltProcess(xmlDoc, xslDoc);

  // Fallback for empty results (common in xslt-processor with certain templates)
  if (html.trim().length === 0) {
    const docElement =
      (xmlDoc as any).documentElement ||
      xmlDoc.childNodes.find((c: any) => c.nodeType === 1);
    if (docElement) {
      html = await xslt.xsltProcess(docElement, xslDoc);
    }
  }

  if (html.trim().length === 0) {
    throw new Error(`Transformation failed for ${xmlName}`);
  }

  return applyDefaultStyles(html);
}

/**
 * Convert XSL/XML files to PDF.
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

  const { fileMap, xmlFiles } = await loadInputFiles(inputPath);
  if (xmlFiles.length === 0) throw new Error("No XML files found.");

  const htmlPages: string[] = [];
  for (const xmlName of xmlFiles) {
    try {
      const html = await transformXml(
        xmlName,
        fileMap.get(xmlName)!,
        fileMap,
        logLevel,
      );
      if (html) htmlPages.push(html);
    } catch (err: any) {
      if (logLevel >= LogLevel.Error)
        console.error(`Error processing ${xmlName}: ${err.message}`);
    }
  }

  if (htmlPages.length === 0)
    throw new Error("No documents successfully transformed.");

  const satoru = await Satoru.init({
    onLog: (level, message) => {
      if (level <= logLevel) console.log(`[Satoru] ${message}`);
    },
    logLevel,
  });

  try {
    const pdf = await satoru.render({
      html: htmlPages,
      width,
      format: "pdf",
      baseUrl: "about:blank",
    });
    fs.writeFileSync(outPdfPath, pdf);
  } finally {
    satoru.destroy();
  }
}

/**
 * Main CLI execution
 */
async function main() {
  const inputFile = process.argv[2];
  const outFile =
    process.argv[3] ||
    (inputFile ? inputFile.replace(/\.(xml|zip)$/i, ".pdf") : "output.pdf");

  if (!inputFile) {
    console.log("Usage: node dist/index.js <input.xml|input.zip> [output.pdf]");
    process.exit(1);
  }

  try {
    await convertToPdf(inputFile, outFile);
    console.log(`Successfully converted ${inputFile} to ${outFile}`);
  } catch (err: any) {
    console.error("Conversion failed:", err.message);
    process.exit(1);
  }
}

main();
