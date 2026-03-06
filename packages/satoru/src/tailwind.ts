import { createGenerator } from "@unocss/core";
import presetUno from "@unocss/preset-wind4";

export const createCSS = async (html: string) => {
  const uno = await createGenerator({ presets: [presetUno()] });
  return uno.generate(html).then((v) => v.css);
};
