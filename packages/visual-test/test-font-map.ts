import { Satoru, LogLevel } from "../satoru/src/node";

async function test() {
  const satoru = await Satoru.create(
    (await import("../satoru/dist/satoru.js")).default
  );

  const html = `
    <html>
      <body style="font-family: 'MyCustomSans';">
        Hello custom font mapping!
      </body>
    </html>
  `;

  console.log("--- Testing custom fontMap ---");
  const result = await satoru.render({
    value: html,
    width: 800,
    format: "svg",
    fontMap: {
      "MyCustomSans": "Pacifico"
    },
    logLevel: LogLevel.Info,
    onLog: (level, message) => {
      console.log(`[Satoru] ${message}`);
    }
  });

  if (result.includes("Pacifico") || result.includes("pacifico")) {
    console.log("SUCCESS: Pacifico font was requested/loaded for MyCustomSans");
  } else {
    console.log("FAILURE: Pacifico font was not found in result");
  }
}

test().catch(console.error);
