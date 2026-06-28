#include <gtest/gtest.h>
#include <core/SkMatrix.h>
#include <string>
#include <vector>
#include <cmath>

// ============================================================================
// Isomorphic copy of the CSS transform function → matrix logic from
// container_skia::push_transform (container_skia.cpp L2030-2085).
//
// The production code parses css_token values inline; this test extracts
// the pure matrix-computation portion so it can be tested without linking
// the full container_skia.cpp (which has heavy Skia dependencies).
// ============================================================================
namespace {

// ── unit conversion (mirrors L2044-2051) ──────────────────────────────────

/// Convert an angle value to degrees as done in push_transform.
/// "rad" → multiply by 180/π; "turn" → multiply by 360; else pass-through.
static float convert_angle_to_degrees(float value, const std::string& unit) {
    if (unit == "rad") {
        return value * 180.0f / 3.14159265f;
    }
    if (unit == "turn") {
        return value * 360.0f;
    }
    return value;  // "deg" or unitless → already degrees
}

// ── matrix computation (mirrors L2057-2085) ───────────────────────────────

/// Build an SkMatrix for a single CSS transform function.
///
/// @param name  Lower-case CSS function name (e.g. "matrix", "translate3d").
/// @param vals  Pre-resolved numeric arguments.  Percentages have already
///              been converted to absolute values; angle units have already
///              been normalised to degrees for rotate/skew functions.
/// @return The corresponding 2D affine transformation matrix (identity when
///         the function is unrecognised or has insufficient arguments).
static SkMatrix compute_transform_matrix(const std::string& name,
                                          const std::vector<float>& vals) {
    SkMatrix m;  // default-constructs as identity

    if (name == "matrix" && vals.size() >= 6) {
        // CSS matrix(a,b,c,d,e,f) →  [a  c  e]
        //                            [b  d  f]
        //                            [0  0  1]
        m.setAll(vals[0], vals[2], vals[4],
                 vals[1], vals[3], vals[5],
                 0, 0, 1);
    } else if (name == "translate" || name == "translate3d") {
        float tx = vals.size() > 0 ? vals[0] : 0;
        float ty = vals.size() > 1 ? vals[1] : 0;
        m.setTranslate(tx, ty);
    } else if (name == "translatex") {
        m.setTranslate(vals.size() > 0 ? vals[0] : 0, 0);
    } else if (name == "translatey") {
        m.setTranslate(0, vals.size() > 0 ? vals[0] : 0);
    } else if (name == "scale" || name == "scale3d") {
        float sx = vals.size() > 0 ? vals[0] : 1;
        float sy = vals.size() > 1 ? vals[1] : sx;  // sy defaults to sx
        m.setScaleTranslate(sx, sy, 0, 0);
    } else if (name == "scalex") {
        m.setScaleTranslate(vals.size() > 0 ? vals[0] : 1, 1, 0, 0);
    } else if (name == "scaley") {
        m.setScaleTranslate(1, vals.size() > 0 ? vals[0] : 1, 0, 0);
    } else if (name == "rotate" || name == "rotatez") {
        float deg = vals.size() > 0 ? vals[0] : 0;
        float rad = deg * 3.14159265f / 180.0f;
        float c = cosf(rad);
        float s = sinf(rad);
        // 2D rotation:  [cos -sin  0]
        //               [sin  cos  0]
        //               [0    0    1]
        m.setAll(c, -s, 0, s, c, 0, 0, 0, 1);
    } else if (name == "skew") {
        float kx = vals.size() > 0 ? vals[0] : 0;
        float ky = vals.size() > 1 ? vals[1] : 0;
        // 2D skew:  [1  tan(kx)  0]
        //           [tan(ky)  1  0]
        //           [0       0   1]
        m.setAll(1, tanf(kx * 3.14159265f / 180.0f), 0,
                 tanf(ky * 3.14159265f / 180.0f), 1, 0,
                 0, 0, 1);
    } else if (name == "skewx") {
        m.setAll(1, tanf(vals[0] * 3.14159265f / 180.0f), 0,
                 0, 1, 0,
                 0, 0, 1);
    } else if (name == "skewy") {
        m.setAll(1, 0, 0,
                 tanf(vals[0] * 3.14159265f / 180.0f), 1, 0,
                 0, 0, 1);
    }
    // Unrecognised function name → identity (no-op in the CSS spec)
    return m;
}

static bool is_identity(const SkMatrix& m) {
    SkMatrix id;
    return m == id;
}

}  // anonymous namespace

