#pragma once

#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>

#include <optional>

namespace sw
{
	/// Asked of a unit while someone selects targets: may `attacker` target it with `attack`, and with what range?
	/// Each handler returns the attack it lets through, possibly narrowed, or `std::nullopt` to refuse it; after a
	/// refusal no other handler is asked. The distance to the unit is checked against the attack that comes out.
	///
	/// \code
	/// std::optional<Attack> Flying::on(const Targeted& targeted) const
	/// {
	///     if (targeted.attack().kind == Melee)
	///         return std::nullopt;                       // cannot be hit in melee
	///     return Attack{.kind = targeted.attack().kind, .range = targeted.attack().range.shrunk(Distance{1})};
	/// }
	/// \endcode
	struct Targeted
	{
		/// What a handler returns: the attack it lets through, or nothing.
		using Answer = std::optional<Attack>;

		/// Who attacks.
		UnitId attacker;

		/// The attack as the handlers so far let it through; `std::nullopt` once one refused it.
		Answer exposure;

		/// The attack as it stands. A handler always sees one: after a refusal no handler is asked.
		[[nodiscard]]
		Attack attack() const
		{
			return exposure.value_or(Attack{});
		}

		/// The answer so far, for the engine to fill in.
		[[nodiscard]]
		Answer& answer()
		{
			return exposure;
		}
	};
}
