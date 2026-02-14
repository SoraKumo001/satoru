#ifndef LH_STYLESHEET_H
#define LH_STYLESHEET_H

#include "css_selector.h"
#include "css_tokenizer.h"

namespace litehtml
{

// https://www.w3.org/TR/cssom-1/#css-declarations
struct raw_declaration
{
	using vector = std::vector<raw_declaration>;

	string name; // property name
	css_token_vector value = {}; // default value is specified here to get rid of gcc warning "missing initializer for member"
	bool important = false;

	operator bool() const { return name != ""; }
};

// intermediate half-parsed rule that is used internally by the parser
class raw_rule
{
public:
	using ptr = shared_ptr<raw_rule>;
	using vector = std::vector<ptr>;

	enum rule_type { qualified, at };

	raw_rule(rule_type type, string name = "") : type(type), name(name) {}

	rule_type type;
	// An at-rule has a name, a prelude consisting of a list of component values, and an optional block consisting of a simple {} block.
	string name;
	// https://www.w3.org/TR/css-syntax-3/#qualified-rule
	// A qualified rule has a prelude consisting of a list of component values, and a block consisting of a simple {} block.
	// Note: Most qualified rules will be style rules, where the prelude is a selector and the block a list of declarations.
	css_token_vector prelude;
	css_token block;
};

class css
{
	css_selector::vector	m_selectors;
	std::map<string, int>	m_resolved_ranks;
	std::map<string, int>	m_segment_orders;
	std::map<string, int>	m_next_order;
	int						m_anon_count = 0;
public:
	static const int unlayered_id = 2000000000;

	const css_selector::vector& selectors() const
	{
		return m_selectors;
	}

	template<class Input>
	void	parse_css_stylesheet(const Input& input, string baseurl, shared_ptr<document> doc, media_query_list_list::ptr media = nullptr, bool top_level = true, int layer = unlayered_id, string layer_prefix = "");

	void	sort_selectors();

private:
	bool	parse_style_rule(raw_rule::ptr rule, string baseurl, shared_ptr<document> doc, media_query_list_list::ptr media, int layer);
	void	parse_import_rule(raw_rule::ptr rule, string baseurl, shared_ptr<document> doc, media_query_list_list::ptr media, int layer, string layer_prefix);
	void	add_selector(const css_selector::ptr& selector, int layer);
	int		get_layer_id(const string& name);
	void	parse_property_rule(raw_rule::ptr rule, shared_ptr<document> doc);
	bool	evaluate_supports(const css_token_vector& tokens, shared_ptr<document> doc);
	bool	evaluate_supports_condition(const css_token_vector& tokens, int& index, shared_ptr<document> doc);
	bool	evaluate_supports_feature(const css_token& token, shared_ptr<document> doc);
};

inline void css::add_selector(const css_selector::ptr& selector, int layer)
{
	selector->m_order = (int)m_selectors.size();
	selector->m_layer = layer;
	m_selectors.push_back(selector);
}


} // namespace litehtml

#endif  // LH_STYLESHEET_H