// ═════════════════════════════════════════════════════════════════════════════
// Unit Conversion Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST(TransformUnitConversion, DegIsPassthrough) {
    EXPECT_FLOAT_EQ(convert_angle_to_degrees(90, "deg"), 90);
}

TEST(TransformUnitConversion, RadToDeg) {
    // π/2 rad → 90°
    float result = convert_angle_to_degrees(3.14159265f / 2.0f, "rad");
    EXPECT_NEAR(result, 90.0f, 0.001f);
}

TEST(TransformUnitConversion, TurnToDeg) {
    EXPECT_FLOAT_EQ(convert_angle_to_degrees(1.0f, "turn"), 360.0f);
    EXPECT_FLOAT_EQ(convert_angle_to_degrees(0.25f, "turn"), 90.0f);
    EXPECT_FLOAT_EQ(convert_angle_to_degrees(0.5f, "turn"), 180.0f);
}

TEST(TransformUnitConversion, UnitlessDefaultsToDeg) {
    // No unit (or unknown unit) → pass through as degrees
    EXPECT_FLOAT_EQ(convert_angle_to_degrees(45, ""), 45);
    EXPECT_FLOAT_EQ(convert_angle_to_degrees(-30, "xxx"), -30);
}

// ═════════════════════════════════════════════════════════════════════════════
// matrix() Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST(TransformMatrix, Identity) {
    auto m = compute_transform_matrix("matrix", {1, 0, 0, 1, 0, 0});
    EXPECT_TRUE(is_identity(m));
}

TEST(TransformMatrix, Scale2x) {
    auto m = compute_transform_matrix("matrix", {2, 0, 0, 2, 0, 0});
    EXPECT_FLOAT_EQ(m.fMat[0], 2);  // scaleX
    EXPECT_FLOAT_EQ(m.fMat[4], 2);  // scaleY
    EXPECT_FLOAT_EQ(m.fMat[2], 0);  // transX
    EXPECT_FLOAT_EQ(m.fMat[5], 0);  // transY
}

TEST(TransformMatrix, Translate) {
    auto m = compute_transform_matrix("matrix", {1, 0, 0, 1, 100, 50});
    EXPECT_FLOAT_EQ(m.fMat[2], 100);
    EXPECT_FLOAT_EQ(m.fMat[5], 50);
    EXPECT_FLOAT_EQ(m.fMat[0], 1);
    EXPECT_FLOAT_EQ(m.fMat[4], 1);
}

TEST(TransformMatrix, RotateAndScale) {
    // matrix(0.5, 0.5, -0.5, 0.5, 0, 0):
    //   [a  c  e]   [ 0.5  -0.5  0]
    //   [b  d  f] = [ 0.5   0.5  0]
    //   [0  0  1]   [ 0     0    1]
    auto m = compute_transform_matrix("matrix", {0.5f, 0.5f, -0.5f, 0.5f, 0, 0});
    EXPECT_FLOAT_EQ(m.fMat[0], 0.5f);
    EXPECT_FLOAT_EQ(m.fMat[1], -0.5f);
    EXPECT_FLOAT_EQ(m.fMat[2], 0);
    EXPECT_FLOAT_EQ(m.fMat[3], 0.5f);
    EXPECT_FLOAT_EQ(m.fMat[4], 0.5f);
    EXPECT_FLOAT_EQ(m.fMat[5], 0);
}

