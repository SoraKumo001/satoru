#include "html.h"
#include "css_length.h"
#include "css_parser.h"

namespace litehtml
{

bool css_length::from_token(const css_token& token, int options, const string& keywords)
{
	if (token.type == CV_FUNCTION && lowcase(token.name) == "calc")
	{
		float px = 0;
		float percent = 0;
		bool ok = true;

		// Simplified calc parser for components: sums of numbers/dimensions/percentages
		// Tailwind v4: calc(100% - 20px)
		for (size_t i = 0; i < token.value.size(); i++)
		{
			const auto& t = token.value[i];
			if (t.type == WHITESPACE) continue;
			
			float sign = 1;
			if (t.ch == '-') sign = -1;
			else if (t.ch == '+') sign = 1;

			const css_token* val_tok = &t;
			if (t.ch == '-' || t.ch == '+')
			{
				i++;
				while (i < token.value.size() && token.value[i].type == WHITESPACE) i++;
				if (i >= token.value.size()) { ok = false; break; }
				val_tok = &token.value[i];
			}

			if (val_tok->type == DIMENSION)
			{
				// Assume all dimensions are converted to px via document::cvt_units if possible.
				// But here we don't have document. We'll handle px, rem, em etc. later if needed.
				// For now, if it's px, add to px.
				if (lowcase(val_tok->unit) == "px") px += val_tok->n.number * sign;
				else { /* ignore for now or fallback */ }
			}
			else if (val_tok->type == PERCENTAGE)
			{
				percent += val_tok->n.number * sign;
			}
			else if (val_tok->type == NUMBER)
			{
				px += val_tok->n.number * sign;
			}
		}

		if (ok)
		{
			set_calc(px, percent);
			return true;
		}
	}

	if ((options & f_positive) && is_one_of(token.type, NUMBER, DIMENSION, PERCENTAGE) && token.n.number < 0)
		return false;

	if (token.type == IDENT)
	{
		int idx = value_index(lowcase(token.name), keywords);
		if (idx == -1) return false;
		m_predef = idx;
		m_is_predefined = true;
		return true;
	}
	else if (token.type == DIMENSION)
	{
		if (!(options & f_length)) return false;

		int idx = value_index(lowcase(token.unit), css_units_strings);
		// note: 1none and 1\% are invalid
		if (idx == -1 || idx == css_units_none || idx == css_units_percentage)
			return false;
		
		m_value = token.n.number;
		m_units = (css_units)idx;
		m_is_predefined = false;
		return true;
	}
	else if (token.type == PERCENTAGE)
	{
		if (!(options & f_percentage)) return false;

		m_value = token.n.number;
		m_units = css_units_percentage;
		m_is_predefined = false;
		return true;
	}
	else if (token.type == NUMBER)
	{
		// if token is a nonzero number and neither f_number nor f_integer is specified in the options
		if (!(options & (f_number | f_integer)) && token.n.number != 0)
			return false;
		// if token is a zero number and neither of f_number, f_integer or f_length are specified in the options
		if (!(options & (f_number | f_integer | f_length)) && token.n.number == 0)
			return false;
		if ((options & f_integer) && token.n.number_type != css_number_integer)
			return false;
		
		m_value = token.n.number;
		m_units = css_units_none;
		m_is_predefined = false;
		return true;
	}
	return false;
}

string css_length::to_string() const
{
	if(m_is_predefined)
	{
		return "def(" + std::to_string(m_predef) + ")";
	}
	return std::to_string(m_value) + "{" + index_value(m_units, css_units_strings) + "}";
}

css_length css_length::predef_value(int val)
{
	css_length len;
	len.predef(val);
	return len;
}

} // namespace litehtml