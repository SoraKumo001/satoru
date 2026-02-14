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
		m_layers.push_back("");
		return (int)m_layers.size() - 1;
	}

	// Handle dot-separated names to ensure ancestors exist
	size_t dot_pos = name.find('.');
	if (dot_pos != string::npos)
	{
		get_layer_id(name.substr(0, dot_pos));
	}

	auto it = std::find(m_layers.begin(), m_layers.end(), name);
	if (it != m_layers.end())
	{
		return (int)std::distance(m_layers.begin(), it);
	}

	m_layers.push_back(name);
	return (int)m_layers.size() - 1;
}

} // namespace litehtml
