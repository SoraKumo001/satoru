#include <cmath>

#include "bridge/magic_tags.h"
#include "container_skia.h"

namespace litehtml {
vector<css_token_vector> parse_comma_separated_list(const css_token_vector& tokens);
}
#include <cmath>

#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRRect.h"
#include "include/core/SkRect.h"
#include "libs/litehtml/include/litehtml/css_length.h"
#include "utils/skia_utils.h"

// ────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ────────────────────────────────────────────────────────────────────────────
namespace {

static size_t skip_ws(const litehtml::css_token_vector& tokens, size_t i) {
    while (i < tokens.size() && tokens[i].type == ' ') ++i;
    return i;
}

static size_t find_ident_in_group(const litehtml::css_token_vector& tokens, size_t i,
                                  const std::string& name) {
    while (i < tokens.size()) {
        i = skip_ws(tokens, i);
        if (i >= tokens.size()) break;
        if (tokens[i].ident() == name) return i;
        i++;
    }
    return tokens.size();
}

}  // anonymous namespace

// ────────────────────────────────────────────────────────────────────────────
// Clip (set_clip / del_clip)
// ────────────────────────────────────────────────────────────────────────────

void container_skia::set_clip(const litehtml::position& pos,
                              const litehtml::border_radiuses& bdr_radius) {
    if (!m_canvas) return;
    flush();
    if (m_tagging) {
        clip_info info;
        info.pos = pos;
        info.radius = bdr_radius;
        m_usedClips.push_back(info);

        SkPaint p;
        p.setColor(make_magic_color(satoru::MagicTag::ClipPush, (int)m_usedClips.size()));
        m_canvas->drawRect(SkRect::MakeXYWH(0, 0, 0.001f, 0.001f), p);
    } else {
        m_canvas->save();
        if (pos.width > 0 && pos.height > 0) {
            m_canvas->clipRRect(make_rrect(pos, bdr_radius), true);
        }
    }
    m_clips.push_back({pos, bdr_radius});
}

void container_skia::del_clip() {
    if (m_clips.empty()) return;
    if (m_canvas) {
        flush();
        if (m_tagging) {
            SkPaint p;
            p.setColor(make_magic_color(satoru::MagicTag::ClipPop));
            m_canvas->drawRect(SkRect::MakeXYWH(0, 0, 0.001f, 0.001f), p);
        } else {
            m_canvas->restore();
        }
    }
    m_clips.pop_back();
}

// ────────────────────────────────────────────────────────────────────────────
// CSS clip-path
// ────────────────────────────────────────────────────────────────────────────

