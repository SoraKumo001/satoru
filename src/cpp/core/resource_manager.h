#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

class SatoruContext;

enum class ResourceType : int { Raw = 0, Font = 1, Image = 2, Css = 3 };

struct ResourceRequest {
    std::string url;
    std::string name;  // Font family name, or other identifier
    ResourceType type;

    // For std::set to work
    bool operator<(const ResourceRequest& other) const {
        if (url != other.url) return url < other.url;
        if (name != other.name) return name < other.name;
        return type < other.type;
    }
};

class ResourceManager {
   public:
    ResourceManager(SatoruContext& context);

    // Register a needed resource
    void request(const std::string& url, const std::string& name, ResourceType type);

    // Get list of pending requests to send to JS
    std::vector<ResourceRequest> getPendingRequests();

    // Receive data from JS
    void add(const std::string& url, const uint8_t* data, size_t size, ResourceType type);

    bool has(const std::string& url) const;

   private:
    SatoruContext& m_context;
    std::set<ResourceRequest> m_requests;
    std::set<std::string> m_resolvedUrls;
    std::map<std::string, std::set<std::string>>
        m_urlToNames;  // Map URL to requested names (e.g. Font Families)
};

#endif  // RESOURCE_MANAGER_H