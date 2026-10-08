#pragma once

#include <Core/Base/Concepts.hpp>

#include <typeindex>
#include <typeinfo>
#include <vector>

namespace sw
{
	/// The kinds of damage an ability deals, declared on the component that implements it. Other features react to a
	/// kind (`taken.is<common::Wound>()`), not to a particular ability, so they never name each other.
	///
	/// \code
	/// struct Rending
	/// {
	///     static constexpr Ability Name{"rending"};
	///     using Tags = TagList<common::Wound>;
	///     ...
	/// };
	/// \endcode
	template <ClassType... Tag>
	struct TagList
	{};

	/// The kinds of a particular hit's damage, at run time: what `effect::attack(..., ability)` reads from the ability's
	/// TagList and HitTaken shows its handlers.
	class TagSet
	{
	public:
		TagSet() = default;

		/// The kinds listed in `TagList<Tag...>`.
		template <ClassType... Tag>
		explicit TagSet(TagList<Tag...> /*list*/) :
				_tags{std::type_index(typeid(Tag))...}
		{}

		/// True if the damage is of kind `Tag`.
		template <ClassType Tag>
		[[nodiscard]]
		bool has() const
		{
			return has(typeid(Tag));
		}

		/// has<Tag>, by the kind's type.
		[[nodiscard]]
		bool has(std::type_index tag) const;

	private:
		std::vector<std::type_index> _tags;
	};
}
