#include "html.h"
#include "style.h"
#include "css_parser.h"
#include "internal.h"
#include <set>
#include <cstdio>
#include "html_tag.h"
#include "document.h"

namespace litehtml
{

  bool parse_bg_image(const css_token &token, image &bg_image, document_container *container);
  bool parse_bg_position_size(const css_token_vector &tokens, int &index, css_length &x, css_length &y, css_size &size);
  bool parse_bg_size(const css_token_vector &tokens, int &index, css_size &size);
  bool parse_two_lengths(const css_token_vector &tokens, css_length len[2], int options);
  template <class T, class... Args>
  int parse_1234_values(const css_token_vector &tokens, T result[4], bool (*func)(const css_token &, T &, Args...), Args... args);
  int parse_1234_lengths(const css_token_vector &tokens, css_length len[4], int options, string keywords = "");
  bool parse_border_width(const css_token &tok, css_length &width);
  bool parse_font_family(const css_token_vector &tokens, string &font_family);
  bool parse_font_weight(const css_token &tok, css_length &weight);

  std::map<string_id, string> style::m_valid_values =
      {
          {_display_, style_display_strings},
          {_visibility_, visibility_strings},
          {_position_, element_position_strings},
          {_float_, element_float_strings},
          {_clear_, element_clear_strings},
          {_overflow_, overflow_strings},
          {_appearance_, appearance_strings},
          {_box_sizing_, box_sizing_strings},

          {_text_align_, text_align_strings},
          {_vertical_align_, vertical_align_strings},
          {_text_transform_, text_transform_strings},
          {_white_space_, white_space_strings},

          {_font_style_, font_style_strings},
          {_font_variant_, font_variant_strings},
          {_font_weight_, font_weight_strings},

          {_list_style_type_, list_style_type_strings},
          {_list_style_position_, list_style_position_strings},

          {_border_left_style_, border_style_strings},
          {_border_right_style_, border_style_strings},
          {_border_top_style_, border_style_strings},
          {_border_bottom_style_, border_style_strings},
          {_border_collapse_, border_collapse_strings},
          {_table_layout_, table_layout_strings},

          {_background_attachment_, background_attachment_strings},
          {_background_repeat_, background_repeat_strings},
          {_background_clip_, background_box_strings},
          {_background_origin_, background_box_strings},

          {_flex_direction_, flex_direction_strings},
          {_flex_wrap_, flex_wrap_strings},
          {_justify_content_, flex_justify_content_strings},
          {_align_content_, flex_align_content_strings},
          {_align_items_, flex_align_items_strings},
          {_align_self_, flex_align_items_strings},

          {_caption_side_, caption_side_strings},

          {__webkit_box_orient_, box_orient_strings},
          {_text_decoration_style_, style_text_decoration_style_strings},
          {_text_emphasis_position_, style_text_emphasis_position_strings},
          {_text_overflow_, text_overflow_strings},
  };
  std::map<string_id, vector<string_id>> shorthands =
      {
          {_font_, {_font_style_, _font_variant_, _font_weight_, _font_size_, _line_height_, _font_family_}},

          {_background_, {_background_color_, _background_position_x_, _background_position_y_, _background_repeat_, _background_attachment_, _background_image_, _background_image_baseurl_, _background_size_, _background_origin_, _background_clip_}},

          {_list_style_, {_list_style_image_, _list_style_image_baseurl_, _list_style_position_, _list_style_type_}},

          {_margin_, {_margin_top_, _margin_right_, _margin_bottom_, _margin_left_}},
          {_padding_, {_padding_top_, _padding_right_, _padding_bottom_, _padding_left_}},

          {_border_width_, {_border_top_width_, _border_right_width_, _border_bottom_width_, _border_left_width_}},
          {_border_style_, {_border_top_style_, _border_right_style_, _border_bottom_style_, _border_left_style_}},
          {_border_color_, {_border_top_color_, _border_right_color_, _border_bottom_color_, _border_left_color_}},
          {_border_top_, {_border_top_width_, _border_top_style_, _border_top_color_}},
          {_border_right_, {_border_right_width_, _border_right_style_, _border_right_color_}},
          {_border_bottom_, {_border_bottom_width_, _border_bottom_style_, _border_bottom_color_}},
          {_border_left_, {_border_left_width_, _border_left_style_, _border_left_color_}},
          {_border_, {_border_top_width_, _border_right_width_, _border_bottom_width_, _border_left_width_, _border_top_style_, _border_right_style_, _border_bottom_style_, _border_left_style_, _border_top_color_, _border_right_color_, _border_bottom_color_, _border_left_color_}},

          {_flex_, {_flex_grow_, _flex_shrink_, _flex_basis_}},
          {_flex_flow_, {_flex_direction_, _flex_wrap_}},
          {_gap_, {_row_gap_, _column_gap_}},

          {_text_decoration_, {_text_decoration_color_, _text_decoration_line_, _text_decoration_style_, _text_decoration_thickness_}},
          {_text_emphasis_, {_text_emphasis_style_, _text_emphasis_color_}},
  };

  void style::add(const string &txt, const string &baseurl, document_container *container, int layer, selector_specificity specificity)
  {
    auto tokens = normalize(txt, f_componentize);
    add(tokens, baseurl, container, layer, specificity);
  }

  void style::add(const css_token_vector &tokens, const string &baseurl, document_container *container, int layer, selector_specificity specificity)
  {
    m_layer = layer;
    m_specificity = specificity;
    raw_declaration::vector decls;
    raw_rule::vector rules;
    css_parser(tokens).consume_style_block_contents(decls, rules);
    if (!rules.empty())
      css_parse_error("rule inside a style block");
    if (decls.empty())
      return;

    for (auto &decl : decls)
    {
      remove_whitespace(decl.value);
      string name = decl.name.substr(0, 2) == "--" ? decl.name : lowcase(decl.name);
      add_property(_id(name), decl.value, baseurl, decl.important, container, m_layer, m_specificity);
    }
  }

  bool has_var(const css_token_vector &tokens)
  {
    for (auto &tok : tokens)
    {
      if (tok.type == CV_FUNCTION && lowcase(tok.name) == "var")
        return true;
      if (tok.is_component_value() && has_var(tok.value))
        return true;
    }
    return false;
  }

  void style::inherit_property(string_id name, bool important)
  {
    auto atomic_properties = at(shorthands, name);
    if (!atomic_properties.empty())
    {
      for (auto atomic : atomic_properties)
        add_parsed_property(atomic, property_value(inherit(), important, false, m_layer, m_specificity));
    }
    else
      add_parsed_property(name, property_value(inherit(), important, false, m_layer, m_specificity));
  }

  void style::add_length_property(string_id name, css_token val, string keywords, int options, bool important)
  {
    css_length length;
    if (length.from_token(val, options, keywords))
      add_parsed_property(name, property_value(length, important, false, m_layer, m_specificity));
  }

