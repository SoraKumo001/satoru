#include "text_utils.h"

#include <linebreak.h>
#include <utf8proc.h>

#include <algorithm>

#include "bridge/bridge_types.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkTypeface.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skshaper/include/SkShaper_harfbuzz.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "utils/skunicode_satoru.h"

namespace satoru {

char32_t decode_utf8_char(const char **ptr) {
    if (!ptr || !*ptr || !**ptr) return 0;

    utf8proc_int32_t cp;
    utf8proc_ssize_t result = utf8proc_iterate((const utf8proc_uint8_t *)*ptr, -1, &cp);

    if (result < 0) {
        (*ptr)++;  // Advance one byte on error
        return 0xFFFD;
    }

    *ptr += result;
    return (char32_t)cp;
}

std::string normalize_utf8(const char *text) {
    if (!text || !*text) return "";
    utf8proc_uint8_t *retval = nullptr;
    utf8proc_ssize_t len = utf8proc_map((const utf8proc_uint8_t *)text, 0, &retval,
                                        (utf8proc_option_t)UTF8PROC_COMPOSE);
    if (len < 0 || !retval) return std::string(text);
    std::string result((const char *)retval);
    free(retval);
    return result;
}

namespace {
class SatoruFontRunIterator : public SkShaper::FontRunIterator {
   public:
    struct CharFont {
        size_t len;
        SkFont font;
    };

    SatoruFontRunIterator(const std::vector<CharFont> &charFonts)
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

    const SkFont &currentFont() const override {
        return m_charFonts[std::min(m_currentIndex, m_charFonts.size() - 1)].font;
    }

   private:
    const std::vector<CharFont> &m_charFonts;
    size_t m_currentPos;
    size_t m_currentIndex;
};

class WidthRunHandler : public SkShaper::RunHandler {
   public:
    WidthRunHandler() : fWidth(0) {}
    void beginLine() override {}
    void runInfo(const RunInfo &info) override { fWidth += info.fAdvance.fX; }
    void commitRunInfo() override {}
    Buffer runBuffer(const RunInfo &info) override {
        return {nullptr, nullptr, nullptr, nullptr, {0, 0}};
    }
    void commitRunBuffer(const RunInfo &) override {}
    void commitLine() override {}
    double width() const { return fWidth; }

