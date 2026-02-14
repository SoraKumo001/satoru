#include "resource_manager.h"

#include <ctre.hpp>
#include <iostream>

#include "../utils/skia_utils.h"
#include "container_skia.h"
#include "satoru_context.h"

extern container_skia* g_discovery_container;

ResourceManager::ResourceManager(SatoruContext& context) : m_context(context) {}

void ResourceManager::request(const std::string& url, const std::string& name, ResourceType type) {
    if (url.empty()) return;
    if (m_resolvedUrls.count(url)) return;  // Already resolved

    // Track name association
    if (!name.empty()) {
        m_urlToNames[url].insert(name);
    }

    if (url.substr(0, 5) == "data:") {
        size_t commaPos = url.find(',');
        if (commaPos != std::string::npos) {
            std::string metadata = url.substr(0, commaPos);
            std::string rawData = url.substr(commaPos + 1);
            if (metadata.find(";base64") != std::string::npos) {
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

    m_requests.insert({url, name, type});
}

std::vector<ResourceRequest> ResourceManager::getPendingRequests() {
    std::vector<ResourceRequest> result(m_requests.begin(), m_requests.end());
    m_requests.clear();
    return result;
}

void ResourceManager::add(const std::string& url, const uint8_t* data, size_t size,
                          ResourceType type) {
    if (url.empty()) return;

    m_resolvedUrls[url] = type;

    if (!data || size == 0) {
        std::cerr << "ResourceManager: Received empty data for " << url << std::endl;
        return;
    }

    if (type == ResourceType::Font) {
        // Attempt to register under all requested names associated with this URL
        bool registered = false;
        std::string primaryName = "";

        auto it = m_urlToNames.find(url);
        if (it != m_urlToNames.end()) {
            for (const auto& name : it->second) {
                m_context.loadFont(name.c_str(), data, size);
                if (primaryName.empty()) primaryName = name;
                registered = true;
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
            m_context.loadFont(fontName.c_str(), data, size);
        }

        // Generate @font-face and add it to extra CSS so litehtml knows about it
        std::string weight = "400";
        if (ctre::search<"(?i)700|bold">(url))
            weight = "700";
        else if (ctre::search<"(?i)300|light">(url))
            weight = "300";
        else if (ctre::search<"(?i)500|medium">(url))
            weight = "500";
        else if (ctre::search<"(?i)900|black">(url))
            weight = "900";
        else if (ctre::search<"(?i)100|thin">(url))
            weight = "100";

        std::string style = "normal";
        if (ctre::search<"(?i)italic|oblique">(url)) style = "italic";

        std::string fontFace = "@font-face { font-family: '" + primaryName +
                               "'; font-weight: " + weight + "; font-style: " + style +
                               "; src: url('" + url + "'); }";
        m_context.addCss(fontFace);
        m_context.fontManager.scanFontFaces(fontFace);

    } else if (type == ResourceType::Image) {
        m_context.loadImageFromData(url.c_str(), data, size, url.c_str());
    } else if (type == ResourceType::Css) {
        std::string lowerUrl = url;
        std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);
        if (lowerUrl.find(".woff2") != std::string::npos ||
            lowerUrl.find(".woff") != std::string::npos ||
            lowerUrl.find(".ttf") != std::string::npos ||
            lowerUrl.find(".otf") != std::string::npos ||
            lowerUrl.find(".ttc") != std::string::npos ||
            lowerUrl.find("font-woff") != std::string::npos ||
            lowerUrl.find("font-ttf") != std::string::npos ||
            lowerUrl.find("font-otf") != std::string::npos ||
            lowerUrl.find("application/font") != std::string::npos) {
            // This is actually a font file that was requested as CSS (likely due to <link
            // rel="stylesheet">)
            this->add(url, data, size, ResourceType::Font);
            return;
        }
        std::string css((const char*)data, size);
        m_context.addCss(css);
        m_context.fontManager.scanFontFaces(css);
    }
}

bool ResourceManager::has(const std::string& url) const { return m_resolvedUrls.count(url) > 0; }

void ResourceManager::clear() {
    m_requests.clear();
    m_resolvedUrls.clear();
    m_urlToNames.clear();
}

void ResourceManager::clear(ResourceType type) {
    // Clear pending requests of this type
    for (auto it = m_requests.begin(); it != m_requests.end();) {
        if (it->type == type) {
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
