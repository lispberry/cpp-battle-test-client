#include <Core/Events/Tags.hpp>

#include <algorithm>
#include <typeindex>

namespace sw
{
	bool TagSet::has(const std::type_index tag) const
	{
		return std::ranges::find(_tags, tag) != _tags.end();
	}
}