  void style::add_property(string_id name, const css_token_vector &value, const string &baseurl, bool important, document_container *container, int layer, selector_specificity specificity)
  {
    m_layer = layer;
    m_specificity = specificity;
    if (value.empty() && _s(name).substr(0, 2) != "--")
      return;

    if (has_var(value))
      return add_parsed_property(name, property_value(value, important, true, m_layer, m_specificity));

    css_token val = value.size() == 1 ? value[0] : css_token();
    string ident = val.ident();

    if (ident == "inherit")
      return inherit_property(name, important);

    int idx[4];
    web_color clr[4];
    css_length len[4];
    string str;

    switch (name)
    {
    case _transform_:
    case _transform_origin_:
    case _translate_:
    case _rotate_:
    case _scale_:
    case _filter_:
    case _backdrop_filter_:
      add_parsed_property(name, property_value(value, important, false, m_layer, m_specificity));
      break;

    case _display_:
    case _visibility_:
    case _position_:
    case _float_:
    case _clear_:
    case _appearance_:
    case _box_sizing_:
    case _overflow_:
    case _text_overflow_:
    case _text_align_:
    case _vertical_align_:
    case _text_transform_:
    case _white_space_:
    case _font_style_:
    case _font_variant_:
    case _text_decoration_style_:
    case _list_style_type_:
    case _list_style_position_:
    case _border_top_style_:
    case _border_bottom_style_:
    case _border_left_style_:
    case _border_right_style_:
    case _border_collapse_:
    case _table_layout_:
    case _flex_direction_:
    case _flex_wrap_:
    case _justify_content_:
    case _align_content_:
    case _caption_side_:
    case __webkit_box_orient_:
      if (int index = value_index(ident, m_valid_values[name]); index >= 0)
        add_parsed_property(name, property_value(index, important, false, m_layer, m_specificity));
      break;

    case _z_index_:
      return add_length_property(name, val, "auto", f_integer, important);

    case _text_indent_:
      return add_length_property(name, val, "", f_length_percentage, important);

    case _padding_left_:
    case _padding_right_:
    case _padding_top_:
    case _padding_bottom_:
      return add_length_property(name, val, "", f_length_percentage | f_positive, important);

    case _left_:
    case _right_:
    case _top_:
    case _bottom_:
    case _margin_left_:
    case _margin_right_:
    case _margin_top_:
    case _margin_bottom_:
      return add_length_property(name, val, "auto", f_length_percentage, important);

    case _width_:
    case _height_:
    case _min_width_:
    case _min_height_:
      return add_length_property(name, val, "auto", f_length_percentage | f_positive, important);

    case _max_width_:
    case _max_height_:
      return add_length_property(name, val, "none", f_length_percentage | f_positive, important);

    case _line_height_:
      return add_length_property(name, val, "normal", f_number | f_length_percentage | f_positive, important);

    case _font_size_:
      return add_length_property(name, val, font_size_strings, f_length_percentage | f_positive, important);

    case _margin_inline_:
      add_property(_margin_left_, value, baseurl, important, container, layer, specificity);
      add_property(_margin_right_, value, baseurl, important, container, layer, specificity);
      return;
    case _margin_inline_start_:
      add_property(_margin_left_, value, baseurl, important, container, layer, specificity);
      return;
    case _margin_inline_end_:
      add_property(_margin_right_, value, baseurl, important, container, layer, specificity);
      return;
    case _margin_block_:
      add_property(_margin_top_, value, baseurl, important, container, layer, specificity);
      add_property(_margin_bottom_, value, baseurl, important, container, layer, specificity);
      return;
    case _margin_block_start_:
      add_property(_margin_top_, value, baseurl, important, container, layer, specificity);
      return;
    case _margin_block_end_:
      add_property(_margin_bottom_, value, baseurl, important, container, layer, specificity);
      return;

    case _padding_inline_:
      add_property(_padding_left_, value, baseurl, important, container, layer, specificity);
      add_property(_padding_right_, value, baseurl, important, container, layer, specificity);
      return;
    case _padding_inline_start_:
      add_property(_padding_left_, value, baseurl, important, container, layer, specificity);
      return;
    case _padding_inline_end_:
      add_property(_padding_right_, value, baseurl, important, container, layer, specificity);
      return;
    case _padding_block_:
      add_property(_padding_top_, value, baseurl, important, container, layer, specificity);
      add_property(_padding_bottom_, value, baseurl, important, container, layer, specificity);
      return;
    case _padding_block_start_:
      add_property(_padding_top_, value, baseurl, important, container, layer, specificity);
      return;
    case _padding_block_end_:
      add_property(_padding_bottom_, value, baseurl, important, container, layer, specificity);
      return;

    case _inset_inline_:
      add_property(_left_, value, baseurl, important, container, layer, specificity);
      add_property(_right_, value, baseurl, important, container, layer, specificity);
      return;
    case _inset_inline_start_:
      add_property(_left_, value, baseurl, important, container, layer, specificity);
      return;
    case _inset_inline_end_:
      add_property(_right_, value, baseurl, important, container, layer, specificity);
      return;
    case _inset_block_:
      add_property(_top_, value, baseurl, important, container, layer, specificity);
      add_property(_bottom_, value, baseurl, important, container, layer, specificity);
      return;
    case _inset_block_start_:
      add_property(_top_, value, baseurl, important, container, layer, specificity);
      return;
    case _inset_block_end_:
      add_property(_bottom_, value, baseurl, important, container, layer, specificity);
      return;

    case _border_inline_:
      add_property(_border_left_, value, baseurl, important, container, layer, specificity);
      add_property(_border_right_, value, baseurl, important, container, layer, specificity);
      return;
    case _border_inline_width_:
      add_property(_border_left_width_, value, baseurl, important, container, layer, specificity);
      add_property(_border_right_width_, value, baseurl, important, container, layer, specificity);
      return;
    case _border_inline_style_:
      add_property(_border_left_style_, value, baseurl, important, container, layer, specificity);
      add_property(_border_right_style_, value, baseurl, important, container, layer, specificity);
      return;
    case _border_inline_color_:
      add_property(_border_left_color_, value, baseurl, important, container, layer, specificity);
      add_property(_border_right_color_, value, baseurl, important, container, layer, specificity);
      return;
    case _border_block_:
      add_property(_border_top_, value, baseurl, important, container, layer, specificity);
      add_property(_border_bottom_, value, baseurl, important, container, layer, specificity);
      return;
    case _border_block_width_:
      add_property(_border_top_width_, value, baseurl, important, container, layer, specificity);
      add_property(_border_bottom_width_, value, baseurl, important, container, layer, specificity);
      return;
    case _border_block_style_:
      add_property(_border_top_style_, value, baseurl, important, container, layer, specificity);
      add_property(_border_bottom_style_, value, baseurl, important, container, layer, specificity);
      return;
    case _border_block_color_:
      add_property(_border_top_color_, value, baseurl, important, container, layer, specificity);
      add_property(_border_bottom_color_, value, baseurl, important, container, layer, specificity);
      return;

    case _inset_:
      if (int n = parse_1234_lengths(value, len, f_length_percentage, "auto"))
        add_four_properties(_top_, len, n, important);
      return;

    case _margin_:
      if (int n = parse_1234_lengths(value, len, f_length_percentage, "auto"))
        add_four_properties(_margin_top_, len, n, important);
      break;

    case _padding_:
      if (int n = parse_1234_lengths(value, len, f_length_percentage | f_positive))
        add_four_properties(_padding_top_, len, n, important);
      break;

    case _color_:
      if (ident == "currentcolor")
        return inherit_property(name, important);
    case _background_color_:
    case _border_top_color_:
    case _border_bottom_color_:
    case _border_left_color_:
    case _border_right_color_:
      if (parse_color(val, *clr, container))
        add_parsed_property(name, property_value(*clr, important, false, m_layer, m_specificity));
      break;

    case _background_:
      parse_background(value, baseurl, important, container);
      break;

    case _background_image_:
      parse_background_image(value, baseurl, important, container);
      break;

    case _background_position_:
      parse_background_position(value, important);
      break;

    case _background_size_:
      parse_background_size(value, important);
      break;

    case _background_repeat_:
    case _background_attachment_:
    case _background_origin_:
    case _background_clip_:
      parse_keyword_comma_list(name, value, important);
      break;

    case _border_:
      parse_border(value, important, container);
      break;

    case _border_left_:
    case _border_right_:
    case _border_top_:
    case _border_bottom_:
      parse_border_side(name, value, important, container);
      break;

    case _border_width_:
      if (int n = parse_1234_values(value, len, parse_border_width))
        add_four_properties(_border_top_width_, len, n, important);
      break;
    case _border_style_:
      if (int n = parse_1234_values(value, idx, parse_keyword, (string)border_style_strings, 0))
        add_four_properties(_border_top_style_, idx, n, important);
      break;
    case _border_color_:
      if (int n = parse_1234_values(value, clr, parse_color, container))
        add_four_properties(_border_top_color_, clr, n, important);
      break;

    case _border_top_width_:
    case _border_bottom_width_:
    case _border_left_width_:
    case _border_right_width_:
      if (parse_border_width(val, *len))
        add_parsed_property(name, property_value(*len, important, false, m_layer, m_specificity));
      break;

    case _border_bottom_left_radius_:
    case _border_bottom_right_radius_:
    case _border_top_right_radius_:
    case _border_top_left_radius_:
      if (parse_two_lengths(value, len, f_length_percentage | f_positive))
      {
        add_parsed_property(_id(_s(name) + "-x"), property_value(len[0], important, false, m_layer, m_specificity));
        add_parsed_property(_id(_s(name) + "-y"), property_value(len[1], important, false, m_layer, m_specificity));
      }
      break;

    case _border_radius_x_:
    case _border_radius_y_:
    {
      string_id top_left = name == _border_radius_x_ ? _border_top_left_radius_x_ : _border_top_left_radius_y_;

      if (int n = parse_1234_lengths(value, len, f_length_percentage | f_positive))
        add_four_properties(top_left, len, n, important);
      break;
    }

    case _border_radius_:
      parse_border_radius(value, important);
      break;

    case _border_spacing_:
      if (parse_two_lengths(value, len, f_length | f_positive))
      {
        add_parsed_property(__litehtml_border_spacing_x_, property_value(len[0], important, false, m_layer, m_specificity));
        add_parsed_property(__litehtml_border_spacing_y_, property_value(len[1], important, false, m_layer, m_specificity));
      }
      break;

    case _list_style_image_:
      if (string url; parse_list_style_image(val, url))
      {
        add_parsed_property(_list_style_image_, property_value(url, important, false, m_layer, m_specificity));
        add_parsed_property(_list_style_image_baseurl_, property_value(baseurl, important, false, m_layer, m_specificity));
      }
      break;

    case _list_style_:
      parse_list_style(value, baseurl, important);
      break;

    case _font_:
      parse_font(value, important);
      break;

    case _font_family_:
      if (parse_font_family(value, str))
        add_parsed_property(name, property_value(str, important, false, m_layer, m_specificity));
      break;

    case _font_weight_:
      if (parse_font_weight(val, *len))
        add_parsed_property(name, property_value(*len, important, false, m_layer, m_specificity));
      break;

    case _text_decoration_:
      parse_text_decoration(value, important, container);
      break;

    case _text_underline_offset_:
      add_length_property(name, val, "auto", f_length_percentage, important);
      break;

    case _text_decoration_thickness_:
      add_length_property(name, val, style_text_decoration_thickness_strings, f_length_percentage | f_positive, important);
      break;

    case _text_decoration_color_:
      parse_text_decoration_color(val, important, container);
      break;

    case _text_decoration_line_:
      parse_text_decoration_line(value, important);
      break;

    case _text_emphasis_:
      parse_text_emphasis(value, important, container);
      break;

    case _text_emphasis_style_:
      str = get_repr(value, 0, -1, true);
      add_parsed_property(name, property_value(str, important, false, m_layer, m_specificity));
      break;

    case _text_emphasis_color_:
      parse_text_emphasis_color(val, important, container);
      break;

    case _text_emphasis_position_:
      parse_text_emphasis_position(value, important);
      break;

    case _flex_:
      parse_flex(value, important);
      break;

    case _opacity_:
    {
      css_length length;
      if (length.from_token(val, f_number | f_percentage))
      {
        float opacity = 1.0f;
        if (length.units() == css_units_percentage)
          opacity = length.val() / 100.0f;
        else
          opacity = length.val();
        if (opacity < 0)
          opacity = 0;
        if (opacity > 1)
          opacity = 1;
        add_parsed_property(name, property_value(opacity, important, false, m_layer, m_specificity));
      }
      break;
    }

    case _flex_grow_:
    case _flex_shrink_:
      if (val.type == NUMBER && val.n.number >= 0)
        add_parsed_property(name, property_value(val.n.number, important, false, m_layer, m_specificity));
      break;

    case _flex_basis_:
      add_length_property(name, val, flex_basis_strings, f_length_percentage | f_positive, important);
      break;

    case _flex_flow_:
      parse_flex_flow(value, important);
      break;

    case _align_items_:
    case _align_self_:
      parse_align_self(name, value, important);
      break;

    case _column_gap_:
    case _row_gap_:
      return add_length_property(name, val, "normal", f_length_percentage | f_positive, important);

    case _gap_:
      if (parse_two_lengths(value, len, f_length_percentage | f_positive))
      {
        add_parsed_property(_row_gap_, property_value(len[0], important, false, m_layer, m_specificity));
        add_parsed_property(_column_gap_, property_value(len[1], important, false, m_layer, m_specificity));
      }
      break;

    case _text_shadow_:
    case _box_shadow_:
      parse_shadow(name, value, important, container);
      break;

    case _line_clamp_:
    case __webkit_line_clamp_:
    case _order_:
      if (val.type == NUMBER && val.n.number_type == css_number_integer)
        add_parsed_property(name, property_value((int)val.n.number, important, false, m_layer, m_specificity));
      break;

    case _counter_increment_:
    case _counter_reset_:
    {
      string_vector strings;
      for (const auto &tok : value)
        strings.push_back(tok.get_repr(true));
      add_parsed_property(name, property_value(strings, important, false, m_layer, m_specificity));
      break;
    }

    case _content_:
      str = get_repr(value, 0, -1, true);
      add_parsed_property(name, property_value(str, important, false, m_layer, m_specificity));
      break;

    case _cursor_:
      str = get_repr(value, 0, -1, true);
      add_parsed_property(name, property_value(str, important, false, m_layer, m_specificity));
      break;

    case _aspect_ratio_:
      parse_aspect_ratio(value, important);
      break;

    case _margin_trim_:
    case __webkit_hyphens_:
    case __moz_orient_:
    case __webkit_appearance_:
    case _contain_intrinsic_size_:
      add_parsed_property(name, property_value(get_repr(value), important, false, m_layer, m_specificity));
      break;

    default:
      if (_s(name).substr(0, 2) == "--" && _s(name).size() >= 3 &&
          (value.empty() || is_declaration_value(value)))
        add_parsed_property(name, property_value(value, important, false, m_layer, m_specificity));
    }
  }