TEST(TransformMatrix, TooFewArgsReturnsIdentity) {
    // matrix(1,0,0,1) → 4 args, condition vals.size() >= 6 fails
    auto m = compute_transform_matrix("matrix", {1, 0, 0, 1});
    EXPECT_TRUE(is_identity(m));
}

TEST(TransformMatrix, ExtraArgsIgnored) {
    // matrix(2,0,0,2,0,0,99) → 7 args, extra 99 is ignored
    auto m = compute_transform_matrix("matrix", {2, 0, 0, 2, 0, 0, 99});
    EXPECT_FLOAT_EQ(m.fMat[0], 2);
    EXPECT_FLOAT_EQ(m.fMat[4], 2);
}

// ═════════════════════════════════════════════════════════════════════════════
// translate() / translate3d() Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST(TransformTranslate3d, Translate2d) {
    auto m = compute_transform_matrix("translate3d", {100, 200, 0});
    EXPECT_FLOAT_EQ(m.fMat[2], 100);
    EXPECT_FLOAT_EQ(m.fMat[5], 200);
}

TEST(TransformTranslate3d, ZIgnored) {
    // Third component (Z) is ignored in 2D rendering
    auto m = compute_transform_matrix("translate3d", {100, 200, 50});
    EXPECT_FLOAT_EQ(m.fMat[2], 100);
    EXPECT_FLOAT_EQ(m.fMat[5], 200);
}

TEST(TransformTranslate3d, EmptyArgs) {
    auto m = compute_transform_matrix("translate3d", {});
    EXPECT_TRUE(is_identity(m));
}

TEST(TransformTranslate, BasicTranslate) {
    auto m = compute_transform_matrix("translate", {50, 25});
    EXPECT_FLOAT_EQ(m.fMat[2], 50);
    EXPECT_FLOAT_EQ(m.fMat[5], 25);
}

TEST(TransformTranslateX, TranslateXOnly) {
    auto m = compute_transform_matrix("translatex", {75});
    EXPECT_FLOAT_EQ(m.fMat[2], 75);
    EXPECT_FLOAT_EQ(m.fMat[5], 0);
}

TEST(TransformTranslateY, TranslateYOnly) {
    auto m = compute_transform_matrix("translatey", {-30});
    EXPECT_FLOAT_EQ(m.fMat[2], 0);
    EXPECT_FLOAT_EQ(m.fMat[5], -30);
}

// ═════════════════════════════════════════════════════════════════════════════
// scale() / scale3d() Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST(TransformScale3d, UniformScale) {
    auto m = compute_transform_matrix("scale3d", {2, 2, 1});
    EXPECT_FLOAT_EQ(m.fMat[0], 2);
    EXPECT_FLOAT_EQ(m.fMat[4], 2);
}

TEST(TransformScale3d, NonUniformScale) {
    auto m = compute_transform_matrix("scale3d", {2, 0.5f, 1});
    EXPECT_FLOAT_EQ(m.fMat[0], 2);
    EXPECT_FLOAT_EQ(m.fMat[4], 0.5f);
}

TEST(TransformScale3d, ZeroScale) {
    auto m = compute_transform_matrix("scale3d", {0, 0, 1});
    EXPECT_FLOAT_EQ(m.fMat[0], 0);
    EXPECT_FLOAT_EQ(m.fMat[4], 0);
}

TEST(TransformScale3d, EmptyArgsDefaultToOne) {
    auto m = compute_transform_matrix("scale3d", {});
    EXPECT_TRUE(is_identity(m));
}

TEST(TransformScale, SingleArgReplicated) {
    // scale(3) → sx=3, sy=3
    auto m = compute_transform_matrix("scale", {3});
    EXPECT_FLOAT_EQ(m.fMat[0], 3);
    EXPECT_FLOAT_EQ(m.fMat[4], 3);
}