   private:
    double fWidth;
};
}  // namespace

MeasureResult measure_text(const char *text, font_info *fi, double max_width,
                           std::set<char32_t> *used_codepoints) {
    MeasureResult result = {0.0, 0, true, text};
    if (!text || !*text || !fi || fi->fonts.empty()) return result;

    auto unicode = satoru::MakeUnicode();
    auto shaper = SkShapers::HB::ShapeDontWrapOrReorder(unicode, nullptr);
    if (!shaper) return result;

    // 1. 各文字の使用フォントを判定
    std::vector<SatoruFontRunIterator::CharFont> charFonts;
    const char *p = text;
    while (*p) {
        const char *prev_p = p;
        char32_t u = decode_utf8_char(&p);
        if (used_codepoints) used_codepoints->insert(u);

        SkFont *selected_font = nullptr;
        for (auto f : fi->fonts) {
            SkGlyphID glyph = 0;
            SkUnichar sc = (SkUnichar)u;
            f->getTypeface()->unicharsToGlyphs(SkSpan<const SkUnichar>(&sc, 1),
                                               SkSpan<SkGlyphID>(&glyph, 1));
            if (glyph != 0) {
                selected_font = f;
                break;
            }
        }
        if (!selected_font) selected_font = fi->fonts[0];

        SkFont font = *selected_font;
        if (fi->fake_bold) font.setEmbolden(true);
        charFonts.push_back({(size_t)(p - prev_p), font});
    }

    size_t total_len = p - text;
    bool limit_width = max_width >= 0;

    if (!limit_width) {
        WidthRunHandler handler;
        SatoruFontRunIterator fontRuns(charFonts);
        SkShaper::TrivialBiDiRunIterator bidi(0, total_len);
        SkShaper::TrivialScriptRunIterator script(SkSetFourByteTag('Z', 'y', 'y', 'y'), total_len);
        SkShaper::TrivialLanguageRunIterator lang("en", total_len);

        shaper->shape(text, total_len, fontRuns, bidi, script, lang, nullptr, 0, 1000000, &handler);
        result.width = handler.width();
        result.length = total_len;
        result.last_safe_pos = text + total_len;
        return result;
    }

    // 幅制限がある場合は、徐々に文字を増やして計測 (最適化の余地ありだがまずは正確性)
    const double epsilon = 0.01;
    double last_w = 0;
    size_t current_char_idx = 0;
    const char *walk_p = text;
    utf8proc_int32_t state = 0;

    while (current_char_idx < charFonts.size()) {
        size_t next_char_idx = current_char_idx + 1;

        // 結合文字などの grapheme boundary を考慮
        const char *next_walk_p = walk_p + charFonts[current_char_idx].len;
        bool is_break = true;
        if (next_char_idx < charFonts.size()) {
            const char *p1 = walk_p;
            char32_t u1 = decode_utf8_char(&p1);
            const char *p2 = next_walk_p;
            char32_t u2 = decode_utf8_char(&p2);
            is_break = utf8proc_grapheme_break_stateful(u1, u2, &state);
        }

        if (is_break) {
            size_t sub_len = next_walk_p - text;
            WidthRunHandler handler;
            std::vector<SatoruFontRunIterator::CharFont> subFonts(
                charFonts.begin(), charFonts.begin() + next_char_idx);
            SatoruFontRunIterator fontRuns(subFonts);
            SkShaper::TrivialBiDiRunIterator bidi(0, sub_len);
            SkShaper::TrivialScriptRunIterator script(SkSetFourByteTag('Z', 'y', 'y', 'y'),
                                                      sub_len);
            SkShaper::TrivialLanguageRunIterator lang("en", sub_len);

            shaper->shape(text, sub_len, fontRuns, bidi, script, lang, nullptr, 0, 1000000,
                          &handler);

            if (handler.width() > max_width + epsilon) {
                result.fits = false;
                result.width = last_w;
                result.length = walk_p - text;
                result.last_safe_pos = walk_p;
                return result;
            }
            last_w = handler.width();
            result.last_safe_pos = next_walk_p;
        }

        walk_p = next_walk_p;
        current_char_idx = next_char_idx;
    }

    result.width = last_w;
    result.length = total_len;
    result.last_safe_pos = text + total_len;
    return result;
}

double text_width(const char *text, font_info *fi, std::set<char32_t> *used_codepoints) {
    return measure_text(text, fi, -1.0, used_codepoints).width;
}

std::string ellipsize_text(const char *text, font_info *fi, double max_width,
                           std::set<char32_t> *used_codepoints) {
    if (!text || !*text) return "";

    // First, check if full text fits
    MeasureResult full_res = measure_text(text, fi, max_width, used_codepoints);
    if (full_res.fits) return std::string(text);

    // Calculate ellipsis width
    const char *ellipsis = "...";
    double ellipsis_width = text_width(ellipsis, fi, used_codepoints);

    // Use a small epsilon to handle float precision issues
    const double epsilon = 0.1;

    if (max_width < ellipsis_width - epsilon) {
        // スペースが足りない場合でも、ellipsis モードなら最低限 ... を出す
        return ellipsis;
    }

    double available_width = std::max(0.0, max_width - ellipsis_width);

    // Re-measure with stricter limit
    MeasureResult part_res = measure_text(text, fi, available_width, nullptr);

    std::string result(text, part_res.length);  // length is bytes processed that fit
    result += ellipsis;

    return result;
}

}  // namespace satoru
