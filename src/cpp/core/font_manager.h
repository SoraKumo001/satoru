#ifndef SATORU_FONT_MANAGER_H
#define SATORU_FONT_MANAGER_H

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "bridge/bridge_types.h"
#include "core/ifont_manager.h"
#include "core/text/unicode_service.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkTypeface.h"
#include "libs/litehtml/include/litehtml.h"

class SatoruFontManager : public satoru::IFontManager {
   public:
    SatoruFontManager();
    ~SatoruFontManager() override = default;

    // ── IFontManager implementation ────────────────────────────────────
    bool loadFont(const char* name, const uint8_t* data, int size,
                  const char* url = nullptr) override;
    void clear() override;

    void scanFontFaces(const std::string& css) override;
    std::vector<std::string> getFontUrls(
        const std::string& family, int weight, SkFontStyle::Slant slant,
        const std::set<char32_t>* usedCodepoints = nullptr) const override;
    std::string getFontUrl(const std::string& family, int weight,
                           SkFontStyle::Slant slant) const override;
    bool hasFontFaceSource(const std::string& family, const std::string& url) const override;

    std::vector<sk_sp<SkTypeface>> matchFonts(const std::string& family, int weight,
                                              SkFontStyle::Slant slant) override;
    int getMatchedWeight(sk_sp<SkTypeface> typeface, const std::string& family) override;
    int getMatchedSlant(sk_sp<SkTypeface> typeface, const std::string& family) override;

    SkFont* createSkFont(sk_sp<SkTypeface> typeface, float size, int weight) override;

    void addFallbackTypeface(sk_sp<SkTypeface> tf) override { m_fallbackTypefaces.push_back(tf); }
    const std::vector<sk_sp<SkTypeface>>& getFallbackTypefaces() const override {
        return m_fallbackTypefaces;
    }

    sk_sp<SkTypeface> getDefaultTypeface() const override { return m_defaultTypeface; }

    SkFont selectFont(char32_t u, font_info* fi, SkFont* lastSelectedFont,
                      const satoru::UnicodeService& unicode) override;
    SkFont selectFont(char32_t u, font_info* fi, SkFont* lastSelectedFont,
                      const satoru::UnicodeService& unicode, bool isEmoji, bool isMark) override;

    std::string generateFontFaceCSS() const override;

    // ── Non-virtual extensions (SatoruFontManager-specific) ───────────
    sk_sp<SkFontMgr> getFontMgr() const { return m_fontMgr; }

   private:
    sk_sp<SkFontMgr> m_fontMgr;

    struct cached_typeface {
        sk_sp<SkTypeface> typeface;
        SkFontStyle intended_style;
    };
    std::map<std::string, std::vector<cached_typeface>> m_typefaceCache;

    struct font_face_source {
        std::string url;
        std::string unicode_range;
        std::vector<std::pair<uint32_t, uint32_t>> ranges;
    };
    std::map<font_request, std::vector<font_face_source>> m_fontFaces;

    sk_sp<SkTypeface> m_defaultTypeface;
    std::vector<sk_sp<SkTypeface>> m_fallbackTypefaces;

    std::string cleanName(std::string_view name) const;
    void parseUnicodeRange(const std::string& rangeStr,
                           std::vector<std::pair<uint32_t, uint32_t>>& outRanges) const;
    bool checkUnicodeRange(char32_t codepoint,
                           const std::vector<std::pair<uint32_t, uint32_t>>& ranges) const;
};

#endif  // SATORU_FONT_MANAGER_H
