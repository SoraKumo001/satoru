#ifndef SATORU_CONTEXT_H
#define SATORU_CONTEXT_H

#include <map>
#include <string>
#include <vector>

#include "../utils/image_types.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"

class SatoruContext {
    std::vector<uint8_t> m_lastPng;

   public:
    sk_sp<SkFontMgr> fontMgr;
    std::map<std::string, std::vector<sk_sp<SkTypeface>>> typefaceCache;
    sk_sp<SkTypeface> defaultTypeface;
    std::vector<sk_sp<SkTypeface>> fallbackTypefaces;
    std::map<std::string, image_info> imageCache;

    void init();

    void load_font(const char *name, const uint8_t *data, int size) { loadFont(name, data, size); }
    void loadFont(const char *name, const uint8_t *data, int size);

    void load_image(const char *name, const char *data_url, int width, int height) {
        loadImage(name, data_url, width, height);
    }
    void loadImage(const char *name, const char *data_url, int width, int height);
    void loadImageFromData(const char *name, const uint8_t *data, size_t size);

    void clear_images() { clearImages(); }
    void clearImages();

    void clear_fonts() { clearFonts(); }
    void clearFonts();

    void set_last_png(std::vector<uint8_t> &&data) { m_lastPng = std::move(data); }
    const std::vector<uint8_t> &get_last_png() const { return m_lastPng; }

    sk_sp<SkTypeface> get_typeface(const std::string &family, int weight, SkFontStyle::Slant slant, bool &out_fake_bold);
    std::vector<sk_sp<SkTypeface>> get_typefaces(const std::string &family, int weight, SkFontStyle::Slant slant, bool &out_fake_bold);
    bool get_image_size(const std::string &url, int &w, int &h);
};

#endif  // SATORU_CONTEXT_H