SkPath container_skia::parse_clip_path(const litehtml::css_token_vector& tokens,
                                       const litehtml::position& pos) {
    SkPathBuilder builder;
    if (tokens.empty()) return builder.detach();

    for (const auto& tok : tokens) {
        if (tok.type == litehtml::CV_FUNCTION) {
            std::string name = litehtml::lowcase(tok.name);
            auto args = litehtml::parse_comma_separated_list(tok.value);

            if (name == "circle") {
                float cx = (float)pos.x + (float)pos.width * 0.5f;
                float cy = (float)pos.y + (float)pos.height * 0.5f;
                float r = std::min((float)pos.width, (float)pos.height) * 0.5f;

                if (!args.empty() && !args[0].empty()) {
                    size_t idx = skip_ws(args[0], 0);
                    if (idx < args[0].size()) {
                        litehtml::css_length len;
                        if (len.from_token(args[0][idx], litehtml::f_length_percentage)) {
                            r = len.calc_percent((float)std::sqrt((double)pos.width * pos.width +
                                                                  (double)pos.height * pos.height) /
                                                 (float)std::sqrt(2.0));
                        }
                        idx++;
                    }

                    size_t at_pos = find_ident_in_group(args[0], idx, "at");
                    if (at_pos < args[0].size()) {
                        size_t k = at_pos + 1;
                        k = skip_ws(args[0], k);
                        if (k < args[0].size()) {
                            litehtml::css_length lx;
                            if (lx.from_token(args[0][k], litehtml::f_length_percentage)) {
                                cx = lx.calc_percent((float)pos.width) + (float)pos.x;
                            }
                            k++;
                            k = skip_ws(args[0], k);
                            if (k < args[0].size()) {
                                litehtml::css_length ly;
                                if (ly.from_token(args[0][k], litehtml::f_length_percentage)) {
                                    cy = ly.calc_percent((float)pos.height) + (float)pos.y;
                                }
                            }
                        }
                    }
                }
                builder.addCircle(cx, cy, r);
            } else if (name == "ellipse") {
                float cx = (float)pos.x + (float)pos.width * 0.5f;
                float cy = (float)pos.y + (float)pos.height * 0.5f;
                float rx = (float)pos.width * 0.5f;
                float ry = (float)pos.height * 0.5f;

                if (!args.empty() && !args[0].empty()) {
                    size_t idx = skip_ws(args[0], 0);
                    if (idx < args[0].size()) {
                        litehtml::css_length lx;
                        if (lx.from_token(args[0][idx], litehtml::f_length_percentage)) {
                            rx = lx.calc_percent((float)pos.width);
                        }
                        idx++;
                        idx = skip_ws(args[0], idx);
                        if (idx < args[0].size()) {
                            litehtml::css_length ly;
                            if (ly.from_token(args[0][idx], litehtml::f_length_percentage)) {
                                ry = ly.calc_percent((float)pos.height);
                            }
                            idx++;
                        }
                    }

                    size_t at_pos = find_ident_in_group(args[0], idx, "at");
                    if (at_pos < args[0].size()) {
                        size_t k = at_pos + 1;
                        k = skip_ws(args[0], k);
                        if (k < args[0].size()) {
                            litehtml::css_length lx;
                            if (lx.from_token(args[0][k], litehtml::f_length_percentage)) {
                                cx = lx.calc_percent((float)pos.width) + (float)pos.x;
                            }
                            k++;
                            k = skip_ws(args[0], k);
                            if (k < args[0].size()) {
                                litehtml::css_length ly;
                                if (ly.from_token(args[0][k], litehtml::f_length_percentage)) {
                                    cy = ly.calc_percent((float)pos.height) + (float)pos.y;
                                }
                            }
                        }
                    }
                }
                builder.addOval(SkRect::MakeLTRB(cx - rx, cy - ry, cx + rx, cy + ry));
            } else if (name == "inset") {
                float top = 0, right = 0, bottom = 0, left = 0;
                litehtml::border_radiuses b_radius;

                if (!args.empty()) {
                    std::vector<litehtml::css_length> lengths;
                    bool has_round = false;
                    std::vector<litehtml::css_length> round_lengths;

                    for (const auto& group : args) {
                        if (group.empty()) continue;

                        size_t round_pos = find_ident_in_group(group, 0, "round");
                        if (round_pos < group.size()) {
                            has_round = true;
                            for (size_t j = 0; j < round_pos;) {
                                j = skip_ws(group, j);
                                if (j >= round_pos) break;
                                litehtml::css_length l;
                                if (l.from_token(group[j], litehtml::f_length_percentage)) {
                                    lengths.push_back(l);
                                }
                                j++;
                            }
                            for (size_t j = round_pos + 1; j < group.size();) {
                                j = skip_ws(group, j);
                                if (j >= group.size()) break;
                                litehtml::css_length l;
                                if (l.from_token(group[j], litehtml::f_length_percentage)) {
                                    round_lengths.push_back(l);
                                }
                                j++;
                            }
                        } else {
                            for (size_t j = 0; j < group.size();) {
                                j = skip_ws(group, j);
                                if (j >= group.size()) break;
                                litehtml::css_length l;
                                if (l.from_token(group[j], litehtml::f_length_percentage)) {
                                    lengths.push_back(l);
                                }
                                j++;
                            }
                        }
                    }

                    if (lengths.size() == 1) {
                        top = right = bottom = left = lengths[0].calc_percent((float)pos.height);
                    } else if (lengths.size() == 2) {
                        top = bottom = lengths[0].calc_percent((float)pos.height);
                        right = left = lengths[1].calc_percent((float)pos.width);
                    } else if (lengths.size() == 3) {
                        top = lengths[0].calc_percent((float)pos.height);
                        right = left = lengths[1].calc_percent((float)pos.width);
                        bottom = lengths[2].calc_percent((float)pos.height);
                    } else if (lengths.size() >= 4) {
                        top = lengths[0].calc_percent((float)pos.height);
                        right = lengths[1].calc_percent((float)pos.width);
                        bottom = lengths[2].calc_percent((float)pos.height);
                        left = lengths[3].calc_percent((float)pos.width);
                    }

                    if (has_round && !round_lengths.empty()) {
                        float rr =
                            round_lengths[0].calc_percent((float)std::min(pos.width, pos.height));
                        b_radius.top_left_x = b_radius.top_left_y = (int)rr;
                        b_radius.top_right_x = b_radius.top_right_y = (int)rr;
                        b_radius.bottom_right_x = b_radius.bottom_right_y = (int)rr;
                        b_radius.bottom_left_x = b_radius.bottom_left_y = (int)rr;
                    }
                }
                SkRect r = SkRect::MakeLTRB((float)pos.x + left, (float)pos.y + top,
                                            (float)pos.x + (float)pos.width - right,
                                            (float)pos.y + (float)pos.height - bottom);
                if (b_radius.is_zero()) {
                    builder.addRect(r);
                } else {
                    builder.addRRect(make_rrect(litehtml::position((int)r.fLeft, (int)r.fTop,
                                                                   (int)r.width(), (int)r.height()),
                                                b_radius));
                }
            } else if (name == "polygon") {
                bool first = true;
                for (const auto& group : args) {
                    size_t idx = skip_ws(group, 0);
                    if (idx >= group.size()) continue;
                    litehtml::css_length lx;
                    if (!lx.from_token(group[idx], litehtml::f_length_percentage)) continue;
                    idx++;
                    idx = skip_ws(group, idx);
                    if (idx >= group.size()) continue;
                    litehtml::css_length ly;
                    if (!ly.from_token(group[idx], litehtml::f_length_percentage)) continue;

                    float px = lx.calc_percent((float)pos.width) + (float)pos.x;
                    float py = ly.calc_percent((float)pos.height) + (float)pos.y;
                    if (first) {
                        builder.moveTo(px, py);
                        first = false;
                    } else {
                        builder.lineTo(px, py);
                    }
                }
                builder.close();
            }
        }
    }
    return builder.detach();
}

