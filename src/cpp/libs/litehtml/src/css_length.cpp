#include "html.h"
#include "css_length.h"
#include "css_parser.h"

namespace litehtml
{

namespace
{
	struct calc_value
	{
		float px = 0;
		float percent = 0;
		float rem = 0;

		calc_value& operator+=(const calc_value& other) { px += other.px; percent += other.percent; rem += other.rem; return *this; }
		calc_value& operator-=(const calc_value& other) { px -= other.px; percent -= other.percent; rem -= other.rem; return *this; }
		calc_value& operator*=(float scale) { px *= scale; percent *= scale; rem *= scale; return *this; }
		calc_value& operator/=(float div) { if (div != 0) { px /= div; percent /= div; rem /= div; } return *this; }
	};

	calc_value parse_calc_expression(const css_token_vector& tokens, size_t& i);

	calc_value parse_calc_primary(const css_token_vector& tokens, size_t& i)
	{
		while (i < tokens.size() && tokens[i].type == WHITESPACE) i++;
		if (i >= tokens.size()) return {};

		const auto& t = tokens[i];
		if (t.type == ROUND_BLOCK)
		{
			i++;
			size_t sub_i = 0;
			return parse_calc_expression(t.value, sub_i);
		}

		calc_value res;
		if (t.type == DIMENSION)
		{
			string unit = lowcase(t.unit);
			if (unit == "px") res.px = t.n.number;
			else if (unit == "rem") res.rem = t.n.number;
			else if (unit == "em") res.px = t.n.number * 16.0f; // Temporary fallback
			// For now only px and rem are supported without document context in calc
		}
		else if (t.type == PERCENTAGE)
		{
			res.percent = t.n.number;
		}
		else if (t.type == NUMBER)
		{
			res.px = t.n.number;
		}
		i++;
		return res;
	}

	calc_value parse_calc_multiplicative(const css_token_vector& tokens, size_t& i)
	{
		calc_value res = parse_calc_primary(tokens, i);
		while (i < tokens.size())
		{
			while (i < tokens.size() && tokens[i].type == WHITESPACE) i++;
			if (i >= tokens.size()) break;
			if (tokens[i].type == '*')
			{
				i++;
				calc_value next = parse_calc_primary(tokens, i);
				if (res.px != 0 || res.percent != 0) res *= next.px;
				else res = next;
			}
			else if (tokens[i].type == '/')
			{
				i++;
				calc_value next = parse_calc_primary(tokens, i);
				if (next.px != 0) res /= next.px;
			}
			else break;
		}
		return res;
	}

	calc_value parse_calc_expression(const css_token_vector& tokens, size_t& i)
	{
		calc_value res = parse_calc_multiplicative(tokens, i);
		while (i < tokens.size())
		{
			while (i < tokens.size() && tokens[i].type == WHITESPACE) i++;
			if (i >= tokens.size()) break;
			if (tokens[i].type == '+')
			{
				i++;
				res += parse_calc_multiplicative(tokens, i);
			}
			else if (tokens[i].type == '-')
			{
				i++;
				res -= parse_calc_multiplicative(tokens, i);
			}
			else break;
		}
		return res;
	}
}

bool css_length::from_token(const css_token& token, int options, const string& keywords)
{
	if (token.type == CV_FUNCTION)
	{
		string name = lowcase(token.name);
		if (name == "calc")
		{
			size_t i = 0;
			calc_value val = parse_calc_expression(token.value, i);
			set_calc(val.px, val.percent, val.rem);
			return true;
		}
		else if (name == "min" || name == "max" || name == "clamp")
		{
			std::vector<css_length> operands;
			css_token_vector current_operand;
			for (const auto& t : token.value)
			{
				if (t.type == ',')
				{
					if (!current_operand.empty())
					{
						css_length op;
						// Use normalize to componentize and remove whitespace if needed
						// But simpler here: just check if it's a single value or calc
						if (current_operand.size() == 1)
						{
							if (op.from_token(current_operand[0], options, keywords))
								operands.push_back(op);
						}
						else
						{
							// Support calc/nested math inside min/max
							// For simplicity, we could try to parse it as calc if it's multiple tokens
							// or wrap it in a pseudo-calc token
							css_token pseudo(CV_FUNCTION, "calc");
							pseudo.value = current_operand;
							if (op.from_token(pseudo, options, keywords))
								operands.push_back(op);
						}
						current_operand.clear();
					}
				}
				else if (t.type != WHITESPACE)
				{
					current_operand.push_back(t);
				}
			}
			if (!current_operand.empty())
			{
				css_length op;
				if (current_operand.size() == 1)
				{
					if (op.from_token(current_operand[0], options, keywords))
						operands.push_back(op);
				}
				else
				{
					css_token pseudo(CV_FUNCTION, "calc");
					pseudo.value = current_operand;
					if (op.from_token(pseudo, options, keywords))
						operands.push_back(op);
				}
			}

			if (!operands.empty())
			{
				if (name == "min") set_math(op_min, std::move(operands));
				else if (name == "max") set_math(op_max, std::move(operands));
				else if (name == "clamp") set_math(op_clamp, std::move(operands));
				return true;
			}
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

		m_value = token.n.number;
		m_units = css_units_none;
		m_is_predefined = false;
		return true;
	}

	return false;
}

css_length css_length::predef_value(int val)
{
	css_length res;
	res.predef(val);
	return res;
}

string css_length::to_string() const
{
	if (is_predefined()) return "";
	if (m_is_calc)
	{
		if (m_op == op_min) return "min(...)";
		if (m_op == op_max) return "max(...)";
		if (m_op == op_clamp) return "clamp(...)";
		return "calc(" + std::to_string(m_px) + "px + " + std::to_string(m_percent) + "% + " + std::to_string(m_rem) + "rem)";
	}
	if (m_units == css_units_percentage) return std::to_string(m_value) + "%";
	return std::to_string(m_value) + "px";
}

} // namespace litehtml
