#include "font_manager.h"

#include <algorithm>
#include <ctre.hpp>
#include <memory>
#include <mutex>
#include <sstream>

#include "../api/satoru_api.h"
#include "core/text/unicode_service.h"
#include "include/core/SkData.h"
#include "include/core/SkFontArguments.h"
#include "include/core/SkSpan.h"
#include "utils/logging.h"
#include "utils/skia_utils.h"

// External declaration for the custom empty font manager
extern sk_sp<SkFontMgr> SkFontMgr_New_Custom_Empty();

#ifndef SkSetFourByteTag
#define SkSetFourByteTag(a, b, c, d) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))
#endif

namespace {
std::string trim(std::string_view s) {
    auto start = s.find_first_not_of(" \t\r\n'\"");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n'\"");
    return std::string(s.substr(start, end - start + 1));
}

// Global font registries to minimize instantiation overhead
std::mutex g_font_mutex;
std::map<std::string, sk_sp<SkTypeface>> g_url_to_typeface;
std::map<uint64_t, sk_sp<SkTypeface>> g_hash_to_typeface;

struct global_typeface_clone_key {
    SkTypeface* base;
    int weight;
    bool operator<(const global_typeface_clone_key& other) const {
        if (base != other.base) return base < other.base;
        return weight < other.weight;
    }
};
std::map<global_typeface_clone_key, sk_sp<SkTypeface>> g_variable_clone_cache;

uint64_t compute_data_hash(const uint8_t* data, size_t size) {
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < size; ++i) {
        h ^= static_cast<uint64_t>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

sk_sp<SkFontMgr> get_global_font_mgr() {
    static sk_sp<SkFontMgr> mgr = SkFontMgr_New_Custom_Empty();
    return mgr;
}
}  // namespace

SatoruFontManager::SatoruFontManager() { m_fontMgr = get_global_font_mgr(); }

void SatoruFontManager::loadFont(const char* name, const uint8_t* data, int size, const char* url) {
    if (!name || !*name) return;
    if (!m_fontMgr) m_fontMgr = get_global_font_mgr();

    sk_sp<SkTypeface> typeface;
    uint64_t data_hash = 0;

    // Check global caches first
    {
        std::lock_guard<std::mutex> lock(g_font_mutex);
        if (url && *url) {
            auto it = g_url_to_typeface.find(url);
            if (it != g_url_to_typeface.end()) {
                typeface = it->second;
            }
        }
        if (!typeface && data && size > 0) {
            data_hash = compute_data_hash(data, size);
            auto it = g_hash_to_typeface.find(data_hash);
            if (it != g_hash_to_typeface.end()) {
                typeface = it->second;
            }
        }
    }

    if (!typeface && data && size > 0) {
        auto data_ptr = SkData::MakeWithCopy(data, size);
        typeface = m_fontMgr->makeFromData(std::move(data_ptr));

        if (typeface) {
            std::lock_guard<std::mutex> lock(g_font_mutex);
            if (url && *url) {
                g_url_to_typeface[url] = typeface;
            }
            if (data_hash == 0) data_hash = compute_data_hash(data, size);
            g_hash_to_typeface[data_hash] = typeface;
        }
    }

    if (typeface) {
        std::stringstream ss(name);
        std::string item;
        while (std::getline(ss, item, ',')) {
            std::string cleaned = cleanName(trim(item));
            if (cleaned.empty()) continue;

            // Determine intended style from m_fontFaces if url matches
            SkFontStyle intended_style = typeface->fontStyle();
            if (url && *url) {
                for (const auto& entry : m_fontFaces) {
                    if (entry.first.family == cleaned) {
                        for (const auto& src : entry.second) {
                            if (src.url == url) {
                                intended_style =
                                    SkFontStyle(entry.first.weight, SkFontStyle::kNormal_Width,
                                                entry.first.slant);
                                break;
                            }
                        }
                    }
                }
            }

            bool duplicate = false;
            for (const auto& existing : m_typefaceCache[cleaned]) {
                if (existing.typeface->uniqueID() == typeface->uniqueID() &&
                    existing.intended_style == intended_style) {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate) {
                m_typefaceCache[cleaned].push_back({typeface, intended_style});
                if (!m_defaultTypeface) m_defaultTypeface = typeface;

                // If it's an emoji font, also register it under "notocoloremoji"
                if (cleaned.find("emoji") != std::string::npos) {
                    m_typefaceCache["notocoloremoji"].push_back({typeface, intended_style});
                }
            }
        }
    } else {
        SATORU_LOG_ERROR("[Satoru] FAILED to create typeface for '%s'", name);
    }
}

void SatoruFontManager::clear() {
    m_typefaceCache.clear();
    m_fontFaces.clear();
    m_fallbackTypefaces.clear();
    m_defaultTypeface = nullptr;
}

void SatoruFontManager::scanFontFaces(const std::string& css) {
    if (css.find("@font-face") == std::string::npos && css.find("@FONT-FACE") == std::string::npos)
        return;

    std::string_view css_sv = css;
    size_t pos = 0;

    auto case_insensitive_find = [](std::string_view s, std::string_view target, size_t start) {
        if (target.empty()) return std::string_view::npos;
        if (start + target.size() > s.size()) return std::string_view::npos;
        for (size_t i = start; i <= s.size() - target.size(); ++i) {
            bool match = true;
            for (size_t j = 0; j < target.size(); ++j) {
                if (tolower(s[i + j]) != tolower(target[j])) {
                    match = false;
                    break;
                }
            }
            if (match) return i;
        }
        return std::string_view::npos;
    };

    while (true) {
        size_t start_pos = case_insensitive_find(css_sv, "@font-face", pos);
        if (start_pos == std::string_view::npos) break;

        size_t block_start = css_sv.find('{', start_pos);
        if (block_start == std::string_view::npos) break;

        size_t block_end = block_start;
        int depth = 1;
        for (size_t i = block_start + 1; i < css_sv.size(); ++i) {
            if (css_sv[i] == '{')
                depth++;
            else if (css_sv[i] == '}') {
                depth--;
                if (depth == 0) {
                    block_end = i;
                    break;
                }
            }
        }
        if (depth != 0) break;

        std::string_view body_sv = css_sv.substr(block_start + 1, block_end - block_start - 1);
        pos = block_end + 1;

        std::string family = "";
        std::vector<int> weights;
        SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant;

        if (auto m = ctre::search<R"((?i)font-family:\s*([^;\}]+);?)">(body_sv)) {
            family = cleanName(m.get<1>());
        }

        if (auto m = ctre::search<R"((?i)font-weight:\s*([^;\}]+);?)">(body_sv)) {
            std::string w = trim(m.get<1>());
            if (w == "bold")
                weights.push_back(700);
            else if (w == "normal")
                weights.push_back(400);
            else {
                try {
                    if (auto rm = ctre::search<R"((\d+)\s+(\d+))">(w)) {
                        int start = std::stoi(rm.get<1>().to_string());
                        int end = std::stoi(rm.get<2>().to_string());
                        for (int v = 100; v <= 900; v += 100) {
                            if (v >= start && v <= end) weights.push_back(v);
                        }
                    } else {
                        weights.push_back(std::stoi(w));
                    }
                } catch (...) {
                }
            }
        }

        if (weights.empty()) weights.push_back(400);

        if (auto m = ctre::search<R"((?i)font-style:\s*([^;\}]+);?)">(body_sv)) {
            std::string s = trim(m.get<1>());
            if (s == "italic" || s == "oblique") slant = SkFontStyle::kItalic_Slant;
        }

        if (!family.empty()) {
            std::string best_url;
            int best_score = -1;

            size_t url_search_pos = 0;
            while (true) {
                size_t url_idx = case_insensitive_find(body_sv, "url(", url_search_pos);
                if (url_idx == std::string_view::npos) break;

                size_t url_content_start = url_idx + 4;
                size_t url_content_end = body_sv.find(')', url_content_start);
                if (url_content_end == std::string_view::npos) break;

                std::string url =
                    trim(body_sv.substr(url_content_start, url_content_end - url_content_start));
                url_search_pos = url_content_end + 1;

                auto contains_insensitive = [](std::string_view s, std::string_view target) {
                    auto it = std::search(s.begin(), s.end(), target.begin(), target.end(),
                                          [](char a, char b) { return tolower(a) == tolower(b); });
                    return it != s.end();
                };

                int score = 0;
                if (contains_insensitive(url, "woff2"))
                    score = 5;
                else if (contains_insensitive(url, "woff"))
                    score = 4;
                else if (contains_insensitive(url, "ttf") || contains_insensitive(url, "truetype"))
                    score = 3;
                else if (contains_insensitive(url, "otf") || contains_insensitive(url, "opentype"))
                    score = 2;
                else if (contains_insensitive(url, "ttc"))
                    score = 1;
                else if (contains_insensitive(url, ".eot"))
                    score = -1;

                if (score > best_score) {
                    best_score = score;
                    best_url = std::move(url);
                }
            }

            if (!best_url.empty() && best_score >= 0) {
                font_face_source src;
                src.url = best_url;

                if (auto m2 = ctre::search<R"((?i)unicode-range:\s*([^;\}]+);?)">(body_sv)) {
                    src.unicode_range = trim(m2.get<1>().to_string());
                    parseUnicodeRange(src.unicode_range, src.ranges);
                }

                for (int w : weights) {
                    font_request req;
                    req.family = family;
                    req.weight = w;
                    req.slant = slant;

                    bool duplicate = false;
                    for (const auto& existing_src : m_fontFaces[req]) {
                        if (existing_src.url == src.url) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate) {
                        m_fontFaces[req].push_back(src);
                    }
                }
            }
        }
    }
}

std::vector<std::string> SatoruFontManager::getFontUrls(
    const std::string& family, int weight, SkFontStyle::Slant slant,
    const std::set<char32_t>* usedCodepoints) const {
    std::vector<std::string> urls;
    std::stringstream ss(family);
    std::string item;

    while (std::getline(ss, item, ',')) {
        std::string cleanedFamily = cleanName(trim(item).c_str());
        if (cleanedFamily.empty()) continue;

        font_request req;
        req.family = cleanedFamily;
        req.weight = weight;
        req.slant = slant;

        auto it = m_fontFaces.find(req);
        if (it != m_fontFaces.end()) {
            for (const auto& src : it->second) {
                if (std::find(urls.begin(), urls.end(), src.url) != urls.end()) continue;

                bool needed = true;
                if (usedCodepoints && !src.unicode_range.empty()) {
                    needed = false;
                    for (char32_t cp : *usedCodepoints) {
                        if (checkUnicodeRange(cp, src.ranges)) {
                            needed = true;
                            break;
                        }
                    }
                }
                if (needed) {
                    urls.push_back(src.url);
                }
            }
        }

        // If no exact weight match, look for same family and slant but different weight
        // (This helps finding the best available weight if requested weight is missing)
        for (const auto& entry : m_fontFaces) {
            if (entry.first.family == cleanedFamily && entry.first.slant == slant) {
                if (entry.first.weight == weight) continue;  // Already checked

                for (const auto& src : entry.second) {
                    if (std::find(urls.begin(), urls.end(), src.url) == urls.end()) {
                        bool needed = true;
                        if (usedCodepoints && !src.unicode_range.empty()) {
                            needed = false;
                            for (char32_t cp : *usedCodepoints) {
                                if (checkUnicodeRange(cp, src.ranges)) {
                                    needed = true;
                                    break;
                                }
                            }
                        }
                        if (needed) {
                            urls.push_back(src.url);
                        }
                    }
                }
            }
        }
    }

    return urls;
}

std::string SatoruFontManager::getFontUrl(const std::string& family, int weight,
                                          SkFontStyle::Slant slant) const {
    auto urls = getFontUrls(family, weight, slant);
    return urls.empty() ? "" : urls[0];
}

std::vector<sk_sp<SkTypeface>> SatoruFontManager::matchFonts(const std::string& family, int weight,
                                                             SkFontStyle::Slant slant) {
    std::string cleanFamily = cleanName(family);
    auto it = m_typefaceCache.find(cleanFamily);
    if (it != m_typefaceCache.end()) {
        const auto& cached = it->second;
        if (cached.empty()) return {};

        // Find the best match for weight and slant using intended_style
        cached_typeface bestMatch = {nullptr, SkFontStyle()};
        int minDiff = 10000;

        for (const auto& c : cached) {
            SkFontStyle style = c.intended_style;
            int diff = std::abs(style.weight() - weight);
            if (style.slant() != slant) diff += 1000;

            if (diff < minDiff) {
                minDiff = diff;
                bestMatch = c;
            }
        }

        if (bestMatch.typeface) {
            // Also include other typefaces that might have different glyph sets (subsets)
            // but have the same weight/slant. This is important for Google Fonts subsets.
            std::vector<sk_sp<SkTypeface>> matches;
            SkFontStyle bestStyle = bestMatch.intended_style;

            for (const auto& c : cached) {
                if (c.intended_style == bestStyle) {
                    matches.push_back(c.typeface);
                }
            }
            return matches;
        }
    }

    return {};
}

int SatoruFontManager::getMatchedWeight(sk_sp<SkTypeface> typeface, const std::string& family) {
    std::string cleanFamily = cleanName(family);
    auto it = m_typefaceCache.find(cleanFamily);
    if (it != m_typefaceCache.end()) {
        for (const auto& c : it->second) {
            if (c.typeface->uniqueID() == typeface->uniqueID()) {
                return c.intended_style.weight();
            }
        }
    }
    return typeface->fontStyle().weight();
}

int SatoruFontManager::getMatchedSlant(sk_sp<SkTypeface> typeface, const std::string& family) {
    std::string cleanFamily = cleanName(family);
    auto it = m_typefaceCache.find(cleanFamily);
    if (it != m_typefaceCache.end()) {
        for (const auto& c : it->second) {
            if (c.typeface->uniqueID() == typeface->uniqueID()) {
                return c.intended_style.slant();
            }
        }
    }
    return typeface->fontStyle().slant();
}

SkFont* SatoruFontManager::createSkFont(sk_sp<SkTypeface> typeface, float size, int weight) {
    if (!typeface) return nullptr;

    int count = typeface->getVariationDesignPosition(
        SkSpan<SkFontArguments::VariationPosition::Coordinate>());
    if (count > 0) {
        // Check global cache first for variable font clones
        sk_sp<SkTypeface> varTypeface = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_font_mutex);
            global_typeface_clone_key key = {typeface.get(), weight};
            auto it = g_variable_clone_cache.find(key);
            if (it != g_variable_clone_cache.end()) {
                varTypeface = it->second;
            }
        }

        if (varTypeface) {
            SkFont* font = new SkFont(varTypeface, size);
            font->setSubpixel(true);
            font->setLinearMetrics(true);
            font->setEmbeddedBitmaps(true);
            font->setHinting(SkFontHinting::kNone);
            font->setEdging(SkFont::Edging::kAntiAlias);
            return font;
        }

        std::vector<SkFontArguments::VariationPosition::Coordinate> coords(count);
        typeface->getVariationDesignPosition(SkSpan(coords));

        bool found = false;
        for (auto& coord : coords) {
            if (coord.axis == SkSetFourByteTag('w', 'g', 'h', 't')) {
                coord.value = (float)weight;
                found = true;
                break;
            }
        }

        if (found) {
            SkFontArguments args;
            SkFontArguments::VariationPosition pos = {coords.data(), (int)coords.size()};
            args.setVariationDesignPosition(pos);
            varTypeface = typeface->makeClone(args);
            if (varTypeface) {
                std::lock_guard<std::mutex> lock(g_font_mutex);
                global_typeface_clone_key key = {typeface.get(), weight};
                g_variable_clone_cache[key] = varTypeface;

                SkFont* font = new SkFont(varTypeface, size);
                font->setSubpixel(true);
                font->setLinearMetrics(true);
                font->setEmbeddedBitmaps(true);
                font->setHinting(SkFontHinting::kNone);
                font->setEdging(SkFont::Edging::kAntiAlias);
                return font;
            }
        }
    }