  void style::add_property(string_id name, const string &value, const string &baseurl, bool important, document_container *container, int layer, selector_specificity specificity)
  {
    auto tokens = normalize(value, f_componentize | f_remove_whitespace);
    add_property(name, tokens, baseurl, important, container, layer, specificity);
  }

  bool style::parse_list_style_image(const css_token &tok, string &url)
  {
    if (tok.ident() == "none")
    {
      url = "";
      return true;
    }
    return parse_url(tok, url);
  }

  void style::parse_list_style(const css_token_vector &tokens, string baseurl, bool important)
  {
    int type = list_style_type_disc;
    int position = list_style_position_outside;
    string image = "";

    bool type_found = false;
    bool position_found = false;
    bool image_found = false;
    int none_count = 0;

    for (const auto &token : tokens)
    {
      if (token.ident() == "none")
      {
        none_count++;
        continue;
      }
      if (!type_found && parse_keyword(token, type, list_style_type_strings))
        type_found = true;
      else if (!position_found && parse_keyword(token, position, list_style_position_strings))
        position_found = true;
      else if (!image_found && parse_list_style_image(token, image))
        image_found = true;
      else
        return;
    }

    switch (none_count)
    {
    case 0:
      break;
    case 1:
      if (type_found && image_found)
        return;
      if (!type_found)
        type = list_style_type_none;
      break;
    case 2:
      if (type_found || image_found)
        return;
      type = list_style_type_none;
      break;
    default:
      return;
    }

    add_parsed_property(_list_style_type_, property_value(type, important, false, m_layer, m_specificity));
    add_parsed_property(_list_style_position_, property_value(position, important, false, m_layer, m_specificity));
    add_parsed_property(_list_style_image_, property_value(image, important, false, m_layer, m_specificity));
    add_parsed_property(_list_style_image_baseurl_, property_value(baseurl, important, false, m_layer, m_specificity));
  }

