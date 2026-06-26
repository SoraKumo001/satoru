#ifndef SATORU_IFONT_MANAGER_H
#define SATORU_IFONT_MANAGER_H

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "bridge/bridge_types.h"
#include "core/text/unicode_service.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkTypeface.h"

namespace satoru {

/// Abstract interface for font management operations.
/// Enables test doubles for font loading, matching, and @font-face parsing
/// without requiring a full SatoruContext or Skia font infrastructure.
class IFontManager {
   public:
    virtual ~IFontManager() = default;

    // ── Font data loading ──────────────────────────────────────────────
    virtual bool loadFont(const char* name, const uint8_t* data, int size,
                          const char* url = nullptr) = 0;
    virtual void clear() = 0;

    // ── @font-face parsing and URL resolution ──────────────────────────
    virtual void scanFontFaces(const std::string& css) = 0;
    virtual std::vector<std::string> getFontUrls(
        const std::string& family, int weight, SkFontStyle::Slant slant,
        const std::set<char32_t>* usedCodepoints = nullptr) const = 0;
    virtual std::string getFontUrl(const std::string& family, int weight,
                                   SkFontStyle::Slant slant) const = 0;
    virtual bool hasFontFaceSource(const std::string& family, const std::string& url) const = 0;

    // ── Font matching ──────────────────────────────────────────────────
    virtual std::vector<sk_sp<SkTypeface>> matchFonts(const std::string& family, int weight,
                                                      SkFontStyle::Slant slant) = 0;
    virtual int getMatchedWeight(sk_sp<SkTypeface> typeface, const std::string& family) = 0;
    virtual int getMatchedSlant(sk_sp<SkTypeface> typeface, const std::string& family) = 0;

    // ── SkFont instance creation (with Variable Font axis support) ────
    virtual SkFont* createSkFont(sk_sp<SkTypeface> typeface, float size, int weight) = 0;

    // ── Fallback typefaces ─────────────────────────────────────────────
    virtual void addFallbackTypeface(sk_sp<SkTypeface> tf) = 0;
    virtual const std::vector<sk_sp<SkTypeface>>& getFallbackTypefaces() const = 0;
    virtual sk_sp<SkTypeface> getDefaultTypeface() const = 0;

    // ── Per-character font selection ───────────────────────────────────
    virtual SkFont selectFont(char32_t u, font_info* fi, SkFont* lastSelectedFont,
                              const satoru::UnicodeService& unicode) = 0;
    virtual SkFont selectFont(char32_t u, font_info* fi, SkFont* lastSelectedFont,
                              const satoru::UnicodeService& unicode, bool isEmoji, bool isMark) = 0;

    // ── CSS generation ─────────────────────────────────────────────────
    virtual std::string generateFontFaceCSS() const = 0;
};

}  // namespace satoru

#endif  // SATORU_IFONT_MANAGER_H
