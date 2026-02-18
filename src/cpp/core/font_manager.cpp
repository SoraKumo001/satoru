#include "font_manager.h"

#include <algorithm>
#include <ctre.hpp>
#include <sstream>

#include "../api/satoru_api.h"
#include "include/core/SkData.h"
#include "include/core/SkFontArguments.h"
#include "include/core/SkSpan.h"
#include "utils/skia_utils.h"

// External declaration for the custom empty font manager
extern sk_sp<SkFontMgr> SkFontMgr_New_Custom_Empty();

#ifndef SkSetFourByteTag
#define SkSetFourByteTag(a, b, c, d) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))
#endif

namespace {
std::string trim(const std::string &s) {
    auto start = s.find_first_not_of(" \t\r\n'\"");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n'\"");
    return s.substr(start, end - start + 1);
}
}  // namespace

SatoruFontManager::SatoruFontManager() { m_fontMgr = SkFontMgr_New_Custom_Empty(); }

void SatoruFontManager::loadFont(const char *name, const uint8_t *data, int size, const char *url) {
    std::string cleaned = cleanName(name);
    if (!m_fontMgr) m_fontMgr = SkFontMgr_New_Custom_Empty();

    sk_sp<SkTypeface> typeface;

    // Check URL cache first to share typeface objects
    if (url && *url) {
        auto it = m_urlToTypeface.find(url);
        if (it != m_urlToTypeface.end()) {
            typeface = it->second;
        }
    }

    if (!typeface) {
        auto data_ptr = SkData::MakeWithCopy(data, size);
        typeface = m_fontMgr->makeFromData(std::move(data_ptr));
        if (url && *url && typeface) {
            m_urlToTypeface[url] = typeface;
        }
    }

    if (typeface) {
        std::stringstream ss;
        ss << "loadFont: SUCCESS loaded font '" << cleaned << "' (" << size << " bytes) from "
           << (url ? url : "memory");
        satoru_log(LogLevel::Info, ss.str().c_str());

        bool duplicate = false;
        for (const auto &existing : m_typefaceCache[cleaned]) {
            if (existing.get() == typeface.get()) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            m_typefaceCache[cleaned].push_back(typeface);
            if (!m_defaultTypeface) m_defaultTypeface = typeface;
        }
    } else {
        std::stringstream ss;
        ss << "loadFont: FAILED to load font data (" << size << " bytes)";
        satoru_log(LogLevel::Error, ss.str().c_str());
    }
}

void SatoruFontManager::clear() {
    m_typefaceCache.clear();
    m_urlToTypeface.clear();
    m_fontFaces.clear();
    m_fallbackTypefaces.clear();
    m_variableCloneCache.clear();
    m_defaultTypeface = nullptr;
}

