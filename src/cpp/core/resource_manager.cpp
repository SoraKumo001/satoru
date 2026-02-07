#include "resource_manager.h"

#include <iostream>

#include "satoru_context.h"

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
        auto it = m_urlToNames.find(url);
        if (it != m_urlToNames.end()) {
            for (const auto& name : it->second) {
                m_context.loadFont(name.c_str(), data, size);
                registered = true;
            }
        }

        // Fallback if no specific name was associated (e.g. pre-loading)
        if (!registered) {
            m_context.loadFont(url.c_str(), data, size);
        }

    } else if (type == ResourceType::Image) {
        m_context.loadImageFromData(url.c_str(), data, size);
    } else if (type == ResourceType::Css) {
        // CSS text handling to be implemented
    }
}

bool ResourceManager::has(const std::string& url) const { return m_resolvedUrls.count(url) > 0; }
