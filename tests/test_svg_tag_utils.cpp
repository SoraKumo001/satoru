#include <gtest/gtest.h>
#include "core/svg_tag_utils.h"

using namespace litehtml;

// ──────────────────────────────────────────────
// Tests for get_svg_tag_mapping()
// ──────────────────────────────────────────────

TEST(SvgTagUtilsTest, GetSvgTagMapping_ContainsExpectedEntries) {
    const auto& m = get_svg_tag_mapping();

    EXPECT_GT(m.size(), 25);  // at least ~29 entries

    // Core SVG element mappings
    EXPECT_EQ(m.at("clippath"), "clipPath");
    EXPECT_EQ(m.at("lineargradient"), "linearGradient");
    EXPECT_EQ(m.at("radialgradient"), "radialGradient");
    EXPECT_EQ(m.at("textpath"), "textPath");
    EXPECT_EQ(m.at("foreignobject"), "foreignObject");

    // Filter element mappings
    EXPECT_EQ(m.at("fegausianblur"), "feGaussianBlur");
    EXPECT_EQ(m.at("fecolormatrix"), "feColorMatrix");
    EXPECT_EQ(m.at("fecomponenttransfer"), "feComponentTransfer");
    EXPECT_EQ(m.at("fecomposite"), "feComposite");
    EXPECT_EQ(m.at("feconvolvematrix"), "feConvolveMatrix");
    EXPECT_EQ(m.at("fediffuselighting"), "feDiffuseLighting");
    EXPECT_EQ(m.at("fedisplacementmap"), "feDisplacementMap");
    EXPECT_EQ(m.at("fedistantlight"), "feDistantLight");
    EXPECT_EQ(m.at("fedropshadow"), "feDropShadow");
    EXPECT_EQ(m.at("feflood"), "feFlood");
    EXPECT_EQ(m.at("fefunca"), "feFuncA");
    EXPECT_EQ(m.at("fefuncb"), "feFuncB");
    EXPECT_EQ(m.at("fefuncg"), "feFuncG");
    EXPECT_EQ(m.at("fefuncr"), "feFuncR");
    EXPECT_EQ(m.at("feimage"), "feImage");
    EXPECT_EQ(m.at("femerge"), "feMerge");
    EXPECT_EQ(m.at("femergenode"), "feMergeNode");
    EXPECT_EQ(m.at("femorphology"), "feMorphology");
    EXPECT_EQ(m.at("feoffset"), "feOffset");
    EXPECT_EQ(m.at("fepointlight"), "fePointLight");
    EXPECT_EQ(m.at("fespecularlighting"), "feSpecularLighting");
    EXPECT_EQ(m.at("fespotlight"), "feSpotLight");
    EXPECT_EQ(m.at("fetile"), "feTile");
    EXPECT_EQ(m.at("feturbulence"), "feTurbulence");
}

TEST(SvgTagUtilsTest, GetSvgTagMapping_UnknownTagNotPresent) {
    const auto& m = get_svg_tag_mapping();
    EXPECT_EQ(m.find("unknown"), m.end());
    EXPECT_EQ(m.find("div"), m.end());
    EXPECT_EQ(m.find("span"), m.end());
}

// ──────────────────────────────────────────────
// Tests for get_svg_attr_mapping()
// ──────────────────────────────────────────────

