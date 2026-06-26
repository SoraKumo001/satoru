#ifndef SATORU_CONTEXT_H
#define SATORU_CONTEXT_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/ilogger.h"
#include "core/satoru_cache_manager.h"
#include "core/text/text_types.h"
#include "core/text/unicode_service.h"
#include "font_manager.h"
#include "include/core/SkData.h"
#include "include/core/SkFontStyle.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "utils/lru_cache.h"
#include "utils/skia_utils.h"

enum class CssChangeKind {
    Generic,
    UserScan,
    ExternalResource,
    FontResourceCss,
    FontAliasCss,
    GeneratedFontFace,
};

class SatoruContext {
    satoru::ILogger *m_logger = nullptr;
    sk_sp<SkData> m_lastPng;
    sk_sp<SkData> m_lastWebp;
    sk_sp<SkData> m_lastPdf;
    sk_sp<SkData> m_lastSvg;
    std::string m_extraCss;
    std::unordered_set<std::string> m_extraCssBlocks;
    std::map<std::string, std::string> m_fontMap;
    uint64_t m_cssVersion = 0;
    uint64_t m_userCssVersion = 0;
    uint64_t m_externalCssVersion = 0;
    uint64_t m_fontResourceCssVersion = 0;
    uint64_t m_fontAliasCssVersion = 0;
    uint64_t m_generatedFontFaceCssVersion = 0;
    uint64_t m_fontVersion = 0;
    uint64_t m_imageVersion = 0;
    uint64_t m_fontResourceNamedLoadCount = 0;
    uint64_t m_fontResourceFallbackLoadCount = 0;
    uint64_t m_fontResourceRegisteredCount = 0;
    uint64_t m_fontResourceDuplicateLoadCount = 0;
    uint64_t m_generatedFontFaceAttemptCount = 0;
    uint64_t m_generatedFontFaceAddedCount = 0;
    uint64_t m_generatedFontFaceDuplicateCount = 0;

    std::unique_ptr<satoru::UnicodeService> m_unicodeService;
    std::unique_ptr<SkShaper> m_shaper;

   public:
    struct LayoutProfile {
        bool enabled = false;
        double container_create_font_ms = 0.0;
        double container_text_width_ms = 0.0;
        double container_split_text_ms = 0.0;
        double container_bidi_ms = 0.0;
        double text_measure_ms = 0.0;
        double text_analyze_ms = 0.0;
        double text_shape_ms = 0.0;
        double text_shape_prepared_ms = 0.0;
        int container_create_font_count = 0;
        int container_text_width_count = 0;
        int container_text_width_unique_count = 0;
        int container_text_width_duplicate_count = 0;
        int container_split_text_count = 0;
        int container_bidi_count = 0;
        int text_measure_count = 0;
        int text_analyze_count = 0;
        int text_shape_count = 0;
        int text_shape_prepared_count = 0;
        int text_measure_cacheable_count = 0;
        int text_measure_cache_hit_count = 0;
        std::unordered_set<std::string> container_text_width_keys;

        void reset() {
            container_create_font_ms = 0.0;
            container_text_width_ms = 0.0;
            container_split_text_ms = 0.0;
            container_bidi_ms = 0.0;
            text_measure_ms = 0.0;
            text_analyze_ms = 0.0;
            text_shape_ms = 0.0;
            text_shape_prepared_ms = 0.0;
            container_create_font_count = 0;
            container_text_width_count = 0;
            container_text_width_unique_count = 0;
            container_text_width_duplicate_count = 0;
            container_split_text_count = 0;
            container_bidi_count = 0;
            text_measure_count = 0;
            text_analyze_count = 0;
            text_shape_count = 0;
            text_shape_prepared_count = 0;
            text_measure_cacheable_count = 0;
            text_measure_cache_hit_count = 0;
            container_text_width_keys.clear();
        }
    };

    SatoruFontManager fontManager;
    std::map<std::string, image_info> imageCache;
    satoru::SatoruCacheManager cacheManager;
    LayoutProfile layoutProfile;
    bool needsRelayout = false;

    SatoruContext() {}
    SatoruContext(satoru::ILogger *logger) : m_logger(logger) {}

    void init();

    void setLogger(satoru::ILogger *logger) { m_logger = logger; }
    satoru::ILogger *getLogger() const { return m_logger; }

    satoru::UnicodeService &getUnicodeService();
    SkShaper *getShaper();