TEST(TransformScaleX, ScaleXOnly) {
    auto m = compute_transform_matrix("scalex", {4});
    EXPECT_FLOAT_EQ(m.fMat[0], 4);
    EXPECT_FLOAT_EQ(m.fMat[4], 1);
}

TEST(TransformScaleY, ScaleYOnly) {
    auto m = compute_transform_matrix("scaley", {0.5f});
    EXPECT_FLOAT_EQ(m.fMat[0], 1);
    EXPECT_FLOAT_EQ(m.fMat[4], 0.5f);
}

// ═════════════════════════════════════════════════════════════════════════════
// rotate() Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST(TransformRotate, ZeroDeg) {
    auto m = compute_transform_matrix("rotate", {0});
    EXPECT_TRUE(is_identity(m));
}

TEST(TransformRotate, NinetyDeg) {
    auto m = compute_transform_matrix("rotate", {90});
    // 90°:  [0  -1  0]
    //       [1   0  0]
    //       [0   0  1]
    EXPECT_NEAR(m.fMat[0], 0, 0.001f);
    EXPECT_NEAR(m.fMat[1], -1, 0.001f);
    EXPECT_NEAR(m.fMat[3], 1, 0.001f);
    EXPECT_NEAR(m.fMat[4], 0, 0.001f);
}

TEST(TransformRotate, OneEightyDeg) {
    auto m = compute_transform_matrix("rotate", {180});
    // 180°:  [-1  0  0]
    //        [ 0 -1  0]
    //        [ 0  0  1]
    EXPECT_NEAR(m.fMat[0], -1, 0.001f);
    EXPECT_NEAR(m.fMat[1], 0, 0.001f);
    EXPECT_NEAR(m.fMat[3], 0, 0.001f);
    EXPECT_NEAR(m.fMat[4], -1, 0.001f);
}

TEST(TransformRotate, Radians) {
    // 1.5708 rad ≈ 90° (converted via convert_angle_to_degrees first)
    float deg = convert_angle_to_degrees(1.5708f, "rad");
    auto m = compute_transform_matrix("rotate", {deg});
    EXPECT_NEAR(m.fMat[0], 0, 0.001f);
    EXPECT_NEAR(m.fMat[1], -1, 0.001f);
    EXPECT_NEAR(m.fMat[3], 1, 0.001f);
    EXPECT_NEAR(m.fMat[4], 0, 0.001f);
}

TEST(TransformRotate, Turn) {
    // 0.25 turn = 90°
    float deg = convert_angle_to_degrees(0.25f, "turn");
    auto m = compute_transform_matrix("rotate", {deg});
    EXPECT_NEAR(m.fMat[0], 0, 0.001f);
    EXPECT_NEAR(m.fMat[1], -1, 0.001f);
    EXPECT_NEAR(m.fMat[3], 1, 0.001f);
    EXPECT_NEAR(m.fMat[4], 0, 0.001f);
}

TEST(TransformRotate, FortyFiveDeg) {
    auto m = compute_transform_matrix("rotate", {45});
    float c = cosf(45 * 3.14159265f / 180);
    float s = sinf(45 * 3.14159265f / 180);
    EXPECT_NEAR(m.fMat[0], c, 0.001f);
    EXPECT_NEAR(m.fMat[1], -s, 0.001f);
    EXPECT_NEAR(m.fMat[3], s, 0.001f);
    EXPECT_NEAR(m.fMat[4], c, 0.001f);
}

// ═════════════════════════════════════════════════════════════════════════════
// rotatez() Tests  (same code path as rotate)
// ═════════════════════════════════════════════════════════════════════════════

TEST(TransformRotateZ, EquivalentToRotate) {
    auto m_rz = compute_transform_matrix("rotatez", {90});
    auto m_r  = compute_transform_matrix("rotate",  {90});
    EXPECT_EQ(m_rz, m_r);
}

