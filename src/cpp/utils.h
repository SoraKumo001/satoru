#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <cstdint>

std::string clean_font_name(const char* name);
std::string base64_encode(const uint8_t* data, size_t len);
std::vector<uint8_t> base64_decode(const std::string& in);

#endif // UTILS_H