  void style::parse_border_radius(const css_token_vector &tokens, bool important)
  {
    int i;
    for (i = 0; i < (int)tokens.size() && tokens[i].ch != '/'; i++)
    {
    }

    if (i == (int)tokens.size())
    {
      css_length len[4];
      if (int n = parse_1234_lengths(tokens, len, f_length_percentage | f_positive))
      {
        add_four_properties(_border_top_left_radius_x_, len, n, important);
        add_four_properties(_border_top_left_radius_y_, len, n, important);
      }
    }
    else
    {
      auto raduis_x = slice(tokens, 0, i);
      auto raduis_y = slice(tokens, i + 1);

      css_length rx[4], ry[4];
      int n = parse_1234_lengths(raduis_x, rx, f_length_percentage | f_positive);
      int m = parse_1234_lengths(raduis_y, ry, f_length_percentage | f_positive);

      if (n && m)
      {
        add_four_properties(_border_top_left_radius_x_, rx, n, important);
        add_four_properties(_border_top_left_radius_y_, ry, m, important);
      }
    }
  }

  bool parse_border_width(const css_token &token, css_length &w)
  {
    css_length width;
    if (!width.from_token(token, f_length | f_positive, border_width_strings))
      return false;
    if (width.is_predefined())
      width.set_value(border_width_values[width.predef()], css_units_px);
    w = width;
    return true;
  }

  bool parse_border_helper(const css_token_vector &tokens, document_container *container,
                           css_length &width, border_style &style, web_color &color)
  {
    css_length _width = border_width_medium_value;
    border_style _style = border_style_none;
    web_color _color = web_color::current_color;

    bool width_found = false;
    bool style_found = false;
    bool color_found = false;

    for (const auto &token : tokens)
    {
      if (!width_found && parse_border_width(token, _width))
        width_found = true;
      else if (!style_found && parse_keyword(token, _style, border_style_strings))
        style_found = true;
      else if (!color_found && parse_color(token, _color, container))
        color_found = true;
      else
        return false;
    }

    width = _width;
    style = _style;
    color = _color;
    return true;
  }

  void style::parse_border(const css_token_vector &tokens, bool important, document_container *container)
  {
    css_length width;
    border_style style;
    web_color color;

    if (!parse_border_helper(tokens, container, width, style, color))
      return;

    for (auto name : {_border_left_width_, _border_right_width_, _border_top_width_, _border_bottom_width_})
      add_parsed_property(name, property_value(width, important, false, m_layer, m_specificity));

    for (auto name : {_border_left_style_, _border_right_style_, _border_top_style_, _border_bottom_style_})
      add_parsed_property(name, property_value(style, important, false, m_layer, m_specificity));

    for (auto name : {_border_left_color_, _border_right_color_, _border_top_color_, _border_bottom_color_})
      add_parsed_property(name, property_value(color, important, false, m_layer, m_specificity));
  }

  void style::parse_border_side(string_id name, const css_token_vector &tokens, bool important, document_container *container)
  {
    css_length width;
    border_style style;
    web_color color;

    if (!parse_border_helper(tokens, container, width, style, color))
      return;

    add_parsed_property(_id(_s(name) + "-width"), property_value(width, important, false, m_layer, m_specificity));
    add_parsed_property(_id(_s(name) + "-style"), property_value(style, important, false, m_layer, m_specificity));
    add_parsed_property(_id(_s(name) + "-color"), property_value(color, important, false, m_layer, m_specificity));
  }

  bool parse_length(const css_token &tok, css_length &length, int options, string keywords)
  {
    return length.from_token(tok, options, keywords);
  }

  bool parse_two_lengths(const css_token_vector &tokens, css_length len[2], int options)
  {
    auto n = tokens.size();
    if (n != 1 && n != 2)
      return false;

    css_length a, b;
    if (!a.from_token(tokens[0], options))
      return false;
    if (n == 1)
      b = a;
    if (n == 2 && !b.from_token(tokens[1], options))
      return false;

    len[0] = a;
    len[1] = b;
    return true;
  }

  template <class T, class... Args>
  int parse_1234_values(const css_token_vector &tokens, T result[4], bool (*parse)(const css_token &, T &, Args...), Args... args)
  {
    if (tokens.size() > 4)
      return 0;
    for (size_t i = 0; i < tokens.size(); i++)
    {
      if (!parse(tokens[i], result[i], args...))
        return 0;
    }
    return (int)tokens.size();
  }

  int parse_1234_lengths(const css_token_vector &tokens, css_length len[4], int options, string keywords)
  {
    return parse_1234_values(tokens, len, parse_length, options, keywords);
  }

  template <class T>
  void style::add_four_properties(string_id top_name, T val[4], int n, bool important)
  {
    string_id top = top_name;
    string_id right = string_id(top_name + 1);
    string_id bottom = string_id(top_name + 2);
    string_id left = string_id(top_name + 3);

    add_parsed_property(top, property_value(val[0], important, false, m_layer, m_specificity));
    add_parsed_property(right, property_value(val[n > 1], important, false, m_layer, m_specificity));
    add_parsed_property(bottom, property_value(val[n / 3 * 2], important, false, m_layer, m_specificity));
    add_parsed_property(left, property_value(val[n / 2 + n / 4], important, false, m_layer, m_specificity));
  }

  void style::parse_background(const css_token_vector &tokens, const string &baseurl, bool important, document_container *container)
  {
    auto layers = parse_comma_separated_list(tokens);
    if (layers.empty())
      return;

    web_color color;
    std::vector<image> images;
    length_vector x_positions, y_positions;
    size_vector sizes;
    int_vector repeats, attachments, origins, clips;

    for (size_t i = 0; i < layers.size(); i++)
    {
      background bg;
      if (!parse_bg_layer(layers[i], container, bg, i == layers.size() - 1))
        return;

      color = bg.m_color;
      images.push_back(bg.m_image[0]);
      x_positions.push_back(bg.m_position_x[0]);
      y_positions.push_back(bg.m_position_y[0]);
      sizes.push_back(bg.m_size[0]);
      repeats.push_back(bg.m_repeat[0]);
      attachments.push_back(bg.m_attachment[0]);
      origins.push_back(bg.m_origin[0]);
      clips.push_back(bg.m_clip[0]);
    }

    add_parsed_property(_background_color_, property_value(color, important, false, m_layer, m_specificity));
    add_parsed_property(_background_image_, property_value(images, important, false, m_layer, m_specificity));
    add_parsed_property(_background_image_baseurl_, property_value(baseurl, important, false, m_layer, m_specificity));
    add_parsed_property(_background_position_x_, property_value(x_positions, important, false, m_layer, m_specificity));
    add_parsed_property(_background_position_y_, property_value(y_positions, important, false, m_layer, m_specificity));
    add_parsed_property(_background_size_, property_value(sizes, important, false, m_layer, m_specificity));
    add_parsed_property(_background_repeat_, property_value(repeats, important, false, m_layer, m_specificity));
    add_parsed_property(_background_attachment_, property_value(attachments, important, false, m_layer, m_specificity));
    add_parsed_property(_background_origin_, property_value(origins, important, false, m_layer, m_specificity));
    add_parsed_property(_background_clip_, property_value(clips, important, false, m_layer, m_specificity));
  }

