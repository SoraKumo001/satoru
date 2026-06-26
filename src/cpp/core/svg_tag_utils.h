// Extracted SVG tag/attr mapping functions from el_svg.cpp.
// Inline functions + static maps — zero Skia dependency, fully testable.
#ifndef SATORU_SVG_TAG_UTILS_H
#define SATORU_SVG_TAG_UTILS_H

#include <string>
#include <unordered_map>

namespace litehtml {

inline const std::unordered_map<std::string, std::string>& get_svg_tag_mapping() {
    static const std::unordered_map<std::string, std::string> mapping = {
        {"clippath", "clipPath"},
        {"lineargradient", "linearGradient"},
        {"radialgradient", "radialGradient"},
        {"textpath", "textPath"},
        {"fegausianblur", "feGaussianBlur"},
        {"fecolormatrix", "feColorMatrix"},
        {"fecomponenttransfer", "feComponentTransfer"},
        {"fecomposite", "feComposite"},
        {"feconvolvematrix", "feConvolveMatrix"},
        {"fediffuselighting", "feDiffuseLighting"},
        {"fedisplacementmap", "feDisplacementMap"},
        {"fedistantlight", "feDistantLight"},
        {"fedropshadow", "feDropShadow"},
        {"feflood", "feFlood"},
        {"fefunca", "feFuncA"},
        {"fefuncb", "feFuncB"},
        {"fefuncg", "feFuncG"},
        {"fefuncr", "feFuncR"},
        {"feimage", "feImage"},
        {"femerge", "feMerge"},
        {"femergenode", "feMergeNode"},
        {"femorphology", "feMorphology"},
        {"feoffset", "feOffset"},
        {"fepointlight", "fePointLight"},
        {"fespecularlighting", "feSpecularLighting"},
        {"fespotlight", "feSpotLight"},
        {"fetile", "feTile"},
        {"feturbulence", "feTurbulence"},
        {"foreignobject", "foreignObject"},
    };
    return mapping;
}

inline const std::unordered_map<std::string, std::string>& get_svg_attr_mapping() {
    static const std::unordered_map<std::string, std::string> mapping = {
        {"viewbox", "viewBox"},
        {"preserveaspectratio", "preserveAspectRatio"},
        {"gradientunits", "gradientUnits"},
        {"gradienttransform", "gradientTransform"},
        {"patternunits", "patternUnits"},
        {"patterncontentunits", "patternContentUnits"},
        {"patterntransform", "patternTransform"},
        {"clippathunits", "clipPathUnits"},
        {"maskunits", "maskUnits"},
        {"maskcontentunits", "maskContentUnits"},
        {"spreadmethod", "spreadMethod"},
        {"startoffset", "startOffset"},
        {"pathlength", "pathLength"},
        {"lengthadjust", "lengthAdjust"},
        {"textlength", "textLength"},
        {"stddeviation", "stdDeviation"},
        {"surfacescale", "surfaceScale"},
        {"diffuseconstant", "diffuseConstant"},
        {"specularconstant", "specularConstant"},
        {"specularexponent", "specularExponent"},
        {"numoctaves", "numOctaves"},
        {"basefrequency", "baseFrequency"},
        {"stitchtiles", "stitchTiles"},
    };
    return mapping;
}

inline std::string map_tag(const std::string& tag) {
    auto& m = get_svg_tag_mapping();
    auto it = m.find(tag);
    return (it != m.end()) ? it->second : tag;
}

inline std::string map_attr(const std::string& attr) {
    auto& m = get_svg_attr_mapping();
    auto it = m.find(attr);
    return (it != m.end()) ? it->second : attr;
}

}  // namespace litehtml

#endif  // SATORU_SVG_TAG_UTILS_H
