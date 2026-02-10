#include "font_manager.h"

#include <algorithm>
#include <regex>
#include <sstream>

#include "include/core/SkData.h"
#include "include/core/SkFontArguments.h"
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
        std::string cleaned = clean_font_name(name);
        m_typefaceCache[cleaned].push_back(typeface);
        if (!m_defaultTypeface) m_defaultTypeface = typeface;
    }
}

void SatoruFontManager::clear() {
    m_typefaceCache.clear();
    m_fontFaces.clear();
    m_fallbackTypefaces.clear();
    m_defaultTypeface = nullptr;
}

void SatoruFontManager::scanFontFaces(const std::string &css) {
    std::regex fontFaceRegex(R"(@font-face\s*\{([^}]+)\})", std::regex::icase);
    std::regex familyRegex(R"(font-family:\s*([^;]+);?)", std::regex::icase);
    std::regex weightRegex(R"(font-weight:\s*([^;]+);?)", std::regex::icase);
    std::regex styleRegex(R"(font-style:\s*([^;]+);?)", std::regex::icase);
    std::regex urlRegex(R"(url\s*\(\s*['"]?([^'\"\)]+)['"]?\s*\))", std::regex::icase);

    auto words_begin = std::sregex_iterator(css.begin(), css.end(), fontFaceRegex);
    for (std::sregex_iterator i = words_begin; i != std::sregex_iterator(); ++i) {
        std::string body = (*i)[1].str();
        std::smatch m;
        font_request req;
        req.weight = 400;
        req.slant = SkFontStyle::kUpright_Slant;

        if (std::regex_search(body, m, familyRegex))
            req.family = clean_font_name(m[1].str().c_str());

        if (std::regex_search(body, m, weightRegex)) {
            std::string w = trim(m[1].str());
            if (w == "bold")
                req.weight = 700;
            else if (w == "normal")
                req.weight = 400;
            else
                try {
                    req.weight = std::stoi(w);
                } catch (...) {
                }
        }

        if (std::regex_search(body, m, styleRegex)) {
            std::string s = trim(m[1].str());
            if (s == "italic" || s == "oblique") req.slant = SkFontStyle::kItalic_Slant;
        }

        if (!req.family.empty() && std::regex_search(body, m, urlRegex))
            m_fontFaces[req] = trim(m[1].str());
    }
}

std::string SatoruFontManager::getFontUrl(const std::string &family, int weight,
                                          SkFontStyle::Slant slant) const {
    std::stringstream ss(family);
    std::string item;
    bool hasGeneric = false;

    while (std::getline(ss, item, ',')) {
        std::string cleanFamily = clean_font_name(trim(item).c_str());
        if (cleanFamily == "sans-serif" || cleanFamily == "serif" || cleanFamily == "monospace") {
            hasGeneric = true;
            continue;
        }

        font_request req = {cleanFamily, weight, slant};
        auto it = m_fontFaces.find(req);
        if (it != m_fontFaces.end()) return it->second;

        int minDiff = 1000;
        std::string bestUrl = "";
        for (const auto &pair : m_fontFaces) {
            if (pair.first.family == cleanFamily && pair.first.slant == slant) {
                int diff = std::abs(pair.first.weight - weight);
                if (diff < minDiff) {
                    minDiff = diff;
                    bestUrl = pair.second;
                }
            }
        }
        if (!bestUrl.empty()) return bestUrl;
    }

    if (hasGeneric && !m_fontFaces.empty()) {
        return m_fontFaces.begin()->second;
    }
    return "";
}

std::vector<sk_sp<SkTypeface>> SatoruFontManager::matchFonts(const std::string &family, int weight,
                                                             SkFontStyle::Slant slant) {
    std::vector<sk_sp<SkTypeface>> result;
    std::stringstream ss(family);
    std::string item;

    while (std::getline(ss, item, ',')) {
        std::string cleaned = clean_font_name(trim(item).c_str());
        if (m_typefaceCache.count(cleaned)) {
            const auto &list = m_typefaceCache[cleaned];
            sk_sp<SkTypeface> bestMatch = nullptr;
            int minDiff = 1000;

            for (const auto &tf : list) {
                SkFontStyle style = tf->fontStyle();
                int diff = std::abs(style.weight() - weight) + (style.slant() == slant ? 0 : 500);
                if (diff < minDiff) {
                    minDiff = diff;
                    bestMatch = tf;
                }
            }
            if (bestMatch) result.push_back(bestMatch);
        }
    }
    return result;
}

SkFont *SatoruFontManager::createSkFont(sk_sp<SkTypeface> typeface, float size, int weight) {
    if (!typeface) return nullptr;

    sk_sp<SkTypeface> tf = typeface;

    // Apply wght variation for Variable Fonts
    SkFontArguments::VariationPosition::Coordinate coords[] = {
        {SkSetFourByteTag('w', 'g', 'h', 't'), (float)weight}};
    SkFontArguments args;
    args.setVariationDesignPosition({coords, 1});

    auto clone = typeface->makeClone(args);
    if (clone) {
        tf = clone;
    }

    return new SkFont(tf, size);
}