  bool style::parse_bg_layer(const css_token_vector &tokens, document_container *container, background &bg, bool final_layer)
  {
    bg.m_color = web_color::transparent;
    bg.m_image = {{}};
    bg.m_position_x = {css_length(0, css_units_percentage)};
    bg.m_position_y = {css_length(0, css_units_percentage)};
    bg.m_size = {css_size(css_length::predef_value(background_size_auto), css_length::predef_value(background_size_auto))};
    bg.m_repeat = {background_repeat_repeat};
    bg.m_attachment = {background_attachment_scroll};
    bg.m_origin = {background_box_padding};
    bg.m_clip = {background_box_border};

    bool color_found = false;
    bool image_found = false;
    bool position_found = false;
    bool repeat_found = false;
    bool attachment_found = false;
    bool origin_found = false;
    bool clip_found = false;

    for (int i = 0; i < (int)tokens.size(); i++)
    {
      if (!color_found && final_layer && parse_color(tokens[i], bg.m_color, container))
        color_found = true;
      else if (!image_found && parse_bg_image(tokens[i], bg.m_image[0], container))
        image_found = true;
      else if (!position_found && parse_bg_position_size(tokens, i, bg.m_position_x[0], bg.m_position_y[0], bg.m_size[0]))
        position_found = true, i--;
      else if (!repeat_found && parse_keyword(tokens[i], bg.m_repeat[0], background_repeat_strings))
        repeat_found = true;
      else if (!attachment_found && parse_keyword(tokens[i], bg.m_attachment[0], background_attachment_strings))
        attachment_found = true;
      else if (!origin_found && parse_keyword(tokens[i], bg.m_origin[0], background_box_strings))
        origin_found = true, bg.m_clip[0] = bg.m_origin[0];
      else if (!clip_found && parse_keyword(tokens[i], bg.m_clip[0], background_box_strings))
        clip_found = true;
      else
        return false;
    }
    return true;
  }

  bool parse_bg_position_size(const css_token_vector &tokens, int &index, css_length &x, css_length &y, css_size &size)
  {
    if (!parse_bg_position(tokens, index, x, y, true))
      return false;
    if (at(tokens, index).ch != '/')
      return true;
    if (!parse_bg_size(tokens, ++index, size))
    {
      index--;
      return false;
    }
    return true;
  }

  bool parse_bg_size(const css_token_vector &tokens, int &index, css_size &size)
  {
    css_length a, b;
    if (!a.from_token(at(tokens, index), f_length_percentage | f_positive, background_size_strings))
      return false;
    if (a.is_predefined() && a.predef() != background_size_auto)
    {
      size.width = size.height = a;
      index++;
      return true;
    }
    if (b.from_token(at(tokens, index + 1), f_length_percentage | f_positive, "auto"))
      index += 2;
    else
    {
      b.predef(background_size_auto);
      index++;
    }
    size.width = a;
    size.height = b;
    return true;
  }

  bool is_one_of_predef(const css_length &x, int idx1, int idx2)
  {
    return x.is_predefined() && is_one_of(x.predef(), idx1, idx2);
  }

  bool parse_bg_position(const css_token_vector &tokens, int &index, css_length &x, css_length &y, bool convert_keywords_to_percents)
  {
    enum
    {
      left = background_position_left,
      right = background_position_right,
      top = background_position_top,
      bottom = background_position_bottom,
      center = background_position_center
    };

    css_length a, b;
    if (!a.from_token(at(tokens, index), f_length_percentage, background_position_strings))
      return false;
    if (!b.from_token(at(tokens, index + 1), f_length_percentage, background_position_strings))
    {
      b.predef(center);
      if (is_one_of_predef(a, top, bottom))
        swap(a, b);
      index++;
    }
    else
    {
      if ((is_one_of_predef(a, top, bottom) && b.is_predefined()) ||
          (a.is_predefined() && is_one_of_predef(b, left, right)))
        swap(a, b);
      if (is_one_of_predef(a, top, bottom) || is_one_of_predef(b, left, right))
        return false;
      index += 2;
    }

    if (convert_keywords_to_percents)
    {
      if (a.is_predefined())
        a.set_value(background_position_percentages[a.predef()], css_units_percentage);
      if (b.is_predefined())
        b.set_value(background_position_percentages[b.predef()], css_units_percentage);
    }
    x = a;
    y = b;
    return true;
  }

  void style::parse_background_image(const css_token_vector &tokens, const string &baseurl, bool important, document_container *container)
  {
    auto layers = parse_comma_separated_list(tokens);
    if (layers.empty())
      return;
    std::vector<image> images;
    for (const auto &layer : layers)
    {
      image image;
      if (layer.size() != 1)
        return;
      if (!parse_bg_image(layer[0], image, container))
        return;
      images.push_back(image);
    }
    add_parsed_property(_background_image_, property_value(images, important, false, m_layer, m_specificity));
    add_parsed_property(_background_image_baseurl_, property_value(baseurl, important, false, m_layer, m_specificity));
  }

  bool parse_bg_image(const css_token &tok, image &bg_image, document_container *container)
  {
    if (tok.ident() == "none")
    {
      bg_image.type = image::type_none;
      return true;
    }
    string url;
    if (parse_url(tok, url))
    {
      bg_image.type = image::type_url;
      bg_image.url = url;
      return true;
    }
    if (parse_gradient(tok, bg_image.m_gradient, container))
    {
      bg_image.type = image::type_gradient;
      return true;
    }
    return false;
  }

  bool parse_url(const css_token &tok, string &url)
  {
    if (tok.type == URL)
    {
      url = trim(tok.str);
      return true;
    }
    if (tok.type == CV_FUNCTION && is_one_of(lowcase(tok.name), "url", "src") &&
        tok.value.size() == 1 && tok.value[0].type == STRING)
    {
      url = trim(tok.value[0].str);
      return true;
    }
    return false;
  }

  void style::parse_keyword_comma_list(string_id name, const css_token_vector &tokens, bool important)
  {
    auto layers = parse_comma_separated_list(tokens);
    if (layers.empty())
      return;
    int_vector vec;
    for (const auto &layer : layers)
    {
      int idx;
      if (layer.size() != 1)
        return;
      if (!parse_keyword(layer[0], idx, m_valid_values[name]))
        return;
      vec.push_back(idx);
    }
    add_parsed_property(name, property_value(vec, important, false, m_layer, m_specificity));
  }

  void style::parse_background_position(const css_token_vector &tokens, bool important)
  {
    auto layers = parse_comma_separated_list(tokens);
    if (layers.empty())
      return;
    length_vector x_positions, y_positions;
    for (const auto &layer : layers)
    {
      css_length x, y;
      int index = 0;
      if (!parse_bg_position(layer, index, x, y, true) || index != (int)layer.size())
        return;
      x_positions.push_back(x);
      y_positions.push_back(y);
    }
    add_parsed_property(_background_position_x_, property_value(x_positions, important, false, m_layer, m_specificity));
    add_parsed_property(_background_position_y_, property_value(y_positions, important, false, m_layer, m_specificity));
  }

