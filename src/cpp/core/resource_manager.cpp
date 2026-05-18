#include "resource_manager.h"

#include <cctype>
#include <cstring>
#include <iostream>

#include "../utils/skia_utils.h"
#include "container_skia.h"
#include "satoru_context.h"
#include "utils/logging.h"

extern container_skia* g_discovery_container;

namespace {
bool starts_with_ascii_ci(const std::string& s, size_t pos, const char* needle) {
    for (size_t i = 0; needle[i] != '\0'; ++i) {
        if (pos + i >= s.size()) return false;
        if (std::tolower((unsigned char)s[pos + i]) != std::tolower((unsigned char)needle[i])) {
            return false;
        }
    }
    return true;
}

bool contains_ascii(const uint8_t* data, size_t size, const char* needle) {
    size_t needleLen = std::strlen(needle);
    if (!data || needleLen == 0 || size < needleLen) return false;

    for (size_t i = 0; i <= size - needleLen; ++i) {
        if (std::memcmp(data + i, needle, needleLen) == 0) {
            return true;
        }
    }
    return false;
}

bool contains_ascii_ci(const std::string& s, const char* needle) {
    if (!needle || !*needle) return true;
    size_t needleLen = std::strlen(needle);
    if (s.size() < needleLen) return false;

    for (size_t i = 0; i <= s.size() - needleLen; ++i) {
        if (starts_with_ascii_ci(s, i, needle)) return true;
    }
    return false;
}

bool contains_any_ascii_ci(const std::string& s, const char* const* needles, size_t needleCount) {
    for (size_t i = 0; i < s.size(); ++i) {
        for (size_t j = 0; j < needleCount; ++j) {
            if (starts_with_ascii_ci(s, i, needles[j])) return true;
        }
    }
    return false;
}

bool looks_like_font_url(const std::string& url) {
    static constexpr const char* needles[] = {".woff2",    ".woff",    ".ttf",
                                              ".otf",      ".ttc",     "font-woff",
                                              "font-ttf",  "font-otf", "application/font"};
    return contains_any_ascii_ci(url, needles, sizeof(needles) / sizeof(needles[0]));
}

bool contains_weight_marker(const std::string& url, const char* a, const char* b) {
    const char* needles[] = {a, b};
    return contains_any_ascii_ci(url, needles, 2);
}

std::string replace_font_family_names(const std::string& css, const std::string& name) {
    std::string result;
    result.reserve(css.size());
    size_t pos = 0;

    while (pos < css.size()) {
        size_t found = std::string::npos;
        for (size_t i = pos; i < css.size(); ++i) {
            if (starts_with_ascii_ci(css, i, "font-family")) {
                found = i;
                break;
            }
        }
        if (found == std::string::npos) {
            result.append(css, pos, std::string::npos);
            break;
        }

        result.append(css, pos, found - pos);
        size_t colon = css.find(':', found + 11);
        if (colon == std::string::npos) {
            result.append(css, found, std::string::npos);
            break;
        }

        size_t valueStart = colon + 1;
        while (valueStart < css.size() && std::isspace((unsigned char)css[valueStart])) {
            valueStart++;
        }
        size_t valueEnd = valueStart;
        while (valueEnd < css.size() && css[valueEnd] != ';' && css[valueEnd] != '}') {
            valueEnd++;
        }

        result.append(css, found, valueStart - found);
        result.push_back('\'');
        result.append(name);
        result.push_back('\'');
        pos = valueEnd;
    }

    return result;
}
}  // namespace

ResourceManager::ResourceManager(SatoruContext& context) : m_context(context) {
    m_requestedUrls.reserve(64);
    m_resolvedUrls.reserve(64);
    m_urlToNames.reserve(64);
}

void ResourceManager::request(const std::string& url, const std::string& name, ResourceType type,
                              bool redraw_on_ready, const std::string& characters) {
    if (url.empty()) return;
    if (m_resolvedUrls.count(url)) return;  // Already resolved

    // Track name association
    if (!name.empty()) {
        m_urlToNames[url].insert(name);
    }

    if (url.compare(0, 5, "data:") == 0) {
        size_t commaPos = url.find(',');
        if (commaPos != std::string::npos) {
            std::string rawData = url.substr(commaPos + 1);
            size_t base64Pos = url.find(";base64", 0);
            if (base64Pos != std::string::npos && base64Pos < commaPos) {
                std::vector<uint8_t> decoded = base64_decode(rawData);
                if (!decoded.empty()) {
                    this->add(url, decoded.data(), decoded.size(), type);
                    return;
                }
            } else {
                std::string decodedData = url_decode(rawData);
                this->add(url, (const uint8_t*)decodedData.data(), decodedData.size(), type);
                return;
            }
        }
    }

    // Check if this URL is already requested (regardless of name/type)
    if (!m_requestedUrls.insert(url).second) return;

    m_requests.insert({url, name, characters, type, redraw_on_ready});
}

std::vector<ResourceRequest> ResourceManager::getPendingRequests() {
    std::vector<ResourceRequest> result(m_requests.begin(), m_requests.end());
    m_requests.clear();
    m_requestedUrls.clear();
    return result;
}

