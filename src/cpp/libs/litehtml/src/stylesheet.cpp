#include "html.h"
#include "stylesheet.h"
#include "css_parser.h"
#include "document.h"
#include "document_container.h"

namespace litehtml
{

// https://www.w3.org/TR/css-syntax-3/#parse-a-css-stylesheet
template<class Input> // Input == string or css_token_vector
void css::parse_css_stylesheet(const Input& input, string baseurl, document::ptr doc, media_query_list_list::ptr media, bool top_level, int layer, string layer_prefix)
{
	if (doc && media)
		doc->add_media_list(media);

	// To parse a CSS stylesheet, first parse a stylesheet.
	auto rules = css_parser::parse_stylesheet(input, top_level);
	bool import_allowed = top_level;

	// Interpret all of the resulting top-level qualified rules as style rules, defined below.
	// If any style rule is invalid, or any at-rule is not recognized or is invalid according
	// to its grammar or context, it's a parse error. Discard that rule.
	for (auto rule : rules)
	{
		if (rule->type == raw_rule::qualified)
		{
			if (parse_style_rule(rule, baseurl, doc, media, layer))
				import_allowed = false;
			continue;
		}

		// Otherwise: at-rule
		switch (_id(lowcase(rule->name)))
		{
		case _charset_: // ignored  https://www.w3.org/TR/css-syntax-3/#charset-rule
			break;

		case _import_:
			if (import_allowed)
				parse_import_rule(rule, baseurl, doc, media, layer, layer_prefix);
			else
				css_parse_error("incorrect placement of @import rule");
			break;

		// https://www.w3.org/TR/css-conditional-3/#at-media
		// @media <media-query-list> { <stylesheet> }
		case _media_:
		{
			if (rule->block.type != CURLY_BLOCK) break;
			auto new_media = media;
			auto mq_list = parse_media_query_list(rule->prelude, doc);
			// An empty media query list evaluates to true.  https://drafts.csswg.org/mediaqueries-5/#example-6f06ee45
			if (!mq_list.empty())
			{
				new_media = make_shared<media_query_list_list>(media ? *media : media_query_list_list());
				new_media->add(mq_list);
			}
			parse_css_stylesheet(rule->block.value, baseurl, doc, new_media, false, layer, layer_prefix);
			import_allowed = false;
			break;
		}

		case _layer_:
		{
			if (rule->block.type == CURLY_BLOCK)
			{
				string name;
				if (!rule->prelude.empty())
				{
					name = get_repr(rule->prelude);
					trim(name);
				}

				int new_layer_id;
				string new_layer_prefix = layer_prefix;
				if (!name.empty())
				{
					if (!new_layer_prefix.empty()) new_layer_prefix += ".";
					new_layer_prefix += name;
					new_layer_id = get_layer_id(new_layer_prefix);
				}
				else
				{
					new_layer_id = get_layer_id("");
				}
				parse_css_stylesheet(rule->block.value, baseurl, doc, media, false, new_layer_id, new_layer_prefix);
			}
			else
			{
				auto layer_list = parse_comma_separated_list(rule->prelude);
				for (const auto& tokens : layer_list)
				{
					string name = get_repr(tokens);
					trim(name);
					if (!name.empty())
					{
						string full_name = layer_prefix;
						if (!full_name.empty()) full_name += ".";
						full_name += name;
						get_layer_id(full_name);
					}
				}
			}
			import_allowed = false;
			break;
		}

		case _supports_:
		{
			if (rule->block.type != CURLY_BLOCK) break;
			if (evaluate_supports(rule->prelude, doc))
			{
				parse_css_stylesheet(rule->block.value, baseurl, doc, media, false, layer, layer_prefix);
			}
			import_allowed = false;
			break;
		}

		default:
			css_parse_error("unrecognized rule @" + rule->name);
		}
	}
}

// https://drafts.csswg.org/css-cascade-5/#at-import
// `supports` is not supported
// @import [ <url> | <string> ] [ layer | layer(<layer-name>) ]? <media-query-list>?
void css::parse_import_rule(raw_rule::ptr rule, string baseurl, document::ptr doc, media_query_list_list::ptr media, int layer, string layer_prefix)
{
	auto tokens = rule->prelude;
	int index = 0;
	skip_whitespace(tokens, index);
	auto tok = at(tokens, index);
	string url;
	auto parse_string = [](const css_token& tok, string& str)
	{
		if (tok.type != STRING) return false;
		str = tok.str;
		return true;
	};
	bool ok = parse_url(tok, url) || parse_string(tok, url);
	if (!ok) {
		css_parse_error("invalid @import rule");
		return;
	}

	int import_layer = layer;
	string import_layer_prefix = layer_prefix;

	index++;
	skip_whitespace(tokens, index);
	tok = at(tokens, index);
	if (tok.type == IDENT && lowcase(tok.name) == "layer")
	{
		import_layer = get_layer_id("");
		index++;
	}
	else if (tok.type == CV_FUNCTION && lowcase(tok.name) == "layer")
	{
		string name = get_repr(tok.value);
		trim(name);
		if (!import_layer_prefix.empty()) import_layer_prefix += ".";
		import_layer_prefix += name;
		import_layer = get_layer_id(import_layer_prefix);
		index++;
	}

	document_container* container = doc->container();
	string css_text;
	string css_baseurl = baseurl;
	container->import_css(css_text, url, css_baseurl);

	auto new_media = media;
	tokens = slice(tokens, index);
	auto mq_list = parse_media_query_list(tokens, doc);
	if (!mq_list.empty())
	{
		new_media = make_shared<media_query_list_list>(media ? *media : media_query_list_list());
		new_media->add(mq_list);
	}

	parse_css_stylesheet(css_text, css_baseurl, doc, new_media, true, import_layer, import_layer_prefix);
}

// https://www.w3.org/TR/css-syntax-3/#style-rules
bool css::parse_style_rule(raw_rule::ptr rule, string baseurl, document::ptr doc, media_query_list_list::ptr media, int layer)
{
	// The prelude of the qualified rule is parsed as a <selector-list>. If this returns failure, the entire style rule is invalid.
	auto list = parse_selector_list(rule->prelude, strict_mode, doc->mode());
	if (list.empty())
	{
		css_parse_error("invalid selector");
		return false;
	}

	style::ptr style = make_shared<litehtml::style>(); // style block
	// The content of the qualified rule's block is parsed as a style block's contents.
	style->add(rule->block.value, baseurl, doc->container(), layer);

	for (auto sel : list)
	{
		sel->m_style = style;
		sel->m_media_query = media;
		sel->calc_specificity();
		add_selector(sel, layer);
	}
	return true;
}

void css::sort_selectors()
{
	std::sort(m_selectors.begin(), m_selectors.end(),
		 [](const css_selector::ptr& v1, const css_selector::ptr& v2)
		 {
			 return (*v1) < (*v2);
		 }
	);
}

int css::get_layer_id(const string& name)
{
	if (name.empty())
	{
		return get_layer_id("__anon_" + std::to_string(m_anon_count++));
	}

	if (m_resolved_ranks.count(name)) return m_resolved_ranks[name];

	// Split name by '.'
	std::vector<string> segments;
	size_t start = 0;
	size_t end = name.find('.');
	while (end != string::npos)
	{
		segments.push_back(name.substr(start, end - start));
		start = end + 1;
		end = name.find('.', start);
	}
	segments.push_back(name.substr(start));

	string path = "";
	long long rank = 0;
	long long multiplier = 1000000;
	for (int i = 0; i < 3; ++i)
	{
	        string segment = (i < (int)segments.size()) ? segments[i] : "";
	        if (segment.empty())
	        {
	                // SPEC: sub-layers follow parent order.
	                // To make A.B > A, we treat missing sub-segments as having "highest" priority (999)
	                // wait, no. Specification says: "Unlayered styles have the highest priority"
	                // For layers: "later layers have higher priority".
	                // A { ... } is same as A { @layer { ... } } ? No.
	                // Actually, @layer A { .box { ... } } is "unlayered" styles INSIDE layer A.
	                // Styles inside @layer A but not in any sub-layer of A should have HIGHER priority
	                // than any styles in sub-layers of A (like @layer A.B).
	                // Reference: https://developer.mozilla.org/en-US/docs/Web/CSS/@layer#nesting_layers
	                // "styles not in a nested layer are collected together and placed after the nested layers"
	                // So: A.B < A (styles in A but not in sub-layer)
	                // My current code: rank = (A_order+1)*1M + (B_order+1)*1K + (0)
	                // If we want A.B < A, then A should be (A_order+1)*1M + (999)*1K + (999)
	                rank += 999 * multiplier;
                }
                else
                {
                        string full_segment_path = path.empty() ? segment : path + "." + segment;
                        if (m_segment_orders.find(full_segment_path) == m_segment_orders.end())
                        {
                                m_segment_orders[full_segment_path] = m_next_order[path]++;
                        }
                        rank += (long long)(m_segment_orders[full_segment_path]) * multiplier;
                        path = full_segment_path;
                }
                multiplier /= 1000;
        }

	m_resolved_ranks[name] = (int)rank;
	return (int)rank;
}

bool css::evaluate_supports(const css_token_vector& tokens, shared_ptr<document> doc)
{
	int index = 0;
	return evaluate_supports_condition(tokens, index, doc);
}

bool css::evaluate_supports_condition(const css_token_vector& tokens, int& index, shared_ptr<document> doc)
{
	skip_whitespace(tokens, index);
	if (index >= (int)tokens.size()) return false;

	const auto& tok = at(tokens, index);
	bool result = false;

	if (tok.type == IDENT && lowcase(tok.name) == "not")
	{
		index++;
		result = !evaluate_supports_condition(tokens, index, doc);
	}
	else if (tok.type == ROUND_BLOCK)
	{
		result = evaluate_supports_feature(tok, doc);
		index++;
	}
	else
	{
		return false;
	}

	while (true)
	{
		skip_whitespace(tokens, index);
		if (index >= (int)tokens.size()) break;

		const auto& next_tok = at(tokens, index);
		if (next_tok.type == IDENT)
		{
			string op = lowcase(next_tok.name);
			if (op == "and")
			{
				index++;
				result = evaluate_supports_condition(tokens, index, doc) && result;
			}
			else if (op == "or")
			{
				index++;
				result = evaluate_supports_condition(tokens, index, doc) || result;
			}
			else break;
		}
		else break;
	}

	return result;
}

bool css::evaluate_supports_feature(const css_token& block, shared_ptr<document> doc)
{
	if (block.type != ROUND_BLOCK) return false;

	// Check if it's a nested condition
	if (!block.value.empty() && (block.value[0].type == ROUND_BLOCK || (block.value[0].type == IDENT && (lowcase(block.value[0].name) == "not" || lowcase(block.value[0].name) == "and" || lowcase(block.value[0].name) == "or"))))
	{
		int index = 0;
		return evaluate_supports_condition(block.value, index, doc);
	}

	// selector(...) function
	if (!block.value.empty() && block.value[0].type == CV_FUNCTION && lowcase(block.value[0].name) == "selector")
	{
		// We assume all selectors are supported if they parse
		auto list = parse_selector_list(block.value[0].value, strict_mode, doc->mode());
		return !list.empty();
	}

	// (property: value)
	css_token_vector tokens = block.value;
	int index = 0;
	skip_whitespace(tokens, index);
	if (index >= (int)tokens.size() || at(tokens, index).type != IDENT) return false;

	string prop_name = lowcase(at(tokens, index).name);
	index++;
	skip_whitespace(tokens, index);
	if (index >= (int)tokens.size() || at(tokens, index).ch != ':') return false;
	index++;

	css_token_vector value_tokens = slice(tokens, index);
	// In litehtml, trim_whitespace is not public in css_parser.cpp but we can use normalize
	value_tokens = normalize(value_tokens, f_remove_whitespace);

	if (value_tokens.empty()) return false;

	// Check if property is known
	string_id id = _id(prop_name);
	if (id == empty_id) return false;

	// For now, if we have a property ID, we consider it supported.
	// In a real browser, we would check if the value is also valid for that property.
	// Satoru has a fixed set of supported properties in style.cpp.
	
	style st;
	st.add_property(id, value_tokens, "", false, doc->container());
	return !st.get_property(id).is<invalid>();
}

} // namespace litehtml