void SatoruFontManager::scanFontFaces(const std::string &css) {
    for (auto [whole, body] : ctre::search_all<R"((?i)@font-face\s*\{([^{}]*)\})">(css)) {
        std::string body_str = body.to_string();
        std::string family = "";
        std::vector<int> weights;
        SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant;

        if (auto m = ctre::search<R"((?i)font-family:\s*([^;\}]+);?)">(body_str)) {
            family = cleanName(m.get<1>().to_string().c_str());
        }

        if (auto m = ctre::search<R"((?i)font-weight:\s*([^;\}]+);?)">(body_str)) {
            std::string w = trim(m.get<1>().to_string());
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

        if (auto m = ctre::search<R"((?i)font-style:\s*([^;\}]+);?)">(body_str)) {
            std::string s = trim(m.get<1>().to_string());
            if (s == "italic" || s == "oblique") slant = SkFontStyle::kItalic_Slant;
        }

        if (!family.empty()) {
            if (auto m = ctre::search<R"((?i)url\s*\(\s*['"]?([^'\"\)]+)['"]?\s*\))">(body_str)) {
                font_face_source src;
                src.url = trim(m.get<1>().to_string());
                if (auto m2 = ctre::search<R"((?i)unicode-range:\s*([^;\}]+);?)">(body_str)) {
                    src.unicode_range = trim(m2.get<1>().to_string());
                    parseUnicodeRange(src.unicode_range, src.ranges);
                }

                for (int w : weights) {
                    font_request req;
                    req.family = family;
                    req.weight = w;
                    req.slant = slant;

                    bool duplicate = false;
                    for (const auto &existing_src : m_fontFaces[req]) {
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
    const std::string &family, int weight, SkFontStyle::Slant slant,
    const std::set<char32_t> *usedCodepoints) const {
    font_request req;
    req.family = cleanName(family.c_str());
    req.weight = weight;
    req.slant = slant;

    std::vector<std::string> urls;
    auto it = m_fontFaces.find(req);
    if (it != m_fontFaces.end()) {
        for (const auto &src : it->second) {
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

    if (urls.empty()) {
        for (const auto &entry : m_fontFaces) {
            if (entry.first.family == req.family && entry.first.slant == req.slant) {
                for (const auto &src : entry.second) {
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

    if (urls.empty() &&
        (req.family == "sans-serif" || req.family == "serif" || req.family == "monospace")) {
        if (!m_fontFaces.empty()) {
            std::string fallbackFamily = m_fontFaces.begin()->first.family;
            for (const auto &entry : m_fontFaces) {
                if (entry.first.family == fallbackFamily) {
                    for (const auto &src : entry.second) {
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
    }

    return urls;
}

std::string SatoruFontManager::getFontUrl(const std::string &family, int weight,
                                          SkFontStyle::Slant slant) const {
    auto urls = getFontUrls(family, weight, slant);
    return urls.empty() ? "" : urls[0];
}

std::vector<sk_sp<SkTypeface>> SatoruFontManager::matchFonts(const std::string &family, int weight,
                                                             SkFontStyle::Slant slant) {
    std::string cleanFamily = cleanName(family.c_str());
    auto it = m_typefaceCache.find(cleanFamily);
    if (it != m_typefaceCache.end()) {
        const auto &typefaces = it->second;
        if (typefaces.empty()) return {};

        // Find the best match for weight and slant
        sk_sp<SkTypeface> bestMatch = nullptr;
        int minDiff = 10000;

        for (const auto &tf : typefaces) {
            SkFontStyle style = tf->fontStyle();
            int diff = std::abs(style.weight() - weight);
            if (style.slant() != slant) diff += 1000;

            if (diff < minDiff) {
                minDiff = diff;
                bestMatch = tf;
            }
        }

        if (bestMatch) {
            // Also include other typefaces that might have different glyph sets (subsets)
            // but have the same weight/slant. This is important for Google Fonts subsets.
            std::vector<sk_sp<SkTypeface>> matches;
            SkFontStyle bestStyle = bestMatch->fontStyle();

            for (const auto &tf : typefaces) {
                if (tf->fontStyle() == bestStyle) {
                    matches.push_back(tf);
                }
            }
            return matches;
        }
    }

    if (cleanFamily == "serif" || cleanFamily == "sans-serif" || cleanFamily == "monospace") {
        if (m_defaultTypeface) return {m_defaultTypeface};
    }

    return {};
}

SkFont *SatoruFontManager::createSkFont(sk_sp<SkTypeface> typeface, float size, int weight) {
    if (!typeface) return nullptr;

    int count = typeface->getVariationDesignPosition(
        SkSpan<SkFontArguments::VariationPosition::Coordinate>());
    if (count > 0) {
        // Check cache first
        typeface_clone_key key = {typeface.get(), weight};
        auto it = m_variableCloneCache.find(key);
        if (it != m_variableCloneCache.end()) {
            return new SkFont(it->second, size);
        }

        std::vector<SkFontArguments::VariationPosition::Coordinate> coords(count);
        typeface->getVariationDesignPosition(SkSpan(coords));

        bool found = false;
        for (auto &coord : coords) {
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
            sk_sp<SkTypeface> varTypeface = typeface->makeClone(args);
            if (varTypeface) {
                m_variableCloneCache[key] = varTypeface;
                SkFont *font = new SkFont(varTypeface, size);
                font->setSubpixel(true);
                font->setLinearMetrics(true);
                font->setEmbeddedBitmaps(true);
                font->setHinting(SkFontHinting::kNone);
                font->setEdging(SkFont::Edging::kAntiAlias);
                return font;
            }
        }
    }

    SkFont *font = new SkFont(typeface, size);
    font->setSubpixel(true);
    font->setLinearMetrics(true);
    font->setEmbeddedBitmaps(true);
    font->setHinting(SkFontHinting::kNone);
    font->setEdging(SkFont::Edging::kAntiAlias);
    return font;
}

std::string SatoruFontManager::cleanName(const char *name) const {
    if (!name) return "";
    size_t len = strlen(name);
    std::string res;
    res.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        char c = name[i];
        if (c == '\'' || c == '\"') continue;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        res += (char)tolower(c);
    }
    return res;
}

void SatoruFontManager::parseUnicodeRange(
    const std::string &rangeStr, std::vector<std::pair<uint32_t, uint32_t>> &outRanges) const {
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
            if (dashPos != std::string::npos) {
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
    char32_t codepoint, const std::vector<std::pair<uint32_t, uint32_t>> &ranges) const {
    for (const auto &range : ranges) {
        if (codepoint >= range.first && codepoint <= range.second) return true;
    }
    return false;
}

std::string SatoruFontManager::generateFontFaceCSS() const {
    std::stringstream ss;
    std::set<std::string> seen_urls;

    for (const auto &entry : m_fontFaces) {
        const auto &req = entry.first;
        for (const auto &src : entry.second) {
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
