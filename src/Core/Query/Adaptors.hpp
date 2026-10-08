#pragma once

#include <Core/Base/Concepts.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Query/Units.hpp>
#include <Core/Units/Unit.hpp>
#include <Core/World/UnitRef.hpp>

namespace sw
{
	// Game vocabulary for unit queries. Each adaptor is one `where` over a UnitRef question; the rules themselves
	// (who may be attacked, how far apart units are) live in UnitRef and the event hooks. Adaptors measuring from a
	// unit look its placement up once, when the adaptor is made, not once per candidate.

	/// Units touching `unit`: in one of the 8 cells around it (a footprint gap of one).
	///
	/// \code
	/// const bool engaged = turn.units() | adjacentTo(turn.self()) | any();
	/// \endcode
	[[nodiscard]]
	inline auto adjacentTo(const UnitRef unit)
	{
		return where([from = unit.placement()](const UnitRef& other)
					 { return distance(from, other.placement()) == Distance{1}; });
	}

	/// Units whose distance from `unit` lies in `range`, both ends inclusive. Ignores immunities; use
	/// `attackableBy` to pick targets.
	///
	/// \code
	/// const Range nearby{.min = Distance{2}, .max = Distance{3}};
	/// const std::size_t crowd = turn.units() | within(turn.self(), nearby) | count();
	/// \endcode
	[[nodiscard]]
	inline auto within(const UnitRef unit, const Range range)
	{
		return where([from = unit.placement(), range](const UnitRef& other)
					 { return range.contains(distance(from, other.placement())); });
	}

	/// Units `attacker` may hit with `attack`: they have Health, their Targeted hooks let the attack through, and they
	/// are within its (possibly narrowed) range.
	///
	/// \code
	/// const auto shot = common::ranged({.min = Distance{2}, .max = Distance{5}});
	/// const auto target
	///         = turn.units() | attackableBy(turn.self(), shot) | pickRandom(turn);
	/// \endcode
	[[nodiscard]]
	inline auto attackableBy(const UnitRef attacker, const Attack attack)
	{
		return where([attacker, from = attacker.placement(), attack](const UnitRef& target)
					 { return target.isAttackableBy(attacker, from, attack); });
	}

	/// Units having component `C`, in their kit or applied (see UnitRef::has).
	///
	/// \code
	/// const bool anyoneFlying = turn.units() | with<common::Flying>() | any();
	/// \endcode
	template <Component C>
	[[nodiscard]]
	auto with()
	{
		return where([](const UnitRef& unit) { return unit.has<C>(); });
	}

	/// Units of unit class `T`, compared by `T::Name`.
	///
	/// \code
	/// const auto hunter = turn.units() | ofKind<hunter::Hunter>() | first();
	/// \endcode
	template <UnitType T>
	[[nodiscard]]
	auto ofKind()
	{
		return where([](const UnitRef& unit) { return unit.name() == UnitName{T::Name}; });
	}
}
