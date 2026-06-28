// Tests for el_svg — SVG element handling in litehtml.
//
// The el_svg class (el_svg.h / el_svg.cpp) requires litehtml::document and
// Skia infrastructure to instantiate, so we test its core logic in two ways:
//
//   1. **Extracted pure functions** — color_to_string() and
//      replace_current_color() were extracted from draw() into free functions
//      in el_svg.h / el_svg.cpp.  These are tested directly.
//
//      Source: el_svg.h (litehtml::color_to_string, litehtml::replace_current_color)
//
//   2. **Isomorphic helpers** — for get_content_size() and the attribute
//      portion of reconstruct_xml() / write_element().  These helpers
//      replicate the exact algorithm from el_svg.cpp.  Keep them in sync
//      when the production code changes.
//
//      Source: el_svg.cpp lines 81-115 (get_content_size),
//              lines 134-196 (write_element),
//              lines 198-254 (reconstruct_xml)

#include <gtest/gtest.h>
#include <sstream>
#include <map>
#include <string>
#include "core/el_svg.h"
#include "core/svg_tag_utils.h"
#include "litehtml/web_color.h"

using namespace litehtml;

// =====================================================================
// Extracted pure functions — tested directly from el_svg.h / el_svg.cpp
// =====================================================================

// ───────── color_to_string ─────────

TEST(ElSvgColorTest, OpaqueBlackReturnsHex) {
    web_color c(0, 0, 0);
    EXPECT_EQ(color_to_string(c), "#000000");
}

TEST(ElSvgColorTest, OpaqueWhiteReturnsHex) {
    web_color c(255, 255, 255);
    EXPECT_EQ(color_to_string(c), "#FFFFFF");
}

TEST(ElSvgColorTest, OpaqueRedReturnsHex) {
    web_color c(255, 0, 0);
    EXPECT_EQ(color_to_string(c), "#FF0000");
}

TEST(ElSvgColorTest, OpaqueGreenReturnsHex) {
    web_color c(0, 128, 0);
    EXPECT_EQ(color_to_string(c), "#008000");
}

TEST(ElSvgColorTest, OpaqueBlueReturnsHex) {
    web_color c(0, 0, 255);
    EXPECT_EQ(color_to_string(c), "#0000FF");
}

TEST(ElSvgColorTest, SemiTransparentReturnsRgba) {
    web_color c(255, 0, 0, 128);
    std::string s = color_to_string(c);
    EXPECT_EQ(s, "rgba(255,0,0,0.501961)");
}

TEST(ElSvgColorTest, FullyTransparentReturnsRgba) {
    web_color c(0, 0, 0, 0);
    std::string s = color_to_string(c);
    EXPECT_EQ(s, "rgba(0,0,0,0)");
}

TEST(ElSvgColorTest, LowAlphaReturnsRgba) {
    web_color c(100, 200, 50, 1);
    std::string s = color_to_string(c);
    EXPECT_EQ(s, "rgba(100,200,50,0.00392157)");
}

TEST(ElSvgColorTest, MaxAlphaOpaqueReturnsHex) {
    web_color c(10, 20, 30, 255);
    EXPECT_EQ(color_to_string(c), "#0A141E");
}

// ───────── replace_current_color ─────────

TEST(ElSvgReplaceCurrentColorTest, NoCurrentColor_ReturnsUnchanged) {
    std::string xml = "<svg fill=\"red\"><rect/></svg>";
    web_color color(255, 0, 0);
    EXPECT_EQ(replace_current_color(xml, color), xml);
}

TEST(ElSvgReplaceCurrentColorTest, SingleCurrentColor_Replaced) {
    std::string xml = "<rect fill=\"currentColor\"/>";
    web_color color(255, 0, 0);
    EXPECT_EQ(replace_current_color(xml, color), "<rect fill=\"#FF0000\"/>");
}

TEST(ElSvgReplaceCurrentColorTest, MultipleCurrentColor_AllReplaced) {
    std::string xml = "<rect fill=\"currentColor\" stroke=\"currentColor\"/>";
    web_color color(0, 128, 0);
    EXPECT_EQ(replace_current_color(xml, color),
              "<rect fill=\"#008000\" stroke=\"#008000\"/>");
}

