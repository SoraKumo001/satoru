#ifndef SATORU_CONTEXT_H
#define SATORU_CONTEXT_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "font_manager.h"
#include "include/core/SkData.h"
#include "include/core/SkFontStyle.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "utils/skia_utils.h"

class SatoruContext {
    sk_sp<SkData> m_lastPng;
    sk_sp<SkData> m_lastWebp;
    sk_sp<SkData> m_lastPdf;
    sk_sp<SkData> m_lastSvg;
    std::string m_extraCss;

    sk_sp<SkUnicode> m_unicode;
    std::unique_ptr<SkShaper> m_shaper;

   public:
    SatoruFontManager fontManager;
    std::map<std::string, image_info> imageCache;

    void init();

    sk_sp<SkUnicode> getUnicode();
    SkShaper *getShaper();

    void addCss(const std::string &css) { m_extraCss += css + "\n"; }
    const std::string &getExtraCss() const { return m_extraCss; }
    void clearCss() { m_extraCss.clear(); }

    void load_font(const char *name, const uint8_t *data, int size, const char *url = nullptr) {
        fontManager.loadFont(name, data, size, url);
    }
    void loadFont(const char *name, const uint8_t *data, int size, const char *url = nullptr) {
        fontManager.loadFont(name, data, size, url);
    }

    void load_image(const char *name, const char *data_url, int width, int height) {
        loadImage(name, data_url, width, height);
    }
    void loadImage(const char *name, const char *data_url, int width, int height);
    void loadImageFromData(const char *name, const uint8_t *data, size_t size,
                           const char *original_url = nullptr);

    void clear_images() { imageCache.clear(); }
    void clearImages() { imageCache.clear(); }

    void clear_fonts() { fontManager.clear(); }
    void clearFonts() { fontManager.clear(); }

    void clearAll() {
        clearFonts();
        clearImages();
        clearCss();
    }

    sk_sp<SkTypeface> get_typeface(const std::string &family, int weight, SkFontStyle::Slant slant,
                                   bool &out_fake_bold);
    std::vector<sk_sp<SkTypeface>> get_typefaces(const std::string &family, int weight,
                                                 SkFontStyle::Slant slant, bool &out_fake_bold);
    bool get_image_size(const std::string &url, int &w, int &h);

    void set_last_png(sk_sp<SkData> png) { m_lastPng = std::move(png); }
    const sk_sp<SkData> &get_last_png() const { return m_lastPng; }
    size_t get_last_png_size() const { return m_lastPng ? m_lastPng->size() : 0; }

    void set_last_webp(sk_sp<SkData> webp) { m_lastWebp = std::move(webp); }
    const sk_sp<SkData> &get_last_webp() const { return m_lastWebp; }
    size_t get_last_webp_size() const { return m_lastWebp ? m_lastWebp->size() : 0; }

    void set_last_pdf(sk_sp<SkData> pdf) { m_lastPdf = std::move(pdf); }
    const sk_sp<SkData> &get_last_pdf() const { return m_lastPdf; }
    size_t get_last_pdf_size() const { return m_lastPdf ? m_lastPdf->size() : 0; }

    void set_last_svg(sk_sp<SkData> svg) { m_lastSvg = std::move(svg); }
    const sk_sp<SkData> &get_last_svg() const { return m_lastSvg; }
    size_t get_last_svg_size() const { return m_lastSvg ? m_lastSvg->size() : 0; }
};

#endif