void container_skia::push_clip_path(litehtml::uint_ptr hdc,
                                    const litehtml::css_token_vector& clip_path,
                                    const litehtml::position& pos) {
    if (!m_canvas || clip_path.empty()) return;
    flush();

    if (m_tagging) {
        clip_path_info info;
        info.tokens = clip_path;
        info.pos = pos;
        m_usedClipPaths.push_back(info);
        int index = (int)m_usedClipPaths.size();

        SkPaint p;
        p.setColor(make_magic_color(satoru::MagicTag::ClipPathPush, index));

        SkRect rect;
        if (!m_clips.empty()) {
            rect = SkRect::MakeXYWH((float)m_clips.back().first.x, (float)m_clips.back().first.y,
                                    (float)m_clips.back().first.width,
                                    (float)m_clips.back().first.height);
        } else {
            rect = SkRect::MakeWH((float)m_width, (float)m_height);
        }

        m_canvas->drawRect(rect, p);
        m_clip_path_stack_depth++;
        return;
    }

    SkPath path = container_skia::parse_clip_path(clip_path, pos);
    if (!path.isEmpty()) {
        m_canvas->save();
        m_canvas->clipPath(path, true);
        m_clip_path_stack_depth++;
    } else {
        m_canvas->save();
        m_clip_path_stack_depth++;
    }
}

void container_skia::pop_clip_path(litehtml::uint_ptr hdc) {
    if (m_clip_path_stack_depth <= 0) return;
    m_clip_path_stack_depth--;

    if (m_canvas) {
        flush();
        if (m_tagging) {
            SkPaint p;
            p.setColor(make_magic_color(satoru::MagicTag::ClipPathPop));
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

// CSS mask, unknown property → container_skia.cpp (core)

void container_skia::on_unknown_property(const litehtml::string& name,
                                         const litehtml::css_token_vector& value) {
    if (name == "transition" || name == "animation" || name == "resize" ||
        name == "scrollbar-width" || name == "-webkit-box-flex" || name == "-ms-overflow-style" ||
        name == "-webkit-overflow-scrolling" || name == "-webkit-font-smoothing" ||
        name == "color-scheme" || name == "-webkit-text-size-adjust" || name == "line-break" ||
        name == "text-size-adjust") {
        return;
    }
}
