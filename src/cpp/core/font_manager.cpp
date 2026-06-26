#include "font_manager.h"

#include <algorithm>
#include <charconv>
#include <memory>
#include <mutex>
#include <sstream>

#include "core/text/unicode_service.h"
#include "include/core/SkData.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontArguments.h"
#include "include/core/SkFontTypes.h"
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

std::string_view trim_view(std::string_view s) {
    auto start = s.find_first_not_of(" \t\r\n'\"");
    if (start == std::string_view::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n'\"");
    return s.substr(start, end - start + 1);
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
    if (size == 0) return 0;
    uint64_t h = size;
    size_t head = size < 256 ? size : 256;
    for (size_t i = 0; i < head; ++i) {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    size_t tail_start = size > 256 ? size - 256 : 0;
    for (size_t i = tail_start; i < size; ++i) {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

sk_sp<SkFontMgr> get_global_font_mgr() {
    static sk_sp<SkFontMgr> mgr = SkFontMgr_New_Custom_Empty();
    return mgr;
}

char ascii_lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c; }

bool starts_with_ascii_ci(std::string_view s, size_t pos, std::string_view target) {
    if (target.empty()) return true;
    if (pos + target.size() > s.size()) return false;
    for (size_t i = 0; i < target.size(); ++i) {
        if (ascii_lower(s[pos + i]) != ascii_lower(target[i])) return false;
    }
    return true;
}

size_t find_ascii_ci(std::string_view s, std::string_view target, size_t start = 0) {
    if (target.empty()) return start <= s.size() ? start : std::string_view::npos;
    if (start + target.size() > s.size()) return std::string_view::npos;
    for (size_t i = start; i <= s.size() - target.size(); ++i) {
        if (starts_with_ascii_ci(s, i, target)) return i;
    }
    return std::string_view::npos;
}

bool contains_ascii_ci(std::string_view s, std::string_view target) {
    return find_ascii_ci(s, target) != std::string_view::npos;
}

bool equals_ascii_ci(std::string_view a, std::string_view b) {
    a = trim_view(a);
    b = trim_view(b);
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (ascii_lower(a[i]) != ascii_lower(b[i])) return false;
    }
    return true;
}

std::string_view css_property_value(std::string_view body, std::string_view property) {
    size_t prop_pos = find_ascii_ci(body, property);
    if (prop_pos == std::string_view::npos) return {};
    size_t colon_pos = body.find(':', prop_pos + property.size());
    if (colon_pos == std::string_view::npos) return {};
    size_t value_start = colon_pos + 1;
    size_t value_end = body.find(';', value_start);
    if (value_end == std::string_view::npos) value_end = body.size();
    return body.substr(value_start, value_end - value_start);
}

bool parse_int(std::string_view s, int& value) {
    s = trim_view(s);
    if (s.empty()) return false;
    const char* first = s.data();
    const char* last = s.data() + s.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    return ec == std::errc() && ptr == last;
}

bool parse_weight_range(std::string_view s, int& start, int& end) {
    s = trim_view(s);
    size_t sep = s.find_first_of(" \t\r\n");
    if (sep == std::string::npos) return false;
    std::string_view first = s.substr(0, sep);
    size_t second_start = s.find_first_not_of(" \t\r\n", sep);
    if (second_start == std::string::npos) return false;
    std::string_view second = s.substr(second_start);
    return parse_int(first, start) && parse_int(second, end);
}
}  // namespace

SatoruFontManager::SatoruFontManager() { m_fontMgr = get_global_font_mgr(); }

bool SatoruFontManager::loadFont(const char* name, const uint8_t* data, int size, const char* url) {
    if (!name || !*name) return false;
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
        bool changed = false;
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
                changed = true;
                if (!m_defaultTypeface) m_defaultTypeface = typeface;

                // If it's an emoji font, also register it under "notocoloremoji"
                if (cleaned.find("emoji") != std::string::npos) {
                    m_typefaceCache["notocoloremoji"].push_back({typeface, intended_style});
                }
            }
        }
        return changed;
    } else {
        SATORU_LOG_ERROR("[Satoru] FAILED to create typeface for '%s'", name);
    }
    return false;
}

void SatoruFontManager::clear() {
    m_typefaceCache.clear();
    m_fontFaces.clear();
    m_fallbackTypefaces.clear();
    m_defaultTypeface = nullptr;
}

