#ifndef LH_CONTAINER_QUERY_H
#define LH_CONTAINER_QUERY_H

#include "css_tokenizer.h"
#include "string_id.h"
#include "types.h"
#include "media_query.h"

namespace litehtml
{
	class html_tag;
	struct container_condition;

	struct container_query
	{
		string						m_name;
		shared_ptr<media_condition>	m_condition;

		bool check(const container_features& features) const;
	};

	struct container_query_list
	{
		vector<container_query> m_queries;
		bool empty() const { return m_queries.empty(); }
		bool check(const container_features& features) const;
	};

	container_query_list parse_container_query_list(const css_token_vector& tokens, shared_ptr<document> doc);

	class container_query_list_list
	{
	public:
		using ptr    = shared_ptr<container_query_list_list>;
		using vector = std::vector<ptr>;
	private:
		std::vector<container_query_list>	m_container_query_lists;
	public:
		void add(const container_query_list& cq_list)
		{
			m_container_query_lists.push_back(cq_list);
		}

		bool check(const html_tag* el) const;
	};
}

#endif  // LH_CONTAINER_QUERY_H