  void style::parse_background_size(const css_token_vector &tokens, bool important)
  {
    auto layers = parse_comma_separated_list(tokens);
    if (layers.empty())
      return;
    size_vector sizes;
    for (const auto &layer : layers)
    {
      css_size size;
      int index = 0;
      if (!parse_bg_size(layer, index, size) || index != (int)layer.size())
        return;
      sizes.push_back(size);
    }
    add_parsed_property(_background_size_, property_value(sizes, important, false, m_layer, m_specificity));
  }

  bool parse_font_weight(const css_token &tok, css_length &weight)
  {
    if (int idx = value_index(tok.ident(), font_weight_strings); idx >= 0)
    {
      weight.predef(idx);
      return true;
    }
    if (tok.type == NUMBER && tok.n.number >= 1 && tok.n.number <= 1000)
    {
      weight.set_value(tok.n.number, css_units_none);
      return true;
    }
    return false;
  }

  bool parse_font_style_variant_weight(const css_token_vector &tokens, int &index,
                                       int &style, int &variant, css_length &weight)
  {
    bool style_found = false;
    bool variant_found = false;
    bool weight_found = false;
    bool res = false;
    int i = index, count = 0;
    while (i < (int)tokens.size() && count++ < 3)
    {
      const auto &tok = tokens[i++];
      if (tok.ident() == "normal")
      {
        index++;
        res = true;
      }
      else if (!style_found && parse_keyword(tok, style, font_style_strings))
      {
        style_found = true;
        index++;
        res = true;
      }
      else if (!variant_found && parse_keyword(tok, variant, font_variant_strings))
      {
        variant_found = true;
        index++;
        res = true;
      }
      else if (!weight_found && parse_font_weight(tok, weight))
      {
        weight_found = true;
        index++;
        res = true;
      }
      else
        break;
    }
    return res;
  }

  bool is_custom_ident(const css_token &tok)
  {
    if (tok.type != IDENT)
      return false;
    return !is_one_of(lowcase(tok.name), "default", "initial", "inherit", "unset");
  }

  bool parse_font_family(const css_token_vector &tokens, string &font_family)
  {
    auto list = parse_comma_separated_list(tokens);
    if (list.empty())
      return false;
    string result;
    for (const auto &name : list)
    {
      if (name.size() == 1 && name[0].type == STRING)
      {
        result += name[0].str + ',';
        continue;
      }
      string str;
      for (const auto &tok : name)
      {
        if (!is_custom_ident(tok))
          return false;
        str += tok.name + ' ';
      }
      result += trim(str) + ',';
    }
    result.resize(result.size() - 1);
    font_family = result;
    return true;
  }

  void style::parse_font(css_token_vector tokens, bool important)
  {
    int style = font_style_normal;
    int variant = font_variant_normal;
    css_length weight = css_length::predef_value(font_weight_normal);
    css_length size = css_length::predef_value(font_size_medium);
    css_length line_height = css_length::predef_value(line_height_normal);
    string font_family;

    if (tokens.size() == 1 && (tokens[0].type == STRING || tokens[0].type == IDENT) && value_in_list(tokens[0].str, font_system_family_name_strings))
    {
      font_family = tokens[0].str;
    }
    else
    {
      int index = 0;
      parse_font_style_variant_weight(tokens, index, style, variant, weight);
      if (!size.from_token(at(tokens, index), f_length_percentage | f_positive, font_size_strings))
        return;
      index++;
      if (at(tokens, index).ch == '/')
      {
        index++;
        if (!line_height.from_token(at(tokens, index), f_number | f_length_percentage, line_height_strings))
          return;
        index++;
      }
      remove(tokens, 0, index);
      if (!parse_font_family(tokens, font_family))
        return;
    }
    add_parsed_property(_font_style_, property_value(style, important, false, m_layer, m_specificity));
    add_parsed_property(_font_variant_, property_value(variant, important, false, m_layer, m_specificity));
    add_parsed_property(_font_weight_, property_value(weight, important, false, m_layer, m_specificity));
    add_parsed_property(_font_size_, property_value(size, important, false, m_layer, m_specificity));
    add_parsed_property(_line_height_, property_value(line_height, important, false, m_layer, m_specificity));
    add_parsed_property(_font_family_, property_value(font_family, important, false, m_layer, m_specificity));
  }

  void style::parse_text_decoration(const css_token_vector &tokens, bool important, document_container *container)
  {
    css_length len;
    css_token_vector line_tokens;
    for (const auto &token : tokens)
    {
      if (parse_text_decoration_color(token, important, container))
        continue;
      if (parse_length(token, len, f_length_percentage | f_positive, style_text_decoration_thickness_strings))
      {
        add_parsed_property(_text_decoration_thickness_, property_value(len, important, false, m_layer, m_specificity));
      }
      else
      {
        if (token.type == IDENT)
        {
          int style = value_index(token.ident(), style_text_decoration_style_strings);
          if (style >= 0)
          {
            add_parsed_property(_text_decoration_style_, property_value(style, important, false, m_layer, m_specificity));
          }
          else
            line_tokens.push_back(token);
        }
        else
          line_tokens.push_back(token);
      }
    }
    if (!line_tokens.empty())
      parse_text_decoration_line(line_tokens, important);
  }

  bool style::parse_text_decoration_color(const css_token &token, bool important, document_container *container)
  {
    web_color _color;
    if (parse_color(token, _color, container))
    {
      add_parsed_property(_text_decoration_color_, property_value(_color, important, false, m_layer, m_specificity));
      return true;
    }
    if (token.type == IDENT && value_in_list(token.ident(), "auto;currentcolor"))
    {
      add_parsed_property(_text_decoration_color_, property_value(web_color::current_color, important, false, m_layer, m_specificity));
      return true;
    }
    return false;
  }

  void style::parse_text_decoration_line(const css_token_vector &tokens, bool important)
  {
    int val = 0;
    for (const auto &token : tokens)
    {
      if (token.type == IDENT)
      {
        int idx = value_index(token.ident(), style_text_decoration_line_strings);
        if (idx >= 0)
          val |= 1 << (idx - 1);
      }
    }
    add_parsed_property(_text_decoration_line_, property_value(val, important, false, m_layer, m_specificity));
  }

  void style::parse_aspect_ratio(const css_token_vector &tokens, bool important)
  {
    if (tokens.size() == 1 && tokens[0].ident() == "auto")
    {
      add_parsed_property(_aspect_ratio_, property_value(aspect_ratio(), important, false, m_layer, m_specificity));
      return;
    }

    float w = 1, h = 1;
    bool w_found = false, h_found = false;
    bool auto_found = false;

    for (size_t i = 0; i < tokens.size(); i++)
    {
      if (tokens[i].ident() == "auto")
      {
        auto_found = true;
      }
      else if (tokens[i].type == NUMBER)
      {
        if (!w_found)
        {
          w = tokens[i].n.number;
          w_found = true;
        }
        else if (!h_found)
        {
          if (i > 0 && tokens[i - 1].ch == '/')
          {
            h = tokens[i].n.number;
            h_found = true;
          }
          else
            return;
        }
        else
          return;
      }
      else if (tokens[i].ch == '/')
      {
        if (!w_found || h_found)
          return;
      }
      else
        return;
    }

    if (w_found)
    {
      add_parsed_property(_aspect_ratio_, property_value(aspect_ratio(w, h, auto_found), important, false, m_layer, m_specificity));
    }
  }

