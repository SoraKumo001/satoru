// Stub definitions for web_color symbols normally in web_color.cpp
// This lets us test web_color.h without compiling the full web_color.cpp
// (which drags in the CSS parser dependency chain).
#include "litehtml/web_color.h"
#include <algorithm>

namespace litehtml {

const web_color web_color::transparent   = web_color(0, 0, 0, 0);
const web_color web_color::black         = web_color(0, 0, 0, 255);
const web_color web_color::white         = web_color(255, 255, 255, 255);
const web_color web_color::current_color = web_color(true);

web_color web_color::darken(double fraction) const
{
    int v_red   = (int)red;
    int v_green = (int)green;
    int v_blue  = (int)blue;
    v_red   = (int)std::max(v_red   - (v_red   * fraction), 0.0);
    v_green = (int)std::max(v_green - (v_green * fraction), 0.0);
    v_blue  = (int)std::max(v_blue  - (v_blue  * fraction), 0.0);
    return {(byte)v_red, (byte)v_green, (byte)v_blue, alpha};
}

string web_color::to_string() const
{
    char str[9];
    if (alpha)
        snprintf(str, sizeof(str), "%02X%02X%02X%02X", red, green, blue, alpha);
    else
        snprintf(str, sizeof(str), "%02X%02X%02X", red, green, blue);
    return str;
}

} // namespace litehtml
