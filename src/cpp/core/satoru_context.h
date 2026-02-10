#ifndef SATORU_CONTEXT_H
#define SATORU_CONTEXT_H

#include <map>
#include <string>
#include <vector>

#include "include/core/SkFontStyle.h"
#include "include/core/SkTypeface.h"
#include "utils/skia_utils.h"
#include "font_manager.h"

class SatoruContext {
    std::vector<uint8_t> m_lastPng;
    std::vector<uint8_t> m_lastPdf;
    std::string m_extraCss;

   public:
    SatoruFontManager fontManager;
    std::map<std::string, image_info> imageCache;

    void init();

    void addCss(const std::string &css) { m_extraCss += css + "\n"; }
    const std::string &getExtraCss() const { return m_extraCss; }
    void clearCss() { m_extraCss.clear(); }

    void load_font(const char *name, const uint8_t *data, int size) { fontManager.loadFont(name, data, size); }
    void loadFont(const char *name, const uint8_t *data, int size) { fontManager.loadFont(name, data, size); }

    void load_image(const char *name, const char *data_url, int width, int height) {
        loadImage(name, data_url, width, height);
    }
    void loadImage(const char *name, const char *data_url, int width, int height);
    void loadImageFromData(const char *name, const uint8_t *data, size_t size);

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

    void set_last_png(std::vector<uint8_t> &&png) { m_lastPng = std::move(png); }
    const std::vector<uint8_t> &get_last_png() const { return m_lastPng; }

    void set_last_pdf(std::vector<uint8_t> &&pdf) { m_lastPdf = std::move(pdf); }
    const std::vector<uint8_t> &get_last_pdf() const { return m_lastPdf; }
};

#endif