// ═════════════════════════════════════════════════════════════════════════════
// skew() Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST(TransformSkew, XSkewOnly) {
    auto m = compute_transform_matrix("skew", {30, 0});
    float tan30 = tanf(30 * 3.14159265f / 180);
    EXPECT_NEAR(m.fMat[1], tan30, 0.001f);
    EXPECT_NEAR(m.fMat[3], 0, 0.001f);
}

TEST(TransformSkew, YSkewOnly) {
    auto m = compute_transform_matrix("skew", {0, 30});
    float tan30 = tanf(30 * 3.14159265f / 180);
    EXPECT_NEAR(m.fMat[1], 0, 0.001f);
    EXPECT_NEAR(m.fMat[3], tan30, 0.001f);
}

TEST(TransformSkew, BothAxes) {
    auto m = compute_transform_matrix("skew", {30, 30});
    float tan30 = tanf(30 * 3.14159265f / 180);
    EXPECT_NEAR(m.fMat[1], tan30, 0.001f);
    EXPECT_NEAR(m.fMat[3], tan30, 0.001f);
}

TEST(TransformSkewX, ApplySkewX) {
    auto m = compute_transform_matrix("skewx", {30});
    float tan30 = tanf(30 * 3.14159265f / 180);
    EXPECT_NEAR(m.fMat[1], tan30, 0.001f);
    EXPECT_FLOAT_EQ(m.fMat[3], 0);
}

TEST(TransformSkewY, ApplySkewY) {
    auto m = compute_transform_matrix("skewy", {30});
    float tan30 = tanf(30 * 3.14159265f / 180);
    EXPECT_FLOAT_EQ(m.fMat[1], 0);
    EXPECT_NEAR(m.fMat[3], tan30, 0.001f);
}

// ═════════════════════════════════════════════════════════════════════════════
// Edge Cases
// ═════════════════════════════════════════════════════════════════════════════

TEST(TransformEdgeCase, NegativeMatrixValues) {
    // matrix(-1, 0, 0, -1, -100, -50) → all negative
    auto m = compute_transform_matrix("matrix", {-1, 0, 0, -1, -100, -50});
    EXPECT_FLOAT_EQ(m.fMat[0], -1);
    EXPECT_FLOAT_EQ(m.fMat[4], -1);
    EXPECT_FLOAT_EQ(m.fMat[2], -100);
    EXPECT_FLOAT_EQ(m.fMat[5], -50);
}

TEST(TransformEdgeCase, UnknownFunctionReturnsIdentity) {
    EXPECT_TRUE(is_identity(compute_transform_matrix("nonexistent", {1, 2, 3})));
    EXPECT_TRUE(is_identity(compute_transform_matrix("", {1, 2, 3})));
}

TEST(TransformEdgeCase, NegativeRotate) {
    // rotate(-90°) →  [[0  1  0]
    //                  [-1 0  0]
    //                  [0  0  1]]
    auto m = compute_transform_matrix("rotate", {-90});
    EXPECT_NEAR(m.fMat[0], 0, 0.001f);
    EXPECT_NEAR(m.fMat[3], -1, 0.001f);
}

TEST(TransformEdgeCase, RotateNoVals) {
    EXPECT_TRUE(is_identity(compute_transform_matrix("rotate", {})));
}

TEST(TransformEdgeCase, SkewNoVals) {
    EXPECT_TRUE(is_identity(compute_transform_matrix("skew", {})));
}

TEST(TransformEdgeCase, ScaleZeroZero) {
    // scale(0, 0) — valid but collapses geometry to a point
    auto m = compute_transform_matrix("scale", {0, 0});
    EXPECT_FLOAT_EQ(m.fMat[0], 0);
    EXPECT_FLOAT_EQ(m.fMat[4], 0);
}

TEST(TransformEdgeCase, TranslateSingleArg) {
    // translate(100) → tx=100, ty=0
    auto m = compute_transform_matrix("translate", {100});
    EXPECT_FLOAT_EQ(m.fMat[2], 100);
    EXPECT_FLOAT_EQ(m.fMat[5], 0);
}
