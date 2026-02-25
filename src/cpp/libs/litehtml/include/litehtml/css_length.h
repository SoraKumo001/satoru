#ifndef LH_CSS_LENGTH_H
#define LH_CSS_LENGTH_H

#include "types.h"
#include "css_tokenizer.h"

namespace litehtml
{
	// from_token options (flags)
	enum {
		f_length     = 1, // <length> (includes unitless zero)
		f_percentage = 2, // <percentage>
		f_length_percentage = f_length | f_percentage, // <length-percentage>
		f_number     = 4, // <number>
		f_integer    = 8, // <integer>
		f_positive   = 16 // ⩾0 (non-negative)
	};

	// <length> | <percentage> | <number> | auto | none | normal
	// https://developer.mozilla.org/en-US/docs/Web/CSS/length
	class css_length
	{
		union
		{
			float	m_value;
			int		m_predef;
		};
		css_units	m_units;
		bool		m_is_predefined;
		float		m_px;
		float		m_percent;
		float		m_rem;
		bool		m_is_calc;

		enum math_op
		{
			op_none,
			op_min,
			op_max,
			op_clamp
		};
		math_op m_op = op_none;
		std::vector<css_length> m_operands;

	public:
		css_length();
		css_length(float val, css_units units = css_units_px);
		css_length&	operator=(float val);

		bool		is_predefined() const;
		void		predef(int val);
		int			predef() const;
		static css_length predef_value(int val = 0);
		void		set_value(float val, css_units units);
		float		val() const;
		css_units	units() const;
		pixel_t		calc_percent(pixel_t width) const;
		bool		from_token(const css_token& token, int options, const string& predefined_keywords = "");
		string		to_string() const;

		void        set_calc(float px, float percent, float rem) { m_px = px; m_percent = percent; m_rem = rem; m_is_calc = true; m_is_predefined = false; m_op = op_none; m_operands.clear(); }
		void        set_math(math_op op, std::vector<css_length>&& operands) { m_px = 0; m_percent = 0; m_rem = 0; m_op = op; m_operands = std::move(operands); m_is_calc = true; m_is_predefined = false; }
		bool        is_calc() const { return m_is_calc; }
		float       calc_px() const { return m_px; }
		float       calc_percent() const { return m_percent; }
		float       calc_rem() const { return m_rem; }
	};

	using length_vector = vector<css_length>;

	// css_length inlines

	inline css_length::css_length()
	{
		m_value			= 0;
		m_predef		= 0;
		m_units			= css_units_none;
		m_is_predefined	= false;
		m_px            = 0;
		m_percent       = 0;
		m_rem           = 0;
		m_is_calc       = false;
		m_op            = op_none;
	}

	inline css_length::css_length(float val, css_units units)
	{
		m_value = val;
		m_units = units;
		m_is_predefined = false;
		m_px            = 0;
		m_percent       = 0;
		m_rem           = 0;
		m_is_calc       = false;
		m_op            = op_none;
	}

	inline css_length&	css_length::operator=(float val)
	{
		m_value = val;
		m_units = css_units_px;
		m_is_predefined = false;
		m_px            = 0;
		m_percent       = 0;
		m_rem           = 0;
		m_is_calc       = false;
		m_op            = op_none;
		m_operands.clear();
		return *this;
	}

	inline bool css_length::is_predefined() const
	{ 
		return m_is_predefined;					
	}

	inline void css_length::predef(int val)		
	{ 
		m_predef		= val; 
		m_is_predefined = true;	
	}

	inline int css_length::predef() const
	{ 
		if(m_is_predefined)
		{
			return m_predef; 
		}
		return 0;
	}

	inline void css_length::set_value(float val, css_units units)
	{ 
		m_value			= val; 
		m_is_predefined = false;	
		m_units			= units;
	}

	inline float css_length::val() const
	{
		if(!m_is_predefined)
		{
			return m_value;
		}
		return 0;
	}

	inline css_units css_length::units() const
	{
		return m_units;
	}

	inline pixel_t css_length::calc_percent(pixel_t width) const
	{
		if(!is_predefined())
		{
			if (m_is_calc)
			{
				if (m_op == op_none)
				{
					return (pixel_t)(m_px + (width * m_percent / 100.0) + (m_rem * 16.0)); // Temporary 16.0 for rem
				}
				else if (m_op == op_min && !m_operands.empty())
				{
					pixel_t res = m_operands[0].calc_percent(width);
					for (size_t i = 1; i < m_operands.size(); i++)
					{
						res = std::min(res, m_operands[i].calc_percent(width));
					}
					return res;
				}
				else if (m_op == op_max && !m_operands.empty())
				{
					pixel_t res = m_operands[0].calc_percent(width);
					for (size_t i = 1; i < m_operands.size(); i++)
					{
						res = std::max(res, m_operands[i].calc_percent(width));
					}
					return res;
				}
				else if (m_op == op_clamp && m_operands.size() >= 3)
				{
					pixel_t min_v = m_operands[0].calc_percent(width);
					pixel_t val = m_operands[1].calc_percent(width);
					pixel_t max_v = m_operands[2].calc_percent(width);
					return std::max(min_v, std::min(val, max_v));
				}
				return (pixel_t)(m_px + (width * m_percent / 100.0) + (m_rem * 16.0));
			}
			if(units() == css_units_percentage)
			{
				return (pixel_t) (width * m_value / 100.0);
			} else if (units() == css_units_rem)
			{
				return (pixel_t) (m_value * 16.0); // Simple fallback for now
			} else
			{
				return (pixel_t) val();
			}
		}
		return 0;
	}
}

#endif  // LH_CSS_LENGTH_H