TEST(ElSvgReplaceCurrentColorTest, CurrentColorWithAlpha_ReplacedWithRgba) {
    std::string xml = "<rect fill=\"currentColor\"/>";
    web_color color(255, 0, 0, 128);
    std::string result = replace_current_color(xml, color);
    EXPECT_EQ(result, "<rect fill=\"rgba(255,0,0,0.501961)\"/>");
}

TEST(ElSvgReplaceCurrentColorTest, CurrentColorInAttributeValueMiddle) {
    // e.g. url(#foo) currentColor
    std::string xml = "<rect stroke=\"url(#g) currentColor\"/>";
    web_color color(0, 0, 255);
    EXPECT_EQ(replace_current_color(xml, color),
              "<rect stroke=\"url(#g) #0000FF\"/>");
}

TEST(ElSvgReplaceCurrentColorTest, EmptyXml) {
    EXPECT_EQ(replace_current_color("", web_color(255, 0, 0)), "");
}

TEST(ElSvgReplaceCurrentColorTest, CurrentColorOnly_Replaced) {
    EXPECT_EQ(replace_current_color("currentColor", web_color(0, 0, 0)), "#000000");
}

// =====================================================================
// Isomorphic helpers — replicate the core algorithm from el_svg.cpp
//
// These helpers are standalone versions of the internal logic in
// el_svg::get_content_size(), el_svg::write_element(), and
// el_svg::reconstruct_xml().  They operate on plain string_map /
// attribute pairs instead of on el_svg member data, making them
// testable without a full litehtml::document.
//
// KEEP IN SYNC with el_svg.cpp when the production logic changes.
// =====================================================================

// ------------------------------------------------------------------
// Helper: parse_svg_content_size
// Replicates el_svg::get_content_size (el_svg.cpp lines 81-115)
//
// Simulates the priority: css_size → attr_size → viewBox → default(100,100)
// ------------------------------------------------------------------
struct content_size_result {
    float width;
    float height;
};

static content_size_result parse_svg_content_size(
    float css_width, float css_height,
    const char* attr_width, const char* attr_height,
    const char* attr_viewbox)
{
    float sz_w = css_width;
    float sz_h = css_height;

    if (sz_w == 0 || sz_h == 0) {
        if (attr_width)  sz_w = (float)atof(attr_width);
        if (attr_height) sz_h = (float)atof(attr_height);

        if (sz_w == 0 || sz_h == 0) {
            if (attr_viewbox) {
                float vx, vy, vw, vh;
                if (sscanf(attr_viewbox, "%f %f %f %f", &vx, &vy, &vw, &vh) == 4 ||
                    sscanf(attr_viewbox, "%f,%f,%f,%f", &vx, &vy, &vw, &vh) == 4) {
                    if (vw > 0 && vh > 0) {
                        if (sz_w == 0 && sz_h == 0) {
                            sz_w = vw;
                            sz_h = vh;
                        } else if (sz_w == 0) {
                            sz_w = sz_h * vw / vh;
                        } else if (sz_h == 0) {
                            sz_h = sz_w * vh / vw;
                        }
                    }
                }
            }
        }
    }

    if (sz_w == 0) sz_w = 100;
    if (sz_h == 0) sz_h = 100;

    return {sz_w, sz_h};
}

// =====================================================================
// Tests: get_content_size logic
// =====================================================================

TEST(ElSvgContentSizeTest, CssSizeUsedWhenNonZero) {
    auto r = parse_svg_content_size(200, 150, nullptr, nullptr, nullptr);
    EXPECT_EQ(r.width, 200);
    EXPECT_EQ(r.height, 150);
}

TEST(ElSvgContentSizeTest, CssWidthZero_FallsBackToAttrWidth) {
    auto r = parse_svg_content_size(0, 150, "300", nullptr, nullptr);
    EXPECT_EQ(r.width, 300);
    EXPECT_EQ(r.height, 150);
}

TEST(ElSvgContentSizeTest, AttrWidthHeightUsedWhenCssZero) {
    auto r = parse_svg_content_size(0, 0, "400", "300", nullptr);
    EXPECT_EQ(r.width, 400);
    EXPECT_EQ(r.height, 300);
}

TEST(ElSvgContentSizeTest, AttrViewBoxUsedWhenBothZero) {
    auto r = parse_svg_content_size(0, 0, nullptr, nullptr, "0 0 100 200");
    EXPECT_EQ(r.width, 100);
    EXPECT_EQ(r.height, 200);
}