    bool addCss(const std::string &css, CssChangeKind kind = CssChangeKind::Generic) {
        if (css.empty()) return false;
        if (!m_extraCssBlocks.insert(css).second) return false;
        m_extraCss += css + "\n";
        m_cssVersion++;
        switch (kind) {
            case CssChangeKind::UserScan:
                m_userCssVersion++;
                break;
            case CssChangeKind::ExternalResource:
                m_externalCssVersion++;
                break;
            case CssChangeKind::FontResourceCss:
                m_fontResourceCssVersion++;
                break;
            case CssChangeKind::FontAliasCss:
                m_fontAliasCssVersion++;
                break;
            case CssChangeKind::GeneratedFontFace:
                m_generatedFontFaceCssVersion++;
                break;
            case CssChangeKind::Generic:
                break;
        }
        return true;
    }
    const std::string &getExtraCss() const { return m_extraCss; }
    uint64_t getCssVersion() const { return m_cssVersion; }
    uint64_t getUserCssVersion() const { return m_userCssVersion; }
    uint64_t getExternalCssVersion() const { return m_externalCssVersion; }
    uint64_t getFontResourceCssVersion() const { return m_fontResourceCssVersion; }
    uint64_t getFontAliasCssVersion() const { return m_fontAliasCssVersion; }
    uint64_t getGeneratedFontFaceCssVersion() const { return m_generatedFontFaceCssVersion; }
    uint64_t getFontVersion() const { return m_fontVersion; }
    uint64_t getImageVersion() const { return m_imageVersion; }
    uint64_t getFontResourceNamedLoadCount() const { return m_fontResourceNamedLoadCount; }
    uint64_t getFontResourceFallbackLoadCount() const { return m_fontResourceFallbackLoadCount; }
    uint64_t getFontResourceRegisteredCount() const { return m_fontResourceRegisteredCount; }
    uint64_t getFontResourceDuplicateLoadCount() const { return m_fontResourceDuplicateLoadCount; }
    uint64_t getGeneratedFontFaceAttemptCount() const { return m_generatedFontFaceAttemptCount; }
    uint64_t getGeneratedFontFaceAddedCount() const { return m_generatedFontFaceAddedCount; }
    uint64_t getGeneratedFontFaceDuplicateCount() const {
        return m_generatedFontFaceDuplicateCount;
    }
    void noteFontResourceNamedLoad() { m_fontResourceNamedLoadCount++; }
    void noteFontResourceFallbackLoad() { m_fontResourceFallbackLoadCount++; }
    void noteFontResourceRegistered(bool registered) {
        if (registered)
            m_fontResourceRegisteredCount++;
        else
            m_fontResourceDuplicateLoadCount++;
    }
    void noteGeneratedFontFace(bool added) {
        m_generatedFontFaceAttemptCount++;
        if (added)
            m_generatedFontFaceAddedCount++;
        else
            m_generatedFontFaceDuplicateCount++;
    }
    void markFontChanged() { m_fontVersion++; }
    void clearCss() {
        if (m_extraCss.empty() && m_extraCssBlocks.empty()) return;
        m_extraCss.clear();
        m_extraCssBlocks.clear();
        m_cssVersion++;
    }

    bool load_font(const char *name, const uint8_t *data, int size, const char *url = nullptr) {
        bool changed = fontManager.loadFont(name, data, size, url);
        if (changed) m_fontVersion++;
        return changed;
    }
    bool loadFont(const char *name, const uint8_t *data, int size, const char *url = nullptr) {
        bool changed = fontManager.loadFont(name, data, size, url);
        if (changed) m_fontVersion++;
        return changed;
    }

    void load_image(const char *name, const char *data_url, int width, int height) {
        loadImage(name, data_url, width, height);
    }
    void loadImage(const char *name, const char *data_url, int width, int height);
    void loadImageFromData(const char *name, const uint8_t *data, size_t size,
                           const char *original_url = nullptr);
    void loadImageFromPixels(const char *name, int width, int height, const uint8_t *pixels,
                             const char *original_url = nullptr);

    void clear_images() { clearImages(); }
    void clearImages() {
        if (imageCache.empty()) return;
        imageCache.clear();
        m_imageVersion++;
        needsRelayout = true;
    }

    void clear_fonts() { clearFonts(); }
    void clearFonts() {
        fontManager.clear();
        m_fontVersion++;
    }

    void clearAll() {
        clearFonts();
        clearImages();
        clearCss();
        m_fontMap.clear();
        cacheManager.clearAll();
    }

    void setFontMap(const std::map<std::string, std::string> &fontMap) { m_fontMap = fontMap; }
    const std::map<std::string, std::string> &getFontMap() const { return m_fontMap; }

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
