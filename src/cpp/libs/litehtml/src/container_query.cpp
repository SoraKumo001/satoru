#include "html.h"
#include "container_query.h"
#include "css_parser.h"
#include "document.h"
#include "html_tag.h"
#include "render_item.h"

namespace litehtml
{

bool container_query::check(const container_features& features) const
{
	if (!m_condition) return true;
	
	// We need to map container_features to media_features to reuse media_condition::check
	media_features mf;
	mf.width = features.width;
	mf.height = features.height;
	// Other features don't apply to containers
	
	return m_condition->check(mf) == True;
}

bool container_query_list::check(const container_features& features) const
{
	if (empty()) return true;
	for (const auto& query : m_queries)
	{
		if (query.check(features)) return true;
	}
	return false;
}

bool container_query_list_list::check(const html_tag* el) const
{
	if (m_container_query_lists.empty()) return true;

	for (const auto& cq_list : m_container_query_lists)
	{
		bool list_match = false;
		for (const auto& query : cq_list.m_queries)
		{
			// Find matching container ancestor
			element::ptr parent = el->parent();
			bool found = false;
			while (parent)
			{
				const auto& css = parent->css();
				if (css.get_container_type() != container_type_none)
				{
					if (query.m_name.empty() || query.m_name == css.get_container_name())
					{
						container_features feat;
						auto ri = parent->get_render_item();
						if (ri)
						{
							// Use the content box size of the container
							feat.width = ri->pos().width - ri->get_paddings().width() - ri->get_borders().width();
							feat.height = ri->pos().height - ri->get_paddings().height() - ri->get_borders().height();
						}
						else
						{
							feat.width = 0;
							feat.height = 0;
						}
						
						if (query.check(feat))
						{
							list_match = true;
						}
						found = true;
						break;
					}
				}
				parent = parent->parent();
			}
			if (list_match) break;
			
			if (!found && !query.m_condition) 
			{
				// Bare @container matches if found
				// (Shouldn't really happen with valid queries)
			}
		}
		
		if (!list_match) return false;
	}

	return true;
}

// These are defined in media_query.cpp but not exported. 
// For now, I will implement a simplified version or reuse what's possible.
// Ideally, we should refactor media_query.cpp to export parsing functions for conditions.

media_query_list parse_media_query_list(const css_token_vector& tokens, shared_ptr<document> doc);

container_query_list parse_container_query_list(const css_token_vector& _tokens, shared_ptr<document> doc)
{
	css_token_vector tokens = normalize(_tokens, f_componentize | f_remove_whitespace);
	if (tokens.empty()) return {};

	container_query_list result;
	
	// @container <name>? ( <condition> )
	container_query query;
	int index = 0;
	if (tokens[index].type == IDENT)
	{
		query.m_name = tokens[index].ident();
		index++;
	}
	
	if (index < (int)tokens.size() && tokens[index].type == ROUND_BLOCK)
	{
		// Reuse media query parsing logic for the condition
		// We need to pass the ROUND_BLOCK token itself in a vector
		css_token_vector wrapper = { tokens[index] };
		media_query_list mql = parse_media_query_list(wrapper, doc);
		if (!mql.m_queries.empty() && !mql.m_queries[0].m_conditions.empty())
		{
			query.m_condition = make_shared<media_condition>(mql.m_queries[0].m_conditions[0]);
		}
	}
	
	result.m_queries.push_back(query);
	return result;
}

} // namespace litehtml