TEST(ElSvgContentSizeTest, ViewBoxWithCommaSeparator) {
    auto r = parse_svg_content_size(0, 0, nullptr, nullptr, "0,0,100,200");
    EXPECT_EQ(r.width, 100);
    EXPECT_EQ(r.height, 200);
}

TEST(ElSvgContentSizeTest, ViewBoxWidthUsedWhenCssWidthZeroButHeightSet) {
    // css_h=150, viewBox gives ratio → w = 150 * 100 / 200 = 75
    auto r = parse_svg_content_size(0, 150, nullptr, nullptr, "0 0 100 200");
    EXPECT_EQ(r.width, 75);
    EXPECT_EQ(r.height, 150);
}

TEST(ElSvgContentSizeTest, ViewBoxHeightUsedWhenCssHeightZeroButWidthSet) {
    // css_w=200, viewBox gives ratio → h = 200 * 200 / 100 = 400
    auto r = parse_svg_content_size(200, 0, nullptr, nullptr, "0 0 100 200");
    EXPECT_EQ(r.width, 200);
    EXPECT_EQ(r.height, 400);
}

TEST(ElSvgContentSizeTest, DefaultFallbackWhenAllZero) {
    auto r = parse_svg_content_size(0, 0, nullptr, nullptr, nullptr);
    EXPECT_EQ(r.width, 100);
    EXPECT_EQ(r.height, 100);
}

TEST(ElSvgContentSizeTest, DefaultFallbackWhenViewBoxInvalid) {
    auto r = parse_svg_content_size(0, 0, nullptr, nullptr, "not a viewbox");
    EXPECT_EQ(r.width, 100);
    EXPECT_EQ(r.height, 100);
}

TEST(ElSvgContentSizeTest, DefaultFallbackWhenViewBoxHasNegativeDimensions) {
    // vw=-100, vh=-200 → not > 0, so skipped
    auto r = parse_svg_content_size(0, 0, nullptr, nullptr, "0 0 -100 -200");
    EXPECT_EQ(r.width, 100);
    EXPECT_EQ(r.height, 100);
}

TEST(ElSvgContentSizeTest, DefaultFallbackWhenViewBoxHasZeroDimensions) {
    auto r = parse_svg_content_size(0, 0, nullptr, nullptr, "0 0 0 100");
    EXPECT_EQ(r.width, 100);
    EXPECT_EQ(r.height, 100);
}

TEST(ElSvgContentSizeTest, ViewBoxWithNonZeroOrigin) {
    auto r = parse_svg_content_size(0, 0, nullptr, nullptr, "10 20 300 400");
    EXPECT_EQ(r.width, 300);
    EXPECT_EQ(r.height, 400);
}

TEST(ElSvgContentSizeTest, AttrWidthPresent_ViewBoxScalesHeight) {
    // attr width=500, attr height=null → viewBox ratio used for height
    // sz_w=500, sz_h=0 → sz_h = 500 * 200 / 100 = 1000
    auto r = parse_svg_content_size(0, 0, "500", nullptr, "0 0 100 200");
    EXPECT_EQ(r.width, 500);
    EXPECT_EQ(r.height, 1000);
}

TEST(ElSvgContentSizeTest, AttrHeightPresent_ViewBoxScalesWidth) {
    // attr height=300, attr width=null → viewBox ratio used for width
    // sz_h=300, sz_w=0 → sz_w = 300 * 100 / 200 = 150
    auto r = parse_svg_content_size(0, 0, nullptr, "300", "0 0 100 200");
    EXPECT_EQ(r.width, 150);
    EXPECT_EQ(r.height, 300);
}

TEST(ElSvgContentSizeTest, CssPartiallyZero_AttrOverridesBoth) {
    // NOTE: Production behavior — when EITHER css dimension is 0, both
    // dimensions are re-read from attributes, so the non-zero css width (300)
    // gets overwritten by attr_width "500".
    auto r = parse_svg_content_size(300, 0, "500", "400", nullptr);
    EXPECT_EQ(r.width, 500);
    EXPECT_EQ(r.height, 400);
}

TEST(ElSvgContentSizeTest, CssBothNonZero_AttrNotUsed) {
    auto r = parse_svg_content_size(200, 150, "500", "400", nullptr);
    EXPECT_EQ(r.width, 200);
    EXPECT_EQ(r.height, 150);
}