TEST(SvgTagUtilsTest, GetSvgAttrMapping_ContainsExpectedEntries) {
    const auto& m = get_svg_attr_mapping();

    EXPECT_GT(m.size(), 20);  // at least ~23 entries

    // Core SVG attribute mappings
    EXPECT_EQ(m.at("viewbox"), "viewBox");
    EXPECT_EQ(m.at("preserveaspectratio"), "preserveAspectRatio");
    EXPECT_EQ(m.at("gradientunits"), "gradientUnits");
    EXPECT_EQ(m.at("gradienttransform"), "gradientTransform");
    EXPECT_EQ(m.at("patternunits"), "patternUnits");
    EXPECT_EQ(m.at("patterncontentunits"), "patternContentUnits");
    EXPECT_EQ(m.at("patterntransform"), "patternTransform");
    EXPECT_EQ(m.at("clippathunits"), "clipPathUnits");
    EXPECT_EQ(m.at("maskunits"), "maskUnits");
    EXPECT_EQ(m.at("maskcontentunits"), "maskContentUnits");
    EXPECT_EQ(m.at("spreadmethod"), "spreadMethod");
    EXPECT_EQ(m.at("startoffset"), "startOffset");
    EXPECT_EQ(m.at("pathlength"), "pathLength");
    EXPECT_EQ(m.at("lengthadjust"), "lengthAdjust");
    EXPECT_EQ(m.at("textlength"), "textLength");
    EXPECT_EQ(m.at("stddeviation"), "stdDeviation");
    EXPECT_EQ(m.at("surfacescale"), "surfaceScale");
    EXPECT_EQ(m.at("diffuseconstant"), "diffuseConstant");
    EXPECT_EQ(m.at("specularconstant"), "specularConstant");
    EXPECT_EQ(m.at("specularexponent"), "specularExponent");
    EXPECT_EQ(m.at("numoctaves"), "numOctaves");
    EXPECT_EQ(m.at("basefrequency"), "baseFrequency");
    EXPECT_EQ(m.at("stitchtiles"), "stitchTiles");
}

TEST(SvgTagUtilsTest, GetSvgAttrMapping_UnknownAttrNotPresent) {
    const auto& m = get_svg_attr_mapping();
    EXPECT_EQ(m.find("color"), m.end());
    EXPECT_EQ(m.find("width"), m.end());
    EXPECT_EQ(m.find("height"), m.end());
}

// ──────────────────────────────────────────────
// Tests for map_tag()
// ──────────────────────────────────────────────

TEST(SvgTagUtilsTest, MapTag_KnownTagsMappedToCorrectOutput) {
    EXPECT_EQ(map_tag("clippath"), "clipPath");
    EXPECT_EQ(map_tag("lineargradient"), "linearGradient");
    EXPECT_EQ(map_tag("radialgradient"), "radialGradient");
    EXPECT_EQ(map_tag("textpath"), "textPath");
    EXPECT_EQ(map_tag("foreignobject"), "foreignObject");
    EXPECT_EQ(map_tag("fegausianblur"), "feGaussianBlur");
    EXPECT_EQ(map_tag("fedropshadow"), "feDropShadow");
}

TEST(SvgTagUtilsTest, MapTag_UnknownTagReturnsItself) {
    EXPECT_EQ(map_tag("g"), "g");
    EXPECT_EQ(map_tag("svg"), "svg");
    EXPECT_EQ(map_tag("rect"), "rect");
    EXPECT_EQ(map_tag("circle"), "circle");
    EXPECT_EQ(map_tag("ellipse"), "ellipse");
    EXPECT_EQ(map_tag("line"), "line");
    EXPECT_EQ(map_tag("polygon"), "polygon");
    EXPECT_EQ(map_tag("polyline"), "polyline");
    EXPECT_EQ(map_tag("path"), "path");
    EXPECT_EQ(map_tag("text"), "text");
    EXPECT_EQ(map_tag("defs"), "defs");
    EXPECT_EQ(map_tag("use"), "use");
    EXPECT_EQ(map_tag("image"), "image");
    EXPECT_EQ(map_tag("mask"), "mask");
    EXPECT_EQ(map_tag("pattern"), "pattern");
    EXPECT_EQ(map_tag("stop"), "stop");
    EXPECT_EQ(map_tag("tspan"), "tspan");
}

TEST(SvgTagUtilsTest, MapTag_EmptyTagReturnsEmpty) {
    EXPECT_EQ(map_tag(""), "");
}

TEST(SvgTagUtilsTest, MapTag_AlreadyCamelCasePassesThrough) {
    // Tags that are already in correct camelCase should still be in the map
    // but for ones not in the map, they pass through unchanged
    EXPECT_EQ(map_tag("clipPath"), "clipPath");
    EXPECT_EQ(map_tag("linearGradient"), "linearGradient");
}

