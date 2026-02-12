#include "font_manager.h"

#include <algorithm>
#include <regex>
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

void SatoruFontManager::loadFont(const char *name, const uint8_t *data, int size) {
    auto data_ptr = SkData::MakeWithCopy(data, size);
    if (!m_fontMgr) m_fontMgr = SkFontMgr_New_Custom_Empty();

    auto typeface = m_fontMgr->makeFromData(std::move(data_ptr));
    if (typeface) {
        std::string cleaned = cleanName(name);
        m_typefaceCache[cleaned].push_back(typeface);
        if (!m_defaultTypeface) m_defaultTypeface = typeface;

        std::stringstream ss;
        ss << "loadFont: Loaded '" << cleaned
           << "' (Total for family: " << m_typefaceCache[cleaned].size() << ")";
        satoru_log(LogLevel::Info, ss.str().c_str());
    } else {
        std::stringstream ss;
        ss << "loadFont: FAILED to load font data (" << size << " bytes)";
        satoru_log(LogLevel::Error, ss.str().c_str());
    }
}

void SatoruFontManager::clear() {
    m_typefaceCache.clear();
    m_fontFaces.clear();
    m_fallbackTypefaces.clear();
    m_defaultTypeface = nullptr;
}

void SatoruFontManager::scanFontFaces(const std::string &css) {
    std::regex fontFaceRegex(R"(@font-face\s*\{([^{}]*)\})", std::regex::icase);
    std::regex familyRegex(R"(font-family:\s*([^;\}]+);?)", std::regex::icase);
    std::regex weightRegex(R"(font-weight:\s*([^;\}]+);?)", std::regex::icase);
    std::regex styleRegex(R"(font-style:\s*([^;\}]+);?)", std::regex::icase);
    std::regex urlRegex(R"(url\s*\(\s*['"]?([^'\"\)]+)['"]?\s*\))", std::regex::icase);
    std::regex unicodeRangeRegex(R"(unicode-range:\s*([^;\}]+);?)", std::regex::icase);

    auto words_begin = std::sregex_iterator(css.begin(), css.end(), fontFaceRegex);
    for (std::sregex_iterator i = words_begin; i != std::sregex_iterator(); ++i) {
        std::string body = (*i)[1].str();
        std::smatch m;
        std::string family = "";
        std::vector<int> weights;
        SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant;

        if (std::regex_search(body, m, familyRegex)) family = cleanName(m[1].str().c_str());

        if (std::regex_search(body, m, weightRegex)) {
            std::string w = trim(m[1].str());
            if (w == "bold")
                weights.push_back(700);
            else if (w == "normal")
                weights.push_back(400);
            else {
                try {
                    std::regex rangeRegex(R"((\d+)\s+(\d+))");
                    std::smatch rm;
                    if (std::regex_search(w, rm, rangeRegex)) {
                        int start = std::stoi(rm[1].str());
                        int end = std::stoi(rm[2].str());
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

        if (std::regex_search(body, m, styleRegex)) {
            std::string s = trim(m[1].str());
            if (s == "italic" || s == "oblique") slant = SkFontStyle::kItalic_Slant;
        }

        if (!family.empty() && std::regex_search(body, m, urlRegex)) {
            font_face_source src;
            src.url = trim(m[1].str());
            if (std::regex_search(body, m, unicodeRangeRegex)) {
                src.unicode_range = trim(m[1].str());
            }

            for (int w : weights) {
                font_request req;
                req.family = family;
                req.weight = w;
                req.slant = slant;
                m_fontFaces[req].push_back(src);
            }
        }
    }
}

std::vector<std::string> SatoruFontManager::getFontUrls(const std::string &family, int weight,
                                                        SkFontStyle::Slant slant) const {
    font_request req;
    req.family = cleanName(family.c_str());
    req.weight = weight;
    req.slant = slant;

    std::vector<std::string> urls;
    auto it = m_fontFaces.find(req);
    if (it != m_fontFaces.end()) {
        for (const auto &src : it->second) {
            urls.push_back(src.url);
        }
    }

    if (urls.empty()) {
        for (const auto &entry : m_fontFaces) {
            if (entry.first.family == req.family && entry.first.slant == req.slant) {
                for (const auto &src : entry.second) {
                    if (std::find(urls.begin(), urls.end(), src.url) == urls.end()) {
                        urls.push_back(src.url);
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
        return it->second;
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
                return new SkFont(varTypeface, size);
            }
        }
    }

    return new SkFont(typeface, size);
}

std::string SatoruFontManager::cleanName(const char *name) const {
    if (!name) return "";
    std::string s = name;
    std::string res = "";
    for (char c : s) {
        if (c == '\'' || c == '\"') continue;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        res += (char)tolower(c);
    }
    return res;
}