// =====================================================================
// Helper: svg_reconstruct_attrs
// Replicates the attribute-output portion of
// el_svg::reconstruct_xml (el_svg.cpp lines 198-254).
//
// Builds the opening <svg ...> tag content (excluding children)
// from a set of attributes, mirroring the exact production logic.
// ------------------------------------------------------------------
// Notes from el_svg.cpp:
//   - viewBox is passed through with original casing
//   - xmlns / xmlns:xlink are removed (already on root)
//   - x, y, width, height are removed (already on root)
//   - stroke="none" is auto-completed when stroke-width is "0" or "0px"
//     and no stroke attribute exists
//   - href / xlink:href are written as xlink:href=
//   - newlines, tabs, carriage returns are stripped from attribute values
// ------------------------------------------------------------------
static std::string svg_reconstruct_attrs(
    int x, int y, int width, int height,
    const std::map<std::string, std::string>& attrs,
    const std::string& inner_content = "")
{
    std::stringstream ss;
    ss << "<svg x=\"" << x << "\" y=\"" << y << "\" width=\"" << width
       << "\" height=\"" << height
       << "\" xmlns=\"http://www.w3.org/2000/svg\""
       << " xmlns:xlink=\"http://www.w3.org/1999/xlink\"";

    // viewBox pass-through (before main loop to keep original position)
    for (auto const& attr : attrs) {
        std::string attr_name = map_attr(attr.first);
        if (attr_name == "viewBox") {
            ss << " " << attr_name << "=\"" << attr.second << "\"";
        }
    }

    // Check stroke-width == 0
    bool stroke_width_is_zero = false;
    for (auto const& attr : attrs) {
        if (attr.first == "stroke-width" &&
            (attr.second == "0" || attr.second == "0px")) {
            stroke_width_is_zero = true;
            break;
        }
    }

    // Main attribute loop
    bool stroke_attr_written = false;
    for (auto const& attr : attrs) {
        std::string attr_name = map_attr(attr.first);
        std::string attr_val = attr.second;

        // Strip whitespace from values
        attr_val.erase(std::remove(attr_val.begin(), attr_val.end(), '\n'), attr_val.end());
        attr_val.erase(std::remove(attr_val.begin(), attr_val.end(), '\r'), attr_val.end());
        attr_val.erase(std::remove(attr_val.begin(), attr_val.end(), '\t'), attr_val.end());

        // Skip attributes that are already on the root
        if (attr_name == "xmlns" || attr_name == "xmlns:xlink" ||
            attr_name == "x" || attr_name == "y" ||
            attr_name == "width" || attr_name == "height" ||
            attr_name == "viewBox")
            continue;

        if (attr_name == "stroke") {
            if (stroke_width_is_zero) {
                ss << " stroke=\"none\"";
            } else {
                ss << " stroke=\"" << attr_val << "\"";
            }
            stroke_attr_written = true;
        } else if (attr_name == "href" || attr_name == "xlink:href") {
            ss << " xlink:href=\"" << attr_val << "\"";
        } else {
            ss << " " << attr_name << "=\"" << attr_val << "\"";
        }
    }

    // Auto-complete stroke="none" when stroke-width is zero but no stroke attr
    if (stroke_width_is_zero && !stroke_attr_written) {
        ss << " stroke=\"none\"";
    }

    if (inner_content.empty()) {
        ss << "/>";
    } else {
        ss << ">" << inner_content << "</svg>";
    }

    return ss.str();
}

// =====================================================================
// Tests: reconstruct_xml attribute handling
// =====================================================================

