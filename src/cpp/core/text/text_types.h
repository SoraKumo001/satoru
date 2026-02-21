#ifndef SATORU_TEXT_TYPES_H
#define SATORU_TEXT_TYPES_H

#include <vector>
#include <algorithm>
#include <string>
#include "include/core/SkFont.h"
#include "include/core/SkTextBlob.h"
#include "modules/skshaper/include/SkShaper.h"

namespace satoru {

struct CharFont {
    size_t len;
    SkFont font;
};

class SatoruFontRunIterator : public SkShaper::FontRunIterator {
public:
    SatoruFontRunIterator(const std::vector<CharFont>& charFonts)
        : m_charFonts(charFonts), m_currentPos(0), m_currentIndex(0) {}

    void consume() override {
        if (m_currentIndex < m_charFonts.size()) {
            m_currentPos += m_charFonts[m_currentIndex].len;
            m_currentIndex++;
        }
    }

    size_t endOfCurrentRun() const override {
        return m_currentPos +
               (m_currentIndex < m_charFonts.size() ? m_charFonts[m_currentIndex].len : 0);
    }

    bool atEnd() const override { return m_currentIndex >= m_charFonts.size(); }

    const SkFont& currentFont() const override {
        return m_charFonts[std::min(m_currentIndex, m_charFonts.size() - 1)].font;
    }

private:
    const std::vector<CharFont>& m_charFonts;
    size_t m_currentPos;
    size_t m_currentIndex;
};

struct ShapedResult {
    double width;
    sk_sp<SkTextBlob> blob;
};

struct ShapingKey {
    std::string text;
    std::string font_family;
    float font_size;
    int font_weight;
    bool italic;
    bool is_rtl;

    bool operator==(const ShapingKey& other) const {
        return font_size == other.font_size &&
               font_weight == other.font_weight &&
               italic == other.italic &&
               is_rtl == other.is_rtl &&
               font_family == other.font_family &&
               text == other.text;
    }
};

struct ShapingKeyHash {
    std::size_t operator()(const ShapingKey& k) const {
        std::size_t h = std::hash<std::string>{}(k.text);
        h ^= std::hash<std::string>{}(k.font_family) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<float>{}(k.font_size) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.font_weight) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<bool>{}(k.italic) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<bool>{}(k.is_rtl) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

} // namespace satoru

#endif // SATORU_TEXT_TYPES_H
