#pragma once

#include <Core/Model/Types.hpp>
#include <Core/Units/AnyUnit.hpp>

#include <concepts>
#include <cstddef>
#include <memory>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sw
{
	/// Owns every unit. Keeps creation order: it is the order units act in, and the order of every query, so a seed
	/// reproduces a battle exactly. A dead unit stays until the Simulation removes the dead at the end of the round, so
	/// the order never changes while a round is played and loops walk it directly. Feature code reads units through
	/// UnitRef instead.
	///
	/// \code
	/// UnitRegistry registry;
	/// registry.insert(makeUnit<Swordsman>(UnitId{3}, kit));
	/// for (const UnitId id : registry.order())
	/// {
	///     const auto* unit = registry.find(id);
	/// }
	/// registry.removeIf([](const AnyUnit& unit) { return unit.id() == UnitId{3}; });
	/// \endcode
	class UnitRegistry
	{
	public:
		/// Takes ownership of `unit` and appends it to the order. Requires an id not in the registry; not checked.
		void insert(std::unique_ptr<AnyUnit> unit);

		/// Destroys every unit `remove` selects and drops it from the order, in one pass.
		template <std::predicate<const AnyUnit&> P>
		void removeIf(P remove)
		{
			std::erase_if(
					_order,
					[this, &remove](const UnitId id)
					{
						const auto found = _units.find(id);
						if (!remove(std::as_const(*found->second.unit)))
						{
							return false;
						}
						_units.erase(found);
						return true;
					});
		}

		[[nodiscard]]
		bool contains(UnitId id) const;

		/// Unit `id`, or nullptr.
		[[nodiscard]]
		AnyUnit* find(UnitId id);

		[[nodiscard]]
		const AnyUnit* find(UnitId id) const;

		/// Ids in creation order. Invalidated by insert and removeIf.
		[[nodiscard]]
		std::span<const UnitId> order() const;

		/// Orders ids by creation, without searching the order: true if `left` was inserted before `right`. Both must
		/// be in the registry.
		[[nodiscard]]
		bool createdBefore(UnitId left, UnitId right) const;

	private:
		struct Entry
		{
			std::unique_ptr<AnyUnit> unit;
			std::size_t rank;
		};

		std::vector<UnitId> _order;
		std::unordered_map<UnitId, Entry> _units;
		std::size_t _inserted{0};
	};
}