// ──────────────────────────────────────────────
// Tests for map_attr()
// ──────────────────────────────────────────────

TEST(SvgTagUtilsTest, MapAttr_KnownAttrsMappedToCorrectOutput) {
    EXPECT_EQ(map_attr("viewbox"), "viewBox");
    EXPECT_EQ(map_attr("preserveaspectratio"), "preserveAspectRatio");
    EXPECT_EQ(map_attr("gradientunits"), "gradientUnits");
    EXPECT_EQ(map_attr("stddeviation"), "stdDeviation");
    EXPECT_EQ(map_attr("basefrequency"), "baseFrequency");
    EXPECT_EQ(map_attr("stitchtiles"), "stitchTiles");
}

TEST(SvgTagUtilsTest, MapAttr_UnknownAttrReturnsItself) {
    EXPECT_EQ(map_attr("fill"), "fill");
    EXPECT_EQ(map_attr("stroke"), "stroke");
    EXPECT_EQ(map_attr("width"), "width");
    EXPECT_EQ(map_attr("height"), "height");
    EXPECT_EQ(map_attr("x"), "x");
    EXPECT_EQ(map_attr("y"), "y");
    EXPECT_EQ(map_attr("cx"), "cx");
    EXPECT_EQ(map_attr("cy"), "cy");
    EXPECT_EQ(map_attr("r"), "r");
    EXPECT_EQ(map_attr("opacity"), "opacity");
    EXPECT_EQ(map_attr("transform"), "transform");
    EXPECT_EQ(map_attr("style"), "style");
    EXPECT_EQ(map_attr("class"), "class");
    EXPECT_EQ(map_attr("id"), "id");
}

TEST(SvgTagUtilsTest, MapAttr_EmptyAttrReturnsEmpty) {
    EXPECT_EQ(map_attr(""), "");
}

TEST(SvgTagUtilsTest, MapAttr_AlreadyCamelCasePassesThrough) {
    EXPECT_EQ(map_attr("viewBox"), "viewBox");
    EXPECT_EQ(map_attr("preserveAspectRatio"), "preserveAspectRatio");
}

// ──────────────────────────────────────────────
// Edge cases and invariants
// ──────────────────────────────────────────────

TEST(SvgTagUtilsTest, MapTag_MapAttr_NotMutuallyContaminated) {
    // Tag entries should not appear in attr map and vice versa
    const auto& tag_map = get_svg_tag_mapping();
    const auto& attr_map = get_svg_attr_mapping();

    // 'viewBox' is an attr, not a tag
    EXPECT_EQ(tag_map.find("viewbox"), tag_map.end());

    // 'clipPath' is a tag, not an attr
    EXPECT_EQ(attr_map.find("clippath"), attr_map.end());
}

TEST(SvgTagUtilsTest, MapTag_CaseSensitivity) {
    // Mapping keys are lowercase only
    EXPECT_EQ(map_tag("ClipPath"), "ClipPath");  // not in map -> passes through
    EXPECT_EQ(map_tag("CLIPPATH"), "CLIPPATH");  // not in map -> passes through
}

TEST(SvgTagUtilsTest, MapAttr_CaseSensitivity) {
    // Mapping keys are lowercase only
    EXPECT_EQ(map_attr("ViewBox"), "ViewBox");      // not in map -> passes through
    EXPECT_EQ(map_attr("VIEWBOX"), "VIEWBOX");      // not in map -> passes through
}

TEST(SvgTagUtilsTest, GetSvgTagMapping_SameInstance) {
    // Verify the static is a singleton (same address)
    const auto& m1 = get_svg_tag_mapping();
    const auto& m2 = get_svg_tag_mapping();
    EXPECT_EQ(&m1, &m2);
}

TEST(SvgTagUtilsTest, GetSvgAttrMapping_SameInstance) {
    const auto& m1 = get_svg_attr_mapping();
    const auto& m2 = get_svg_attr_mapping();
    EXPECT_EQ(&m1, &m2);
}