    SkFont* font = new SkFont(typeface, size);
    font->setSubpixel(true);
    font->setLinearMetrics(true);
    font->setEmbeddedBitmaps(true);
    font->setHinting(SkFontHinting::kNone);
    font->setEdging(SkFont::Edging::kAntiAlias);
    return font;
}

std::string SatoruFontManager::cleanName(std::string_view name) const {
    std::string res;
    res.reserve(name.length());
    for (char c : name) {
        if (c == '\'' || c == '\"') continue;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        res += (char)tolower(c);
    }
    return res;
}

void SatoruFontManager::parseUnicodeRange(
    const std::string& rangeStr, std::vector<std::pair<uint32_t, uint32_t>>& outRanges) const {
    if (rangeStr.empty()) return;

    std::stringstream ss(rangeStr);
    std::string segment;
    while (std::getline(ss, segment, ',')) {
        segment = trim(segment);
        if (segment.empty()) continue;

        size_t uPos = segment.find("U+");
        if (uPos == std::string::npos) uPos = segment.find("u+");
        if (uPos != std::string::npos) {
            segment = segment.substr(uPos + 2);
        }

        size_t dashPos = segment.find('-');
        uint32_t start = 0, end = 0;

        try {
            if (segment.find('?') != std::string::npos) {
                std::string minStr = segment;
                std::string maxStr = segment;
                std::replace(minStr.begin(), minStr.end(), '?', '0');
                std::replace(maxStr.begin(), maxStr.end(), '?', 'F');
                start = std::stoul(minStr, nullptr, 16);
                end = std::stoul(maxStr, nullptr, 16);
            } else if (dashPos != std::string::npos) {
                std::string startStr = segment.substr(0, dashPos);
                std::string endStr = segment.substr(dashPos + 1);
                start = std::stoul(startStr, nullptr, 16);
                end = std::stoul(endStr, nullptr, 16);
            } else {
                start = std::stoul(segment, nullptr, 16);
                end = start;
            }
            outRanges.push_back({start, end});
        } catch (...) {
        }
    }
}

