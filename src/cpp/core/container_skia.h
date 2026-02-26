#ifndef CONTAINER_SKIA_H
#ifndef LH_TYPES_H
#include "libs/litehtml/include/litehtml/types.h"
#endif
#define CONTAINER_SKIA_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "bridge/bridge_types.h"
#include "core/text/text_renderer.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkRRect.h"
#include "include/core/SkTypeface.h"
#include "libs/litehtml/include/litehtml.h"
#include "resource_manager.h"
#include "satoru_context.h"
#include "utils/logging.h"

class container_skia : public litehtml::document_container {
    SkCanvas *m_canvas;
    int m_width;
    int m_height;
    SatoruContext &m_context;
    ResourceManager *m_resourceManager;

    std::vector<shadow_info> m_usedShadows;
    std::vector<text_shadow_info> m_usedTextShadows;
    std::vector<image_draw_info> m_usedImageDraws;
    std::vector<conic_gradient_info> m_usedConicGradients;
    std::vector<radial_gradient_info> m_usedRadialGradients;
    std::vector<linear_gradient_info> m_usedLinearGradients;
    std::vector<text_draw_info> m_usedTextDraws;
    std::vector<filter_info> m_usedFilters;

    std::set<char32_t> m_usedCodepoints;
    std::set<font_request> m_requestedFontAttributes;

    std::set<font_request> m_missingFonts;
    int m_last_bidi_level = -1;
    int m_last_base_level = -1;

    std::vector<bool> m_asciiUsed;

    std::vector<std::pair<litehtml::position, litehtml::border_radiuses>> m_clips;
    std::vector<float> m_opacity_stack;

    bool m_tagging;
    bool m_textToPaths = false;

    std::vector<std::string> m_usedInlineSvgs;
    std::vector<litehtml::position> m_inlineSvgPositions;
    std::vector<clip_info> m_usedClips;
    std::vector<clip_path_info> m_usedClipPaths;
    std::vector<SkPath> m_usedGlyphs;
    std::vector<glyph_draw_info> m_usedGlyphDraws;

    int m_filter_stack_depth = 0;
    int m_transform_stack_depth = 0;
    int m_clip_path_stack_depth = 0;

    satoru::TextBatcher *m_textBatcher = nullptr;

    std::map<font_request, std::vector<font_info *>> m_createdFonts;

    float get_current_opacity() const {
        float opacity = 1.0f;
        for (float o : m_opacity_stack) {
            opacity *= o;
        }
        return opacity;
    }

   public:
    container_skia(int w, int h, SkCanvas *canvas, SatoruContext &context, ResourceManager *rm,
                   bool tagging = false);
    virtual ~container_skia();

    void set_canvas(SkCanvas *canvas) {
        m_canvas = canvas;
        if (m_textBatcher) {
            m_textBatcher->flush();
            delete m_textBatcher;
        }
        m_textBatcher = new satoru::TextBatcher(&m_context, m_canvas);
    }
    void set_height(int h) { m_height = h; }
    void set_tagging(bool t) { m_tagging = t; }
    void set_text_to_paths(bool to_paths) { m_textToPaths = to_paths; }
    void flush() {
        if (m_textBatcher && m_textBatcher->isActive()) {
            m_textBatcher->flush();
        }
    }
    void reset() {
        if (m_textBatcher) m_textBatcher->flush();
        m_usedShadows.clear();
        m_usedTextShadows.clear();
        m_usedImageDraws.clear();
        m_usedConicGradients.clear();
        m_usedRadialGradients.clear();
        m_usedLinearGradients.clear();
        m_usedTextDraws.clear();
        m_usedInlineSvgs.clear();
        m_usedFilters.clear();
        m_usedClips.clear();
        m_usedClipPaths.clear();
        m_usedGlyphs.clear();
        m_usedGlyphDraws.clear();
        m_filter_stack_depth = 0;
        m_transform_stack_depth = 0;
        m_clip_path_stack_depth = 0;
    }

    SkCanvas *get_canvas() const { return m_canvas; }
    bool is_tagging() const { return m_tagging; }

    int add_inline_svg(const std::string &xml, const litehtml::position &pos) {
        m_usedInlineSvgs.push_back(xml);
        m_inlineSvgPositions.push_back(pos);
        return (int)m_usedInlineSvgs.size();
    }

    const std::vector<std::string> &get_used_inline_svgs() const { return m_usedInlineSvgs; }

    const std::vector<image_draw_info> &get_used_image_draws() const { return m_usedImageDraws; }
    const std::vector<conic_gradient_info> &get_used_conic_gradients() const {
        return m_usedConicGradients;
    }
    const std::vector<radial_gradient_info> &get_used_radial_gradients() const {
        return m_usedRadialGradients;
    }
    const std::vector<linear_gradient_info> &get_used_linear_gradients() const {
        return m_usedLinearGradients;
    }
    const std::vector<shadow_info> &get_used_shadows() const { return m_usedShadows; }
    const std::vector<text_shadow_info> &get_used_text_shadows() const { return m_usedTextShadows; }
    const std::vector<text_draw_info> &get_used_text_draws() const { return m_usedTextDraws; }
    const std::vector<filter_info> &get_used_filters() const { return m_usedFilters; }
    const std::vector<clip_info> &get_used_clips() const { return m_usedClips; }
    const std::vector<clip_path_info> &get_used_clip_paths() const { return m_usedClipPaths; }
    const std::vector<SkPath> &get_used_glyphs() const { return m_usedGlyphs; }
    const std::vector<glyph_draw_info> &get_used_glyph_draws() const { return m_usedGlyphDraws; }

