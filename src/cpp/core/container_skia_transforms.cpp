#include "bridge/magic_tags.h"
#include "container_skia.h"
#include "container_skia_helpers.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"

// ────────────────────────────────────────────────────────────────────────────
// Layer (opacity + blend mode)
// ────────────────────────────────────────────────────────────────────────────

void container_skia::push_layer(litehtml::uint_ptr hdc, float opacity, litehtml::blend_mode bm) {
    m_opacity_stack.push_back(opacity);
    if (m_canvas) {
        flush();

        SkBlendMode sk_bm = to_skia_blend_mode(bm);

        if (m_tagging) {
            SkPaint p;
            if (bm == litehtml::blend_mode_normal) {
                p.setColor(make_magic_color(satoru::MagicTag::LayerPush, (int)(opacity * 255.0f)));
            } else {
                // Pack opacity (8-bit) and blend mode (4-bit) into index
                int packed = ((int)(opacity * 255.0f) << 4) | ((int)bm & 0x0F);
                p.setColor(make_magic_color(satoru::MagicTag::LayerPushBlend, packed));
            }
            SkRect rect;
            if (!m_clips.empty()) {
                rect = SkRect::MakeXYWH(
                    (float)m_clips.back().first.x, (float)m_clips.back().first.y,
                    (float)m_clips.back().first.width, (float)m_clips.back().first.height);
            } else {
                rect = SkRect::MakeWH((float)m_width, (float)m_height);
            }
            m_canvas->drawRect(rect, p);
        } else if (opacity < 1.0f || sk_bm != SkBlendMode::kSrcOver) {
            SkPaint paint;
            paint.setAlphaf(opacity);
            paint.setBlendMode(sk_bm);
            m_canvas->saveLayer(nullptr, &paint);
        } else {
            m_canvas->save();
        }
    }
}

void container_skia::pop_layer(litehtml::uint_ptr hdc) {
    if (m_opacity_stack.empty()) return;
    float opacity = m_opacity_stack.back();
    m_opacity_stack.pop_back();

    if (m_canvas) {
        flush();
        if (m_tagging) {
            SkPaint p;
            p.setColor(make_magic_color(satoru::MagicTag::LayerPop));
            SkRect rect;
            if (!m_clips.empty()) {
                rect = SkRect::MakeXYWH(
                    (float)m_clips.back().first.x, (float)m_clips.back().first.y,
                    (float)m_clips.back().first.width, (float)m_clips.back().first.height);
            } else {
                rect = SkRect::MakeWH((float)m_width, (float)m_height);
            }
            m_canvas->drawRect(rect, p);
        } else {
            m_canvas->restore();
        }
    }
}

// ────────────────────────────────────────────────────────────────────────────
// CSS transform
// ────────────────────────────────────────────────────────────────────────────

void container_skia::push_transform(litehtml::uint_ptr hdc,
                                    const litehtml::css_token_vector& transform,
                                    const litehtml::css_token_vector& origin,
                                    const litehtml::position& pos) {
    if (!m_canvas) return;
    flush();

    m_canvas->save();
    m_transform_stack_depth++;

    float ox = pos.x + pos.width * 0.5f;
    float oy = pos.y + pos.height * 0.5f;

    if (!origin.empty()) {
        int n = 0;
        for (const auto& tok : origin) {
            if (tok.type == litehtml::WHITESPACE) continue;
            litehtml::css_length len;
            if (len.from_token(tok, litehtml::f_length_percentage,
                               "left;right;top;bottom;center")) {
                if (n == 0)
                    ox = len.calc_percent(pos.width) + pos.x;
                else if (n == 1)
                    oy = len.calc_percent(pos.height) + pos.y;
                n++;
            }
        }
    }

    m_canvas->translate(ox, oy);

    for (const auto& tok : transform) {
        if (tok.type == litehtml::CV_FUNCTION) {
            std::string name = litehtml::lowcase(tok.name);
            std::vector<float> vals;
            for (const auto& arg_tok : tok.value) {
                if (arg_tok.type == litehtml::NUMBER || arg_tok.type == litehtml::DIMENSION ||
                    arg_tok.type == litehtml::PERCENTAGE) {
                    float v = arg_tok.n.number;
                    if (arg_tok.type == litehtml::PERCENTAGE) {
                        if (name.find("translate") != std::string::npos) {
                            v = v * (vals.empty() ? pos.width : pos.height) / 100.0f;
                        } else {
                            v /= 100.0f;
                        }
                    }
                    if (arg_tok.type == litehtml::DIMENSION &&
                        (litehtml::lowcase(arg_tok.unit) == "rad" ||
                         litehtml::lowcase(arg_tok.unit) == "turn")) {
                        if (litehtml::lowcase(arg_tok.unit) == "rad") {
                            v = v * 180.0f / 3.14159265f;
                        } else if (litehtml::lowcase(arg_tok.unit) == "turn") {
                            v = v * 360.0f;
                        }
                    }
                    vals.push_back(v);
                }
            }

            if (name == "matrix" && vals.size() >= 6) {
                SkMatrix m;
                m.setAll(vals[0], vals[2], vals[4], vals[1], vals[3], vals[5], 0, 0, 1);
                m_canvas->concat(m);
            } else if (name == "translate" || name == "translate3d") {
                m_canvas->translate(vals.size() > 0 ? vals[0] : 0, vals.size() > 1 ? vals[1] : 0);
            } else if (name == "translatex") {
                m_canvas->translate(vals.size() > 0 ? vals[0] : 0, 0);
            } else if (name == "translatey") {
                m_canvas->translate(0, vals.size() > 0 ? vals[0] : 0);
            } else if (name == "scale" || name == "scale3d") {
                float sx = vals.size() > 0 ? vals[0] : 1;
                float sy = vals.size() > 1 ? vals[1] : sx;
                m_canvas->scale(sx, sy);
            } else if (name == "scalex") {
                m_canvas->scale(vals.size() > 0 ? vals[0] : 1, 1);
            } else if (name == "scaley") {
                m_canvas->scale(1, vals.size() > 0 ? vals[0] : 1);
            } else if (name == "rotate" || name == "rotatez") {
                if (!vals.empty()) m_canvas->rotate(vals[0]);
            } else if (name == "skew" && !vals.empty()) {
                float kx = vals[0];
                float ky = vals.size() > 1 ? vals[1] : 0;
                m_canvas->skew(tanf(kx * 3.14159f / 180.0f), tanf(ky * 3.14159f / 180.0f));
            } else if (name == "skewx" && !vals.empty()) {
                m_canvas->skew(tanf(vals[0] * 3.14159f / 180.0f), 0);
            } else if (name == "skewy" && !vals.empty()) {
                m_canvas->skew(0, tanf(vals[0] * 3.14159f / 180.0f));
            }
        }
    }

    m_canvas->translate(-ox, -oy);
}

void container_skia::pop_transform(litehtml::uint_ptr hdc) {
    if (m_transform_stack_depth <= 0) return;
    m_transform_stack_depth--;

    if (m_canvas) {
        flush();
        m_canvas->restore();
    }
}
