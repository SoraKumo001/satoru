#include "utils.h"
#include <algorithm>

std::string clean_font_name(const char* name) {
    if (!name) return "";
    std::string s = name;
    std::string result;
    for(char c : s) {
        if (c != '\'' && c != '"' && c != ' ' && c != '\\') {
            result += c;
        }
    }
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}