void SatoruFontManager::scanFontFaces(const std::string& css) {
    std::string_view css_sv = css;
    if (!contains_ascii_ci(css_sv, "@font-face")) return;

    size_t pos = 0;

    while (true) {
        size_t start_pos = find_ascii_ci(css_sv, "@font-face", pos);
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

        if (std::string_view family_value = css_property_value(body_sv, "font-family");
            !family_value.empty()) {
            family = cleanName(family_value);
        }

        if (std::string_view weight_value = css_property_value(body_sv, "font-weight");
            !weight_value.empty()) {
            std::string_view w = trim_view(weight_value);
            if (equals_ascii_ci(w, "bold"))
                weights.push_back(700);
            else if (equals_ascii_ci(w, "normal"))
                weights.push_back(400);
            else {
                int start = 0;
                int end = 0;
                if (parse_weight_range(w, start, end)) {
                    for (int v = 100; v <= 900; v += 100) {
                        if (v >= start && v <= end) weights.push_back(v);
                    }
                } else {
                    int parsed = 0;
                    if (parse_int(w, parsed)) weights.push_back(parsed);
                }
            }
        }

        if (weights.empty()) weights.push_back(400);

        if (std::string_view style_value = css_property_value(body_sv, "font-style");
            !style_value.empty()) {
            std::string_view s = trim_view(style_value);
            if (equals_ascii_ci(s, "italic") || equals_ascii_ci(s, "oblique"))
                slant = SkFontStyle::kItalic_Slant;
        }

        if (!family.empty()) {
            std::string_view best_url;
            int best_score = -1;

            size_t url_search_pos = 0;
            while (true) {
                size_t url_idx = find_ascii_ci(body_sv, "url(", url_search_pos);
                if (url_idx == std::string_view::npos) break;

                size_t url_content_start = url_idx + 4;
                size_t url_content_end = body_sv.find(')', url_content_start);
                if (url_content_end == std::string_view::npos) break;

                std::string_view url = trim_view(
                    body_sv.substr(url_content_start, url_content_end - url_content_start));
                url_search_pos = url_content_end + 1;

                int score = 0;
                if (contains_ascii_ci(url, "woff2"))
                    score = 5;
                else if (contains_ascii_ci(url, "woff"))
                    score = 4;
                else if (contains_ascii_ci(url, "ttf") || contains_ascii_ci(url, "truetype"))
                    score = 3;
                else if (contains_ascii_ci(url, "otf") || contains_ascii_ci(url, "opentype"))
                    score = 2;
                else if (contains_ascii_ci(url, "ttc"))
                    score = 1;
                else if (contains_ascii_ci(url, ".eot"))
                    score = -1;

                if (score > best_score) {
                    best_score = score;
                    best_url = std::move(url);
                }
            }

            if (!best_url.empty() && best_score >= 0) {
                font_face_source src;
                src.url = std::string(best_url);

                if (std::string_view range_value = css_property_value(body_sv, "unicode-range");
                    !range_value.empty()) {
                    src.unicode_range = trim(range_value);
                    parseUnicodeRange(src.unicode_range, src.ranges);
                }

                for (int w : weights) {
                    font_request req;
                    req.family = family;
                    req.weight = w;
                    req.slant = slant;

                    auto& sources = m_fontFaces[req];
                    bool duplicate = false;
                    for (const auto& existing_src : sources) {
                        if (existing_src.url == src.url) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate) {
                        sources.push_back(src);
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
    urls.reserve(4);
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
                    for (const auto& range : src.ranges) {
                        auto cp = usedCodepoints->lower_bound((char32_t)range.first);
                        if (cp != usedCodepoints->end() && *cp <= range.second) {
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
                            for (const auto& range : src.ranges) {
                                auto cp = usedCodepoints->lower_bound((char32_t)range.first);
                                if (cp != usedCodepoints->end() && *cp <= range.second) {
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

bool SatoruFontManager::hasFontFaceSource(const std::string& family, const std::string& url) const {
    std::string cleanedFamily = cleanName(family);
    for (const auto& entry : m_fontFaces) {
        if (entry.first.family != cleanedFamily) continue;
        for (const auto& src : entry.second) {
            if (src.url == url) return true;
        }
    }
    return false;
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
    return selectFont(u, fi, lastSelectedFont, unicode, unicode.isEmoji(u), unicode.isMark(u));
}

SkFont SatoruFontManager::selectFont(char32_t u, font_info* fi, SkFont* lastSelectedFont,
                                     const satoru::UnicodeService& unicode, bool is_emoji,
                                     bool is_mark) {
    SkFont* selected_font = nullptr;

    if (!is_mark) {
        auto cached = fi->selected_font_cache.find(u);
        if (cached != fi->selected_font_cache.end()) {
            return cached->second;
        }
    }

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
        if (glyph != 0 || is_mark) {
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

    if (!is_mark) {
        fi->selected_font_cache.emplace(u, font);
    }

    return font;
}
