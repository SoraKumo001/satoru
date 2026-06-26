#pragma once
// Minimal SkFontArguments stub for native tests
#include <cstdint>
#include <vector>

struct SkFontArguments {
    struct VariationPosition {
        struct Coordinate {
            uint32_t axis;
            float value;
        };
        const Coordinate* axes = nullptr;
        size_t axisCount = 0;
    };

    struct Palette {
        int index = 0;
        const uint16_t* overrides = nullptr;
        uint16_t overrideCount = 0;
    };

    VariationPosition variationPosition;
    const Palette* palettes = nullptr;
    int paletteCount = 0;

    void setVariationDesignPosition(VariationPosition pos) {
        variationPosition = pos;
    }
};