  void style::parse_text_emphasis(const css_token_vector &tokens, bool important, document_container *container)
  {
    string style;
    for (const auto &token : std::vector(tokens.rbegin(), tokens.rend()))
    {
      if (parse_text_emphasis_color(token, important, container))
        continue;
      style.insert(0, token.str + " ");
    }
    style = trim(style);
    if (!style.empty())
      add_parsed_property(_text_emphasis_style_, property_value(style, important, false, m_layer, m_specificity));
  }

  bool style::parse_text_emphasis_color(const css_token &token, bool important, document_container *container)
  {
    web_color _color;
    if (parse_color(token, _color, container))
    {
      add_parsed_property(_text_emphasis_color_, property_value(_color, important, false, m_layer, m_specificity));
      return true;
    }
    if (token.type == IDENT && value_in_list(token.ident(), "auto;currentcolor"))
    {
      add_parsed_property(_text_emphasis_color_, property_value(web_color::current_color, important, false, m_layer, m_specificity));
      return true;
    }
    return false;
  }

  void style::parse_text_emphasis_position(const css_token_vector &tokens, bool important)
  {
    int val = 0;
    for (const auto &token : tokens)
    {
      if (token.type == IDENT)
      {
        int idx = value_index(token.ident(), style_text_emphasis_position_strings);
        if (idx >= 0)
          val |= 1 << (idx - 1);
      }
    }
    add_parsed_property(_text_emphasis_position_, property_value(val, important, false, m_layer, m_specificity));
  }

  void style::parse_flex(const css_token_vector &tokens, bool important)
  {
    auto n = tokens.size();
    if (n > 3)
      return;
    const auto &a = at(tokens, 0);
    const auto &b = at(tokens, 1);
    const auto &c = at(tokens, 2);

    struct flex
    {
      float m_grow = 1;
      float m_shrink = 1;
      css_length m_basis = 0;
      bool grow(const css_token &tok)
      {
        if (tok.type != NUMBER || tok.n.number < 0)
          return false;
        m_grow = tok.n.number;
        return true;
      }
      bool shrink(const css_token &tok)
      {
        if (tok.type != NUMBER || tok.n.number < 0)
          return false;
        m_shrink = tok.n.number;
        return true;
      }
      bool basis(const css_token &tok, bool unitless_zero_allowed = false)
      {
        if (!unitless_zero_allowed && tok.type == NUMBER && tok.n.number == 0)
          return false;
        return m_basis.from_token(tok, f_length_percentage | f_positive, flex_basis_strings);
      }
    };
    flex flex;

    if (n == 1)
    {
      string_id ident = _id(a.ident());
      if (is_one_of(ident, _initial_, _auto_, _none_))
      {
        css_length _auto = css_length::predef_value(flex_basis_auto);
        switch (ident)
        {
        case _initial_:
          flex = {0, 1, _auto};
          break;
        case _auto_:
          flex = {1, 1, _auto};
          break;
        case _none_:
          flex = {0, 0, _auto};
          break;
        default:;
        }
      }
      else
      {
        if (!(flex.grow(a) || flex.basis(a)))
          return;
      }
    }
    else if (n == 2)
    {
      if (!((flex.grow(a) && (flex.shrink(b) || flex.basis(b))) || (flex.basis(a) && flex.grow(b))))
        return;
    }
    else
    {
      if (!((flex.grow(a) && flex.shrink(b) && flex.basis(c, true)) || (flex.basis(a) && flex.grow(b) && flex.shrink(c))))
        return;
    }
    add_parsed_property(_flex_grow_, property_value(flex.m_grow, important, false, m_layer, m_specificity));
    add_parsed_property(_flex_shrink_, property_value(flex.m_shrink, important, false, m_layer, m_specificity));
    add_parsed_property(_flex_basis_, property_value(flex.m_basis, important, false, m_layer, m_specificity));
  }

  void style::parse_flex_flow(const css_token_vector &tokens, bool important)
  {
    int flex_direction = flex_direction_row;
    int flex_wrap = flex_wrap_nowrap;
    bool direction_found = false;
    bool wrap_found = false;
    for (const auto &token : tokens)
    {
      if (!direction_found && parse_keyword(token, flex_direction, flex_direction_strings))
        direction_found = true;
      else if (!wrap_found && parse_keyword(token, flex_wrap, flex_wrap_strings))
        wrap_found = true;
      else
        return;
    }
    add_parsed_property(_flex_direction_, property_value(flex_direction, important, false, m_layer, m_specificity));
    add_parsed_property(_flex_wrap_, property_value(flex_wrap, important, false, m_layer, m_specificity));
  }

  void style::parse_align_self(string_id name, const css_token_vector &tokens, bool important)
  {
    auto n = tokens.size();
    if (n > 2)
      return;
    if (tokens[0].type != IDENT || (n == 2 && tokens[1].type != IDENT))
      return;
    string a = tokens[0].ident();
    if (name == _align_items_ && a == "auto")
      return;
    if (n == 1)
    {
      int idx = value_index(a, flex_align_items_strings);
      if (idx >= 0)
        add_parsed_property(name, property_value(idx, important, false, m_layer, m_specificity));
      return;
    }
    string b = tokens[1].ident();
    if (a == "baseline")
      swap(a, b);
    if (b == "baseline" && is_one_of(a, "first", "last"))
    {
      int idx = flex_align_items_baseline | (a == "first" ? flex_align_items_first : flex_align_items_last);
      add_parsed_property(name, property_value(idx, important, false, m_layer, m_specificity));
      return;
    }
    int idx = value_index(b, self_position_strings);
    if (idx >= 0 && is_one_of(a, "safe", "unsafe"))
    {
      idx |= (a == "safe" ? flex_align_items_safe : flex_align_items_unsafe);
      add_parsed_property(name, property_value(idx, important, false, m_layer, m_specificity));
    }
  }

  void style::add_parsed_property(string_id name, const property_value &propval)
  {
    auto prop = m_properties.find(name);
    if (prop != m_properties.end())
    {
      if (!prop->second.m_important)
      {
          if (propval.m_important)
          {
              prop->second = propval;
          }
          else if (propval.m_layer > prop->second.m_layer)
          {
              prop->second = propval;
          }
          else if (propval.m_layer == prop->second.m_layer)
          {
              if (propval.m_specificity >= prop->second.m_specificity)
                  prop->second = propval;
          }
      }
      else if (propval.m_important)
      {
          if (propval.m_layer < prop->second.m_layer)
          {
              prop->second = propval;
          }
          else if (propval.m_layer == prop->second.m_layer)
          {
              if (propval.m_specificity >= prop->second.m_specificity)
                  prop->second = propval;
          }
      }
    }
    else
      m_properties[name] = propval;
  }

  void style::remove_property(string_id name, bool important)
  {
    auto prop = m_properties.find(name);
    if (prop != m_properties.end())
    {
      if (!prop->second.m_important || (important && prop->second.m_important))
        m_properties.erase(prop);
    }
  }

  void style::combine(const style &src, selector_specificity specificity)
  {
    for (const auto &property : src.m_properties)
    {
        property_value val = property.second;
        if (specificity != selector_specificity())
        {
            val.m_specificity = specificity;
        }
        add_parsed_property(property.first, val);
    }
  }

  const property_value &style::get_property(string_id name) const
  {
    auto it = m_properties.find(name);
    if (it != m_properties.end())
      return it->second;
    static property_value dummy;
    return dummy;
  }

  bool check_var_syntax(const css_token_vector &args)
  {
    if (args.empty())
      return false;
    string name = args[0].ident();
    if (name.substr(0, 2) != "--" || name.size() <= 2)
      return false;
    if (args.size() > 1 && args[1].ch != ',')
      return false;
    if (args.size() > 2 && !is_declaration_value(args, 2))
      return false;
    return true;
  }