    int add_glyph(const SkPath &path) {
        for (size_t i = 0; i < m_usedGlyphs.size(); ++i) {
            if (m_usedGlyphs[i] == path) return (int)i + 1;
        }
        m_usedGlyphs.push_back(path);
        return (int)m_usedGlyphs.size();
    }

    int add_glyph_draw(const glyph_draw_info &info) {
        m_usedGlyphDraws.push_back(info);
        return (int)m_usedGlyphDraws.size();
    }

    const std::set<char32_t> &get_used_codepoints() const { return m_usedCodepoints; }
    const std::set<font_request> &get_requested_font_attributes() const {
        return m_requestedFontAttributes;
    }

    const std::set<font_request> &get_missing_fonts() const { return m_missingFonts; }

    std::map<font_request, std::set<char32_t>> get_used_fonts_characters() const;

    // litehtml::document_container implementations
    virtual litehtml::uint_ptr create_font(const litehtml::font_description &desc,
                                           const litehtml::document *doc,
                                           litehtml::font_metrics *fm) override;
    virtual void delete_font(litehtml::uint_ptr hFont) override;
    virtual litehtml::pixel_t text_width(const char *text, litehtml::uint_ptr hFont,
                                         litehtml::direction dir, litehtml::writing_mode mode) override;
    virtual void draw_text(litehtml::uint_ptr hdc, const char *text, litehtml::uint_ptr hFont,
                           litehtml::web_color color, const litehtml::position &pos,
                           litehtml::text_overflow overflow, litehtml::direction dir, litehtml::writing_mode mode) override;
    virtual litehtml::pixel_t pt_to_px(float pt) const override;
    virtual litehtml::pixel_t get_default_font_size() const override;
    virtual const char *get_default_font_name() const override;
    virtual void draw_list_marker(litehtml::uint_ptr hdc,
                                  const litehtml::list_marker &marker) override;
    virtual void load_image(const char *src, const char *baseurl, bool redraw_on_ready) override;
    virtual void get_image_size(const char *src, const char *baseurl, litehtml::size &sz) override;
    virtual void draw_image(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                            const std::string &url, const std::string &base_url) override;
    virtual void draw_solid_fill(litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
                                 const litehtml::web_color &color) override;
    virtual void draw_linear_gradient(
        litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
        const litehtml::background_layer::linear_gradient &gradient) override;
    virtual void draw_radial_gradient(
        litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
        const litehtml::background_layer::radial_gradient &gradient) override;
    virtual void draw_conic_gradient(
        litehtml::uint_ptr hdc, const litehtml::background_layer &layer,
        const litehtml::background_layer::conic_gradient &gradient) override;
    virtual void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders &borders,
                              const litehtml::position &draw_pos, bool root) override;
    virtual void draw_box_shadow(litehtml::uint_ptr hdc, const litehtml::shadow_vector &shadows,
                                 const litehtml::position &pos,
                                 const litehtml::border_radiuses &radius, bool inset) override;

    virtual int get_bidi_level(const char *text, int base_level) override;
    virtual void set_caption(const char *caption) override {}
    virtual void set_base_url(const char *base_url) override {}
    virtual void link(const std::shared_ptr<litehtml::document> &doc,
                      const litehtml::element::ptr &el) override {}
    virtual void on_anchor_click(const char *url, const litehtml::element::ptr &el) override {}
    virtual void on_mouse_event(const litehtml::element::ptr &el,
                                litehtml::mouse_event event) override {}
    virtual void set_cursor(const char *cursor) override {}
    virtual void transform_text(litehtml::string &text, litehtml::text_transform tt) override;
    virtual void import_css(litehtml::string &text, const litehtml::string &url,
                            litehtml::string &baseurl) override;
    virtual void set_clip(const litehtml::position &pos,
                          const litehtml::border_radiuses &bdr_radius) override;
    virtual void del_clip() override;
    virtual void get_viewport(litehtml::position &viewport) const override;
    virtual void get_media_features(litehtml::media_features &features) const override;
    virtual void get_language(litehtml::string &language, litehtml::string &culture) const override;
    virtual void split_text(const char *text, const std::function<void(const char *)> &on_word,
                            const std::function<void(const char *)> &on_space) override;

    virtual void push_layer(litehtml::uint_ptr hdc, float opacity) override;
    virtual void pop_layer(litehtml::uint_ptr hdc) override;

    virtual void push_transform(litehtml::uint_ptr hdc, const litehtml::css_token_vector &transform,
                                const litehtml::css_token_vector &origin,
                                const litehtml::position &pos) override;
    virtual void pop_transform(litehtml::uint_ptr hdc) override;
    virtual void push_filter(litehtml::uint_ptr hdc,
                             const litehtml::css_token_vector &filter) override;
    virtual void pop_filter(litehtml::uint_ptr hdc) override;

    virtual void push_clip_path(litehtml::uint_ptr hdc, const litehtml::css_token_vector &clip_path,
                                const litehtml::position &pos) override;
    virtual void pop_clip_path(litehtml::uint_ptr hdc) override;

    virtual void pop_backdrop_filter(litehtml::uint_ptr hdc) override;
    virtual void push_backdrop_filter(litehtml::uint_ptr hdc,
                                      const std::shared_ptr<litehtml::render_item> &el) override;

    virtual litehtml::element::ptr create_element(
        const char *tag_name, const litehtml::string_map &attributes,
        const std::shared_ptr<litehtml::document> &doc) override;

    static SkPath parse_clip_path(const litehtml::css_token_vector &tokens,
                                  const litehtml::position &pos);
};

#endif