TEST(ElSvgReconstructTest, BasicAttributes) {
    std::map<std::string, std::string> attrs = {
        {"fill", "red"},
        {"stroke", "black"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 200, attrs);
    EXPECT_NE(result.find("fill=\"red\""), std::string::npos);
    EXPECT_NE(result.find("stroke=\"black\""), std::string::npos);
    EXPECT_NE(result.find("x=\"0\""), std::string::npos);
    EXPECT_NE(result.find("y=\"0\""), std::string::npos);
    EXPECT_NE(result.find("width=\"100\""), std::string::npos);
    EXPECT_NE(result.find("height=\"200\""), std::string::npos);
    EXPECT_NE(result.find("xmlns=\"http://www.w3.org/2000/svg\""), std::string::npos);
}

TEST(ElSvgReconstructTest, StrokeWidthZero_AutoCompletesStrokeNone) {
    std::map<std::string, std::string> attrs = {
        {"fill", "red"},
        {"stroke-width", "0"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    EXPECT_NE(result.find("stroke=\"none\""), std::string::npos);
}

TEST(ElSvgReconstructTest, StrokeWidthZeroPx_AutoCompletesStrokeNone) {
    std::map<std::string, std::string> attrs = {
        {"stroke-width", "0px"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    EXPECT_NE(result.find("stroke=\"none\""), std::string::npos);
}

TEST(ElSvgReconstructTest, StrokeWidthZero_OverridesExistingStroke) {
    // NOTE: Production behavior — when stroke-width=0, stroke is forced to "none",
    // even if a stroke attribute exists. This ensures the SVG renders without strokes.
    std::map<std::string, std::string> attrs = {
        {"stroke", "blue"},
        {"stroke-width", "0"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    EXPECT_NE(result.find("stroke=\"none\""), std::string::npos);
    EXPECT_EQ(result.find("stroke=\"blue\""), std::string::npos);
}

TEST(ElSvgReconstructTest, StrokeWidthNonZero_StrokePassedThrough) {
    std::map<std::string, std::string> attrs = {
        {"stroke", "green"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    EXPECT_NE(result.find("stroke=\"green\""), std::string::npos);
}

TEST(ElSvgReconstructTest, XlinkHrefWrittenAsXlinkHref) {
    std::map<std::string, std::string> attrs = {
        {"href", "http://example.com/image.svg"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    EXPECT_NE(result.find("xlink:href=\"http://example.com/image.svg\""),
              std::string::npos);
}

TEST(ElSvgReconstructTest, XlinkHrefRawAttrPassedThrough) {
    std::map<std::string, std::string> attrs = {
        {"xlink:href", "http://example.com/img.svg"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    EXPECT_NE(result.find("xlink:href=\"http://example.com/img.svg\""),
              std::string::npos);
}

TEST(ElSvgReconstructTest, ViewBoxPreservesCapitalization) {
    std::map<std::string, std::string> attrs = {
        {"viewBox", "0 0 200 300"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    EXPECT_NE(result.find("viewBox=\"0 0 200 300\""), std::string::npos);
    // Should NOT be lowercased
    EXPECT_EQ(result.find("viewbox=\""), std::string::npos);
}

TEST(ElSvgReconstructTest, ViewBoxLowercaseInput_MappedToViewBox) {
    // map_attr("viewbox") → "viewBox"
    std::map<std::string, std::string> attrs = {
        {"viewbox", "10 20 30 40"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    EXPECT_NE(result.find("viewBox=\"10 20 30 40\""), std::string::npos);
}

TEST(ElSvgReconstructTest, XxmlnsRemoved) {
    std::map<std::string, std::string> attrs = {
        {"fill", "red"},
        {"xmlns", "http://example.com/"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    EXPECT_EQ(result.find("xmlns=\""), result.find("xmlns=\"http://www.w3.org/2000/svg\""));
    // Only one xmlns should be present (the root one)
    EXPECT_EQ(result.rfind("xmlns=\""), result.find("xmlns=\""));
}

TEST(ElSvgReconstructTest, XxmlnsXlinkRemoved) {
    std::map<std::string, std::string> attrs = {
        {"xmlns:xlink", "http://example.com/"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    // Only the root xmlns:xlink should be present
    size_t first = result.find("xmlns:xlink");
    size_t last = result.rfind("xmlns:xlink");
    EXPECT_EQ(first, last);
}

TEST(ElSvgReconstructTest, XAndYRemovedFromAttrs) {
    // x, y, width, height are already on the root, should NOT be duplicated
    std::map<std::string, std::string> attrs = {
        {"x", "50"},
        {"y", "60"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    // The root element has x="0" y="0" width="100" height="100"
    // The attrs should NOT add extra x="50" y="60"
    // Count occurrences of 'x="'
    int x_count = 0;
    size_t pos = 0;
    while ((pos = result.find("x=\"", pos)) != std::string::npos) {
        x_count++;
        pos++;
    }
    // Should have exactly one: from the root <svg x="0"
    EXPECT_EQ(x_count, 1) << "x attribute should appear exactly once on root element";
}

TEST(ElSvgReconstructTest, NewlinesAndTabsStrippedFromValues) {
    std::map<std::string, std::string> attrs = {
        {"d", "M10\nL20\r\n\tZ"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    // The reconstructed value should have whitespace stripped
    EXPECT_NE(result.find("d=\"M10L20Z\""), std::string::npos);
}

TEST(ElSvgReconstructTest, AllSpecialCharsInValue) {
    std::map<std::string, std::string> attrs = {
        {"data-foo", "line1\nline2\r\n\tend"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    EXPECT_NE(result.find("data-foo=\"line1line2end\""), std::string::npos);
}

TEST(ElSvgReconstructTest, SelfClosingWhenNoChildren) {
    std::map<std::string, std::string> attrs = {
        {"fill", "red"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    EXPECT_NE(result.find("/>"), std::string::npos);
    EXPECT_EQ(result.find("></svg>"), std::string::npos);
}

TEST(ElSvgReconstructTest, NonSelfClosingWhenChildrenPresent) {
    std::map<std::string, std::string> attrs = {
        {"fill", "red"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs,
                                               "<rect fill=\"blue\"/>");
    EXPECT_NE(result.find("><rect"), std::string::npos);
    EXPECT_NE(result.find("</svg>"), std::string::npos);
}

TEST(ElSvgReconstructTest, EmptyAttrs) {
    std::map<std::string, std::string> attrs;
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    // Should still produce valid root SVG tag
    EXPECT_NE(result.find("<svg x=\"0\" y=\"0\" width=\"100\" height=\"100\""),
              std::string::npos);
    EXPECT_NE(result.find("/>"), std::string::npos);
}

TEST(ElSvgReconstructTest, MultipleAttrsOrderingPreserved) {
    // The attribute output order follows the map iteration order (alphabetical for std::map)
    std::map<std::string, std::string> attrs = {
        {"cx", "50"},
        {"cy", "50"},
        {"r", "40"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    EXPECT_NE(result.find("cx=\"50\""), std::string::npos);
    EXPECT_NE(result.find("cy=\"50\""), std::string::npos);
    EXPECT_NE(result.find("r=\"40\""), std::string::npos);
}

TEST(ElSvgReconstructTest, StrokeWidthZero_OverridesUrlStroke) {
    // NOTE: Production behavior — stroke-width=0 forces stroke="none"
    // regardless of the stroke value (even url references).
    std::map<std::string, std::string> attrs = {
        {"stroke", "url(#grad)"},
        {"stroke-width", "0"}
    };
    std::string result = svg_reconstruct_attrs(0, 0, 100, 100, attrs);
    EXPECT_NE(result.find("stroke=\"none\""), std::string::npos);
    EXPECT_EQ(result.find("url(#grad)"), std::string::npos);
}

// =====================================================================
// Helper: svg_write_element_attrs
// Replicates the attribute-output portion of el_svg::write_element
// (el_svg.cpp lines 134-196).
//
// Writes element XML for a given tag name and attribute map,
// without children (self-closing).
// ------------------------------------------------------------------
static std::string svg_write_element_attrs(
    const std::string& tag_name,
    const std::map<std::string, std::string>& attrs,
    bool has_children = false)
{
    std::stringstream os;

    std::string mapped_tag = map_tag(tag_name);
    os << "<" << mapped_tag;

    bool is_image_or_use = (mapped_tag == "image" || mapped_tag == "use");

    bool stroke_width_is_zero = false;
    for (auto const& attr : attrs) {
        if (attr.first == "stroke-width" &&
            (attr.second == "0" || attr.second == "0px")) {
            stroke_width_is_zero = true;
            break;
        }
    }

    bool stroke_attr_written = false;
    for (auto const& attr : attrs) {
        std::string attr_name = map_attr(attr.first);
        std::string attr_val = attr.second;

        // Strip whitespace from values
        attr_val.erase(std::remove(attr_val.begin(), attr_val.end(), '\n'), attr_val.end());
        attr_val.erase(std::remove(attr_val.begin(), attr_val.end(), '\r'), attr_val.end());
        attr_val.erase(std::remove(attr_val.begin(), attr_val.end(), '\t'), attr_val.end());

        if (attr_name == "stroke") {
            if (stroke_width_is_zero) {
                os << " stroke=\"none\"";
            } else {
                os << " stroke=\"" << attr_val << "\"";
            }
            stroke_attr_written = true;
        } else if (is_image_or_use &&
                   (attr_name == "href" || attr_name == "xlink:href")) {
            os << " xlink:href=\"" << attr_val << "\"";
        } else if (attr_name != "xmlns" && attr_name != "xmlns:xlink") {
            os << " " << attr_name << "=\"" << attr_val << "\"";
        }
    }

    if (stroke_width_is_zero && !stroke_attr_written) {
        os << " stroke=\"none\"";
    }

    if (has_children) {
        os << "><children/></" << mapped_tag << ">";
    } else {
        os << "/>";
    }

    return os.str();
}

// =====================================================================
// Tests: write_element attribute handling
// =====================================================================

TEST(ElSvgWriteElementTest, BasicTagAndAttrs) {
    std::map<std::string, std::string> attrs = {
        {"fill", "red"},
        {"opacity", "0.5"}
    };
    std::string result = svg_write_element_attrs("rect", attrs);
    EXPECT_EQ(result, "<rect fill=\"red\" opacity=\"0.5\"/>");
}

TEST(ElSvgWriteElementTest, StrokeWidthZero_AutoStrokeNone) {
    std::map<std::string, std::string> attrs = {
        {"fill", "blue"},
        {"stroke-width", "0"}
    };
    std::string result = svg_write_element_attrs("rect", attrs);
    EXPECT_EQ(result, "<rect fill=\"blue\" stroke-width=\"0\" stroke=\"none\"/>");
}

TEST(ElSvgWriteElementTest, ImageTag_HrefBecomesXlinkHref) {
    std::map<std::string, std::string> attrs = {
        {"href", "img.svg"},
        {"width", "100"}
    };
    std::string result = svg_write_element_attrs("image", attrs);
    EXPECT_NE(result.find("xlink:href=\"img.svg\""), std::string::npos);
    // "href=" appears inside " xlink:href=" — verify there's no bare href
    // by checking that the only "href" mention is preceded by "xlink:"
    size_t href_pos = result.find("href=");
    EXPECT_NE(href_pos, std::string::npos);
    EXPECT_GT(href_pos, 7u);
    EXPECT_EQ(result.substr(href_pos - 7, 7), " xlink:");
}

TEST(ElSvgWriteElementTest, UseTag_HrefBecomesXlinkHref) {
    std::map<std::string, std::string> attrs = {
        {"href", "#myShape"}
    };
    std::string result = svg_write_element_attrs("use", attrs);
    EXPECT_EQ(result, "<use xlink:href=\"#myShape\"/>");
}

TEST(ElSvgWriteElementTest, NonImageUseTag_HrefStaysAsIs) {
    // For non-image/use tags, href should be written as-is
    std::map<std::string, std::string> attrs = {
        {"href", "#something"}
    };
    std::string result = svg_write_element_attrs("a", attrs);
    // The helper's xmlns filtering doesn't remove href for non-image tags
    EXPECT_NE(result.find("href=\"#something\""), std::string::npos);
}

TEST(ElSvgWriteElementTest, XxmlnsFiltered) {
    std::map<std::string, std::string> attrs = {
        {"fill", "red"},
        {"xmlns", "http://example.com/"}
    };
    std::string result = svg_write_element_attrs("rect", attrs);
    EXPECT_EQ(result, "<rect fill=\"red\"/>");
}

TEST(ElSvgWriteElementTest, TagNameCaseMapping) {
    // "clippath" should be mapped to "clipPath"
    std::map<std::string, std::string> attrs = {
        {"fill", "red"}
    };
    std::string result = svg_write_element_attrs("clippath", attrs);
    EXPECT_EQ(result, "<clipPath fill=\"red\"/>");
}

TEST(ElSvgWriteElementTest, UnknownTagNamePassesThrough) {
    std::map<std::string, std::string> attrs;
    std::string result = svg_write_element_attrs("customEl", attrs);
    EXPECT_EQ(result, "<customEl/>");
}

TEST(ElSvgWriteElementTest, AttrNameCaseMapping) {
    // "viewbox" should be mapped to "viewBox"
    std::map<std::string, std::string> attrs = {
        {"viewbox", "0 0 100 100"}
    };
    std::string result = svg_write_element_attrs("svg", attrs);
    EXPECT_NE(result.find("viewBox=\"0 0 100 100\""), std::string::npos);
}

TEST(ElSvgWriteElementTest, WhitespaceInValuesStripped) {
    std::map<std::string, std::string> attrs = {
        {"d", "M10\nL20\r\n\tZ"}
    };
    std::string result = svg_write_element_attrs("path", attrs);
    EXPECT_EQ(result, "<path d=\"M10L20Z\"/>");
}

TEST(ElSvgWriteElementTest, EmptyAttrs) {
    std::map<std::string, std::string> attrs;
    std::string result = svg_write_element_attrs("g", attrs);
    EXPECT_EQ(result, "<g/>");
}

TEST(ElSvgWriteElementTest, HasChildren_NonSelfClosing) {
    std::map<std::string, std::string> attrs = {
        {"fill", "red"}
    };
    std::string result = svg_write_element_attrs("g", attrs, true);
    EXPECT_EQ(result, "<g fill=\"red\"><children/></g>");
}

TEST(ElSvgWriteElementTest, FilterOrder_XmlnsLast) {
    // xmlns should be removed; other attrs should appear in order
    std::map<std::string, std::string> attrs = {
        {"fill", "red"},
        {"xmlns", "http://example.com/"},
        {"stroke", "blue"}
    };
    std::string result = svg_write_element_attrs("rect", attrs);
    EXPECT_EQ(result, "<rect fill=\"red\" stroke=\"blue\"/>");
}

// =====================================================================
// Edge cases for color_to_string
// =====================================================================

TEST(ElSvgColorTest, ColorWithAlpha127) {
    web_color c(128, 64, 32, 127);
    std::string s = color_to_string(c);
    EXPECT_EQ(s, "rgba(128,64,32,0.498039)");
}

TEST(ElSvgColorTest, ColorWithAlpha1) {
    web_color c(255, 255, 255, 1);
    std::string s = color_to_string(c);
    EXPECT_EQ(s, "rgba(255,255,255,0.00392157)");
}

// =====================================================================
// Edge cases for replace_current_color
// =====================================================================

TEST(ElSvgReplaceCurrentColorTest, AdjacentCurrentColors_AllReplaced) {
    std::string xml = "currentColor currentColor";
    web_color color(0, 255, 0);
    EXPECT_EQ(replace_current_color(xml, color), "#00FF00 #00FF00");
}

TEST(ElSvgReplaceCurrentColorTest, CurrentColorSubstring_IsReplaced) {
    // Note: replace_current_color does substring matching, so "currentColors"
    // will have "currentColor" (first 12 chars) replaced, leaving trailing "s".
    // This matches the production behavior in el_svg.cpp and Skia's handling.
    std::string xml = "currentColors";
    web_color color(255, 0, 0);
    std::string result = replace_current_color(xml, color);
    EXPECT_EQ(result, "#FF0000s");
    EXPECT_NE(result, "currentColors");
}

TEST(ElSvgReplaceCurrentColorTest, VeryLongString) {
    std::string xml(10000, 'x');
    xml += "currentColor";
    web_color color(0, 0, 0);
    std::string result = replace_current_color(xml, color);
    EXPECT_EQ(result.size(), xml.size() - 12 + 7);  // "currentColor"(12) → "#000000"(7)
    EXPECT_EQ(result.substr(0, 10000), std::string(10000, 'x'));
    EXPECT_EQ(result.substr(10000), "#000000");
}

// =====================================================================
// Combined: reconstruct_xml with currentColor replacement flow
// =====================================================================

TEST(ElSvgCombinedTest, ReconstructThenReplaceCurrentColor) {
    // Simulate the full flow: reconstruct_xml → replace_current_color
    std::map<std::string, std::string> attrs = {
        {"fill", "currentColor"},
        {"stroke", "currentColor"}
    };
    std::string xml = svg_reconstruct_attrs(0, 0, 50, 50, attrs);
    web_color color(0, 100, 200);
    std::string result = replace_current_color(xml, color);

    EXPECT_NE(result.find("fill=\"#0064C8\""), std::string::npos);
    EXPECT_NE(result.find("stroke=\"#0064C8\""), std::string::npos);
    EXPECT_EQ(result.find("currentColor"), std::string::npos);
}