void ResourceManager::add(const std::string& url, const uint8_t* data, size_t size,
                          ResourceType type) {
    if (url.empty()) return;
    if (m_resolvedUrls.count(url)) return;

    m_resolvedUrls[url] = type;

    if (!data || size == 0) {
        return;
    }

    if (type == ResourceType::Font) {
        // Check if the data is actually a CSS file (e.g. from Google Fonts API)
        if (size > 0 && size < 1024 * 1024) {  // Limit check to 1MB
            // Simple heuristic: check for @font-face
            if (contains_ascii(data, size, "@font-face")) {
                std::string content((const char*)data, size);
                if (m_context.addCss(content, CssChangeKind::FontResourceCss)) {
                    m_context.fontManager.scanFontFaces(content);
                }

                // Add aliases for requested names (e.g. serif -> Noto Serif JP)
                auto it = m_urlToNames.find(url);
                if (it != m_urlToNames.end()) {
                    for (const auto& name : it->second) {
                        // Check if the name is already in the CSS (simple check)
                        if (content.find("'" + name + "'") != std::string::npos) continue;
                        if (content.find("\"" + name + "\"") != std::string::npos) continue;

                        std::string alias_css = replace_font_family_names(content, name);
                        if (m_context.addCss(alias_css, CssChangeKind::FontAliasCss)) {
                            m_context.fontManager.scanFontFaces(alias_css);
                        }
                    }
                }

                return;
            }
        }

        // Attempt to register under all requested names associated with this URL
        bool registered = false;
        std::string primaryName = "";

        auto it = m_urlToNames.find(url);
        if (it != m_urlToNames.end()) {
            for (const auto& name : it->second) {
                m_context.loadFont(name.c_str(), data, size, url.c_str());
                if (primaryName.empty()) primaryName = name;
                registered = true;
                m_context.noteFontResourceNamedLoad();

                if (name == "sans-serif" || name == "serif" || name == "monospace") {
                    auto tfs =
                        m_context.fontManager.matchFonts(name, 400, SkFontStyle::kUpright_Slant);
                    if (!tfs.empty()) {
                        m_context.fontManager.addFallbackTypeface(tfs[0]);
                    }
                }
            }
        }

        // Fallback if no specific name was associated (e.g. pre-loading)
        if (!registered) {
            // Infer name from URL
            std::string fontName = url;
            size_t lastSlash = url.find_last_of('/');
            if (lastSlash != std::string::npos) {
                fontName = url.substr(lastSlash + 1);
                size_t lastDot = fontName.find_last_of('.');
                if (lastDot != std::string::npos) fontName = fontName.substr(0, lastDot);
            }
            if (url.find("noto-sans-jp") != std::string::npos) fontName = "Noto Sans JP";
            primaryName = fontName;
            m_context.loadFont(fontName.c_str(), data, size, url.c_str());
            m_context.noteFontResourceFallbackLoad();
        }

        // Generate @font-face and add it to extra CSS so litehtml knows about it
        std::string weight = "400";
        if (contains_weight_marker(url, "700", "bold"))
            weight = "700";
        else if (contains_weight_marker(url, "300", "light"))
            weight = "300";
        else if (contains_weight_marker(url, "500", "medium"))
            weight = "500";
        else if (contains_weight_marker(url, "900", "black"))
            weight = "900";
        else if (contains_weight_marker(url, "100", "thin"))
            weight = "100";

        std::string style = "normal";
        if (contains_weight_marker(url, "italic", "oblique")) style = "italic";

        if (url.compare(0, 5, "data:") != 0) {
            std::string fontFace = "@font-face { font-family: '" + primaryName +
                                   "'; font-weight: " + weight + "; font-style: " + style +
                                   "; src: url('" + url + "'); }";
            bool added = m_context.addCss(fontFace, CssChangeKind::GeneratedFontFace);
            m_context.noteGeneratedFontFace(added);
            if (added) {
                m_context.fontManager.scanFontFaces(fontFace);
            }
        }

    } else if (type == ResourceType::Image) {
        m_context.loadImageFromData(url.c_str(), data, size, url.c_str());
    } else if (type == ResourceType::Css) {
        if (looks_like_font_url(url)) {
            // This is actually a font file that was requested as CSS (likely due to <link
            // rel="stylesheet">)
            this->add(url, data, size, ResourceType::Font);
            return;
        }
        std::string css((const char*)data, size);
        if (m_context.addCss(css, CssChangeKind::ExternalResource)) {
            m_context.fontManager.scanFontFaces(css);
        }
    }
}

bool ResourceManager::has(const std::string& url) const { return m_resolvedUrls.count(url) > 0; }

void ResourceManager::clear() {
    m_requests.clear();
    m_requestedUrls.clear();
    m_resolvedUrls.clear();
    m_urlToNames.clear();
}

void ResourceManager::clear(ResourceType type) {
    // Clear pending requests of this type
    for (auto it = m_requests.begin(); it != m_requests.end();) {
        if (it->type == type) {
            m_requestedUrls.erase(it->url);
            it = m_requests.erase(it);
        } else {
            ++it;
        }
    }

    // Clear resolved URLs of this type
    for (auto it = m_resolvedUrls.begin(); it != m_resolvedUrls.end();) {
        if (it->second == type) {
            m_urlToNames.erase(it->first);
            it = m_resolvedUrls.erase(it);
        } else {
            ++it;
        }
    }
}
