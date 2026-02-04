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

std::vector<uint8_t> base64_decode(const std::string& in) {
    std::vector<uint8_t> out;
    std::vector<int> T(256, -1);
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; i++) T[chars[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) continue;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(uint8_t((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}