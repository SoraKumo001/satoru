#ifndef SATORU_FONT_MANAGER_H
#define SATORU_FONT_MANAGER_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "bridge/bridge_types.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkTypeface.h"
#include "libs/litehtml/include/litehtml.h"

class SatoruFontManager {
   public:
    SatoruFontManager();
    ~SatoruFontManager() = default;

    // フォントデータのロード
    void loadFont(const char* name, const uint8_t* data, int size);
    void clear();

    // @font-face 解析と URL 解決
    void scanFontFaces(const std::string& css);
    std::vector<std::string> getFontUrls(const std::string& family, int weight,
                                         SkFontStyle::Slant slant,
                                         const std::set<char32_t>* usedCodepoints = nullptr) const;
    std::string getFontUrl(const std::string& family, int weight, SkFontStyle::Slant slant) const;

    // フォントマッチング
    std::vector<sk_sp<SkTypeface>> matchFonts(const std::string& family, int weight,
                                              SkFontStyle::Slant slant);

    // SkFont インスタンスの生成 (Variable Font 軸適用含む)
    SkFont* createSkFont(sk_sp<SkTypeface> typeface, float size, int weight);

    // グローバルフォールバックの設定
    void addFallbackTypeface(sk_sp<SkTypeface> tf) { m_fallbackTypefaces.push_back(tf); }
    const std::vector<sk_sp<SkTypeface>>& getFallbackTypefaces() const {
        return m_fallbackTypefaces;
    }

    sk_sp<SkTypeface> getDefaultTypeface() const { return m_defaultTypeface; }

   private:
    sk_sp<SkFontMgr> m_fontMgr;
    std::map<std::string, std::vector<sk_sp<SkTypeface>>> m_typefaceCache;

    struct font_face_source {
        std::string url;
        std::string unicode_range;
    };
    std::map<font_request, std::vector<font_face_source>> m_fontFaces;

    sk_sp<SkTypeface> m_defaultTypeface;
    std::vector<sk_sp<SkTypeface>> m_fallbackTypefaces;

    std::string cleanName(const char* name) const;
    bool checkUnicodeRange(char32_t codepoint, const std::string& range) const;
};

#endif  // SATORU_FONT_MANAGER_H