  bool evaluate_calc(css_token_vector &tokens, const html_tag *el);

  bool subst_var(css_token_vector &tokens, const html_tag *el, std::set<string_id> &used_vars)
  {
    for (int i = 0; i < (int)tokens.size(); i++)
    {
      auto &tok = tokens[i];
      if (tok.type == CV_FUNCTION && lowcase(tok.name) == "var")
      {
        auto args = tok.value;
        if (!check_var_syntax(args))
          return false;
        auto name = _id(args[0].name);
        if (name in used_vars)
          return false;
        used_vars.insert(name);
        css_token_vector value;
        if (el->get_custom_property(name, value))
        {
          remove(tokens, i);
          insert(tokens, i, value);
        }
        else
        {
          if (args.size() == 1)
            return false;
          remove(args, 0, 2);
          remove(tokens, i);
          insert(tokens, i, args);
        }
        return true;
      }
      if (tok.is_component_value() && subst_var(tok.value, el, used_vars))
        return true;
    }
    return false;
  }

  bool evaluate_calc(css_token_vector &tokens, const html_tag *el)
  {
    bool changed = false;
    for (int i = 0; i < (int)tokens.size(); i++)
    {
      auto &tok = tokens[i];
      if (tok.type == CV_FUNCTION && (lowcase(tok.name) == "calc" || lowcase(tok.name) == "min" || lowcase(tok.name) == "max" || lowcase(tok.name) == "clamp"))
      {
        evaluate_calc(tok.value, el);

        string func_name = lowcase(tok.name);

        // Remove whitespace for evaluation
        css_token_vector val;
        for (const auto& t : tok.value) if (t.type != WHITESPACE) val.push_back(t);

        if (func_name == "calc")
        {
          if (val.size() == 1 && (val[0].type == DIMENSION || val[0].type == NUMBER || val[0].type == PERCENTAGE))
          {
            css_token inner = val[0];
            remove(tokens, i);
            insert(tokens, i, {inner});
            changed = true;
            continue;
          }

          if (val.size() == 3)
          {
            const auto& left = val[0];
            const auto& op = val[1];
            const auto& right = val[2];

            if (left.type == DIMENSION && right.type == NUMBER && op.ch == '*')
            {
              css_token result = left;
              result.n.number *= right.n.number;
              remove(tokens, i);
              insert(tokens, i, {result});
              changed = true;
              continue;
            }
            if (left.type == NUMBER && right.type == DIMENSION && op.ch == '*')
            {
              css_token result = right;
              result.n.number *= left.n.number;
              remove(tokens, i);
              insert(tokens, i, {result});
              changed = true;
              continue;
            }
            if (left.type == DIMENSION && right.type == NUMBER && op.ch == '/' && right.n.number != 0)
            {
              css_token result = left;
              result.n.number /= right.n.number;
              remove(tokens, i);
              insert(tokens, i, {result});
              changed = true;
              continue;
            }
            if (left.type == DIMENSION && right.type == DIMENSION && left.unit == right.unit && (op.ch == '+' || op.ch == '-'))
            {
              css_token result = left;
              if (op.ch == '+') result.n.number += right.n.number;
              else result.n.number -= right.n.number;
              remove(tokens, i);
              insert(tokens, i, {result});
              changed = true;
              continue;
            }
            if (left.type == NUMBER && right.type == NUMBER && (op.ch == '+' || op.ch == '-' || op.ch == '*' || op.ch == '/'))
            {
              css_token result = left;
              if (op.ch == '+') result.n.number += right.n.number;
              else if (op.ch == '-') result.n.number -= right.n.number;
              else if (op.ch == '*') result.n.number *= right.n.number;
              else if (op.ch == '/' && right.n.number != 0) result.n.number /= right.n.number;
              remove(tokens, i);
              insert(tokens, i, {result});
              changed = true;
              continue;
            }
          }
        }
        else if (func_name == "min" || func_name == "max")
        {
          auto args = parse_comma_separated_list(val);
          bool all_same = true;
          if (!args.empty())
          {
            css_token first = args[0][0];
            for (const auto& arg : args)
            {
              if (arg.size() != 1 || arg[0].type != first.type || (arg[0].type == DIMENSION && lowcase(arg[0].unit) != lowcase(first.unit)))
              {
                all_same = false;
                break;
              }
            }
            if (all_same)
            {
              css_token result = first;
              for (const auto& arg : args)
              {
                if (func_name == "min") result.n.number = min(result.n.number, arg[0].n.number);
                else result.n.number = max(result.n.number, arg[0].n.number);
              }
              remove(tokens, i);
              insert(tokens, i, {result});
              changed = true;
              continue;
            }
          }
        }
        else if (func_name == "clamp" && val.size() >= 5)
        {
          auto args = parse_comma_separated_list(val);
          if (args.size() == 3)
          {
            bool all_same = true;
            css_token first = args[0][0];
            for (const auto& arg : args)
            {
              if (arg.size() != 1 || arg[0].type != first.type || (arg[0].type == DIMENSION && lowcase(arg[0].unit) != lowcase(first.unit)))
              {
                all_same = false;
                break;
              }
            }
            if (all_same)
            {
              css_token result = first;
              float min_v = args[0][0].n.number;
              float val_v = args[1][0].n.number;
              float max_v = args[2][0].n.number;
              result.n.number = max(min_v, min(val_v, max_v));
              remove(tokens, i);
              insert(tokens, i, {result});
              changed = true;
              continue;
            }
          }
        }
      }
      if (tok.is_component_value())
      {
        if (evaluate_calc(tok.value, el)) changed = true;
      }
    }
    return changed;
  }

  void subst_vars_(string_id name, css_token_vector &tokens, const html_tag *el)
  {
    std::set<string_id> used_vars = {name};
    while (subst_var(tokens, el, used_vars))
      ;
    evaluate_calc(tokens, el);
  }

  void style::subst_vars(const html_tag *el)
  {
    for (auto &prop : m_properties)
    {
      if (prop.second.m_has_var)
      {
        auto &value = prop.second.get<css_token_vector>();
        subst_vars_(prop.first, value, el);
        add_property(prop.first, value, "", prop.second.m_important, el->get_document()->container(), prop.second.m_layer, prop.second.m_specificity);
      }
    }
  }

  void style::parse_shadow(string_id name, const css_token_vector &tokens, bool important, document_container *container)
  {
    shadow_vector shadows;
    css_token_vector shadow_tokens;
    for (size_t i = 0; i <= tokens.size(); ++i)
    {
      if (i == tokens.size() || (tokens[i].type == COMMA))
      {
        if (!shadow_tokens.empty())
        {
          shadow s;
          int lengths_count = 0;
          for (const auto &tok : shadow_tokens)
          {
            if (tok.ident() == "inset")
              s.inset = true;
            else if (parse_color(tok, s.color, container))
            {
            }
            else
            {
              css_length len;
              if (len.from_token(tok, f_length))
              {
                if (lengths_count == 0)
                  s.x = len;
                else if (lengths_count == 1)
                  s.y = len;
                else if (lengths_count == 2)
                  s.blur = len;
                else if (lengths_count == 3)
                  s.spread = len;
                lengths_count++;
              }
            }
          }
          if (lengths_count >= 2)
            shadows.push_back(s);
          shadow_tokens.clear();
        }
      }
      else
        shadow_tokens.push_back(tokens[i]);
    }
    if (!shadows.empty())
      add_parsed_property(name, property_value(shadows, important, false, m_layer, m_specificity));
  }
} // namespace litehtml