bool SatoruFontManager::checkUnicodeRange(
    char32_t codepoint, const std::vector<std::pair<uint32_t, uint32_t>>& ranges) const {
    for (const auto& range : ranges) {
        if (codepoint >= range.first && codepoint <= range.second) return true;
    }
    return false;
}

std::string SatoruFontManager::generateFontFaceCSS() const {
    std::stringstream ss;
    std::set<std::string> seen_urls;

    for (const auto& entry : m_fontFaces) {
        const auto& req = entry.first;
        for (const auto& src : entry.second) {
            std::string key =
                req.family + std::to_string(req.weight) + std::to_string((int)req.slant) + src.url;
            if (seen_urls.count(key)) continue;
            seen_urls.insert(key);

            ss << "@font-face {\n"
               << "  font-family: '" << req.family << "';\n"
               << "  font-weight: " << req.weight << ";\n"
               << "  font-style: "
               << (req.slant == SkFontStyle::kUpright_Slant ? "normal" : "italic") << ";\n"
               << "  src: url('" << src.url << "');\n";
            if (!src.unicode_range.empty()) {
                ss << "  unicode-range: " << src.unicode_range << ";\n";
            }
            ss << "}\n";
        }
    }
    return ss.str();
}

SkFont SatoruFontManager::selectFont(char32_t u, font_info* fi, SkFont* lastSelectedFont,
                                     const satoru::UnicodeService& unicode) {
    SkFont* selected_font = nullptr;
    bool is_emoji = unicode.isEmoji(u);

    if (is_emoji) {
        // Pass 0: Exact match for notocoloremoji
        auto it = m_typefaceCache.find("notocoloremoji");
        if (it != m_typefaceCache.end()) {
            for (auto const& tc : it->second) {
                if (tc.typeface->unicharToGlyph(u) != 0) {
                    static std::map<SkTypefaceID, std::unique_ptr<SkFont>> s_color_cache;
                    SkTypefaceID id = tc.typeface->uniqueID();
                    if (s_color_cache.find(id) == s_color_cache.end()) {
                        s_color_cache[id] = std::unique_ptr<SkFont>(
                            createSkFont(tc.typeface, fi->fonts[0]->getSize(), 400));
                    }
                    selected_font = s_color_cache[id].get();
                    goto emoji_found;
                }
            }
        }

        // PASS 1: Mandatory Color Font Search (name contains "color")
        if (!selected_font) {
            for (auto const& [name, typefaces] : m_typefaceCache) {
                std::string lowerName = name;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                if (lowerName.find("color") != std::string::npos) {
                    for (auto const& tc : typefaces) {
                        if (tc.typeface->unicharToGlyph(u) != 0) {
                            static std::map<SkTypefaceID, std::unique_ptr<SkFont>> s_color_cache;
                            SkTypefaceID id = tc.typeface->uniqueID();
                            if (s_color_cache.find(id) == s_color_cache.end()) {
                                s_color_cache[id] = std::unique_ptr<SkFont>(
                                    createSkFont(tc.typeface, fi->fonts[0]->getSize(), 400));
                            }
                            selected_font = s_color_cache[id].get();
                            break;
                        }
                    }
                }
                if (selected_font) break;
            }
        }

        // PASS 2: General Emoji Font Search
        if (!selected_font) {
            for (auto const& [name, typefaces] : m_typefaceCache) {
                std::string lowerName = name;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                if (lowerName.find("emoji") != std::string::npos) {
                    for (auto const& tc : typefaces) {
                        if (tc.typeface->unicharToGlyph(u) != 0) {
                            static std::map<SkTypefaceID, std::unique_ptr<SkFont>> s_emoji_cache;
                            SkTypefaceID id = tc.typeface->uniqueID();
                            if (s_emoji_cache.find(id) == s_emoji_cache.end()) {
                                s_emoji_cache[id] = std::unique_ptr<SkFont>(
                                    createSkFont(tc.typeface, fi->fonts[0]->getSize(), 400));
                            }
                            selected_font = s_emoji_cache[id].get();
                            break;
                        }
                    }
                }
                if (selected_font) break;
            }
        }
    }
emoji_found:

    // 2. Check if the previous font is usable (MRU optimization)
    if (!selected_font && lastSelectedFont) {
        SkGlyphID glyph = lastSelectedFont->getTypeface()->unicharToGlyph(u);
        if (glyph != 0 || unicode.isMark(u)) {
            selected_font = lastSelectedFont;
        }
    }

    // 3. If still no font selected, search through all fonts
    if (!selected_font) {
        for (auto f : fi->fonts) {
            if (f->getTypeface()->unicharToGlyph(u) != 0) {
                selected_font = f;
                break;
            }
        }
        if (!selected_font) selected_font = fi->fonts[0];
    }

    SkFont font = *selected_font;
    font.setSize(fi->fonts[0]->getSize());
    if (fi->fake_bold) font.setEmbolden(true);
    if (fi->fake_italic) font.setSkewX(-0.25f);

    if (is_emoji) {
        font.setEmbeddedBitmaps(true);
        font.setHinting(SkFontHinting::kNone);
        font.setEdging(SkFont::Edging::kAntiAlias);
    }

    return font;
}
