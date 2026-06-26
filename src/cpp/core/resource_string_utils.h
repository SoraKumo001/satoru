#ifndef SATORU_RESOURCE_STRING_UTILS_H
#define SATORU_RESOURCE_STRING_UTILS_H

#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace satoru {

/**
 * Case-insensitive prefix check.
 * Returns true if needle matches s[pos..pos+len(needle)] case-insensitively.
 */
inline bool starts_with_ascii_ci(const std::string& s, size_t pos, const char* needle) {
    for (size_t i = 0; needle[i] != '\0'; ++i) {
        if (pos + i >= s.size()) return false;
        if (std::tolower((unsigned char)s[pos + i]) != std::tolower((unsigned char)needle[i])) {
            return false;
        }
    }
    return true;
}

/**
 * Binary search for a needle in a byte buffer.
 */
inline bool contains_ascii(const uint8_t* data, size_t size, const char* needle) {
    size_t needleLen = std::strlen(needle);
    if (!data || needleLen == 0 || size < needleLen) return false;

    for (size_t i = 0; i <= size - needleLen; ++i) {
        if (std::memcmp(data + i, needle, needleLen) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Case-insensitive substring search.
 * An empty needle always returns true.
 */
inline bool contains_ascii_ci(const std::string& s, const char* needle) {
    if (!needle || !*needle) return true;
    size_t needleLen = std::strlen(needle);
    if (s.size() < needleLen) return false;

    for (size_t i = 0; i <= s.size() - needleLen; ++i) {
        if (starts_with_ascii_ci(s, i, needle)) return true;
    }
    return false;
}

/**
 * Multi-needle case-insensitive search.
 * Returns true if ANY needle is found in s.
 */
inline bool contains_any_ascii_ci(const std::string& s, const char* const* needles,
                                  size_t needleCount) {
    for (size_t i = 0; i < s.size(); ++i) {
        for (size_t j = 0; j < needleCount; ++j) {
            if (starts_with_ascii_ci(s, i, needles[j])) return true;
        }
    }
    return false;
}

/**
 * Heuristic check if a URL looks like a font resource.
 * Checks against known font extensions and MIME types.
 */
inline bool looks_like_font_url(const std::string& url) {
    static constexpr const char* needles[] = {".woff2",   ".woff",    ".ttf",
                                              ".otf",     ".ttc",     "font-woff",
                                              "font-ttf", "font-otf", "application/font"};
    return contains_any_ascii_ci(url, needles, sizeof(needles) / sizeof(needles[0]));
}

/**
 * Check if URL contains either of two weight/style markers.
 */
inline bool contains_weight_marker(const std::string& url, const char* a, const char* b) {
    const char* needles[] = {a, b};
    return contains_any_ascii_ci(url, needles, 2);
}

/**
 * Replace font-family values in CSS with a single quoted name.
 *
 * Finds `font-family:` declarations and replaces their value with
 * `'<name>'`. Handles multiple font-family declarations in one CSS string.
 * Does NOT handle font-family inside @font-face where properties appear
 * on the same line — assumes each declaration ends with `;` or `}`.
 */
inline std::string replace_font_family_names(const std::string& css, const std::string& name) {
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

}  // namespace satoru

#endif  // SATORU_RESOURCE_STRING_UTILS_H
