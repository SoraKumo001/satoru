#ifndef CONTAINER_SKIA_HELPERS_H
#define CONTAINER_SKIA_HELPERS_H

/// Pure-logic helpers extracted from container_skia.cpp.
///
/// These functions depend only on their arguments (no instance state),
/// making them independently testable without a full container_skia instance.
/// They use the same Skia and litehtml types as the production code.
///
/// Each function is an isomorphic copy of the corresponding inline logic
/// in container_skia.cpp — see that file for the original context.

#include <string>
#include <vector>

#include "include/core/SkBlendMode.h"
#include "include/core/SkMatrix.h"
#include "include/effects/SkColorMatrix.h"
#include "include/effects/SkImageFilters.h"
#include "libs/litehtml/include/litehtml/borders.h"
#include "libs/litehtml/include/litehtml/css_tokenizer.h"

// ────────────────────────────────────────────────────────────────────────────
// 1. Blend mode conversion
// ────────────────────────────────────────────────────────────────────────────

/// Convert litehtml::blend_mode to SkBlendMode.
/// Isomorphic copy of container_skia::to_skia_blend_mode (L1897-1932).
SkBlendMode to_skia_blend_mode(litehtml::blend_mode bm);

// ────────────────────────────────────────────────────────────────────────────
// 2. Border uniformity check
// ────────────────────────────────────────────────────────────────────────────

/// Returns true if all four border sides have identical width, colour, and
/// style (excluding groove/ridge/inset/outset which are never uniform).
/// Isomorphic copy of container_skia.cpp L1445-1454.
bool is_uniform_border(const litehtml::borders& borders);

// ────────────────────────────────────────────────────────────────────────────
// 3. CSS custom-property helpers
// ────────────────────────────────────────────────────────────────────────────

/// Extract the variable name from a CSS var() expression.
/// Returns the empty string on malformed input.
/// Isomorphic copy of container_skia.cpp L1848-1868 (resolve_color).
std::string extract_var_name(const std::string& color);

/// Returns true if \p name starts with "--" (CSS custom property).
inline bool is_css_custom_property(const std::string& name) {
    return name.size() > 2 && name[0] == '-' && name[1] == '-';
}

// ────────────────────────────────────────────────────────────────────────────
// 4. BiDi first-character classification
// ────────────────────────────────────────────────────────────────────────────

/// Returns true if the first character of \p text is a "strong" BiDi character
/// (ASCII letter/digit, CJK ideograph, Hiragana, Katakana, or Hangul).
/// Isomorphic copy of container_skia.cpp L1666-1686 (get_bidi_level).
bool is_first_char_bidi_strong(const char* text);

// ────────────────────────────────────────────────────────────────────────────
// 5. CSS transform matrix computation
// ────────────────────────────────────────────────────────────────────────────

/// Build an SkMatrix for a single CSS transform function.
///
/// @param name  Lower-case CSS function name (e.g. "matrix", "translate").
/// @param vals  Pre-resolved numeric arguments. Percentages have already been
///              converted to absolute values; angle units have already been
///              normalised to degrees for rotate/skew functions.
/// @return The corresponding 2D affine transformation matrix (identity when
///         the function is unrecognised or has insufficient arguments).
/// Isomorphic copy of container_skia.cpp L2054-2084 (push_transform).
SkMatrix compute_transform_matrix(const std::string& name, const std::vector<float>& vals);

// ────────────────────────────────────────────────────────────────────────────
// 6. CSS filter-chain builders
// ────────────────────────────────────────────────────────────────────────────

/// Build a SkImageFilter chain from CSS backdrop-filter tokens.
/// Supports: blur, brightness, contrast, grayscale, sepia, saturate,
///           hue-rotate, invert, opacity.
/// Isomorphic copy of container_skia.cpp L148-345 (push_backdrop_filter).
sk_sp<SkImageFilter> build_backdrop_filter_chain(const litehtml::css_token_vector& filter);

/// Build a SkImageFilter chain from CSS filter tokens.
/// Supports: blur, drop-shadow.
/// Isomorphic copy of container_skia.cpp L2130-2179 (push_filter).
sk_sp<SkImageFilter> build_filter_chain(const litehtml::css_token_vector& filter);

#endif  // CONTAINER_SKIA_HELPERS_H
