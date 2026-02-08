#include "resource_manager.h"

#include <iostream>
#include <regex>

#include "satoru_context.h"
#include "container_skia.h"

extern container_skia *g_discovery_container;

ResourceManager::ResourceManager(SatoruContext& context) : m_context(context) {}

void ResourceManager::request(const std::string& url, const std::string& name, ResourceType type) {
    if (url.empty()) return;
    if (m_resolvedUrls.count(url)) return;  // Already resolved

    m_requests.insert({url, name, type});

    // Track name association
    if (!name.empty()) {
        m_urlToNames[url].insert(name);
    }
}

std::vector<ResourceRequest> ResourceManager::getPendingRequests() {
    std::vector<ResourceRequest> result(m_requests.begin(), m_requests.end());
    m_requests.clear();
    return result;
}

void ResourceManager::add(const std::string& url, const uint8_t* data, size_t size,
                          ResourceType type) {
    if (url.empty()) return;

    m_resolvedUrls.insert(url);

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
        if (std::regex_search(url, std::regex("[-._]700\\b|bold", std::regex::icase))) weight = "700";
        else if (std::regex_search(url, std::regex("[-._]300\\b|light", std::regex::icase))) weight = "300";
        else if (std::regex_search(url, std::regex("[-._]500\\b|medium", std::regex::icase))) weight = "500";
        else if (std::regex_search(url, std::regex("[-._]900\\b|black", std::regex::icase))) weight = "900";

        std::string style = "normal";
        if (std::regex_search(url, std::regex("italic|oblique", std::regex::icase))) style = "italic";

        std::string fontFace = "@font-face { font-family: '" + primaryName + "'; font-weight: " + weight + 
                               "; font-style: " + style + "; src: url('" + url + "'); }";
        m_context.addCss(fontFace);
        if (g_discovery_container) g_discovery_container->scan_font_faces(fontFace);

    } else if (type == ResourceType::Image) {
        m_context.loadImageFromData(url.c_str(), data, size);
    } else if (type == ResourceType::Css) {
        std::string css((const char*)data, size);
        m_context.addCss(css);
        if (g_discovery_container) g_discovery_container->scan_font_faces(css);
    }
}

bool ResourceManager::has(const std::string& url) const { return m_resolvedUrls.count(url) > 0; }