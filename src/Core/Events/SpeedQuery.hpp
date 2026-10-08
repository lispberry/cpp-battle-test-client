#pragma once

#include <Core/Model/Types.hpp>

namespace sw
{
	/// Asked when a unit marches (`Turn::speed`): how many cells may it move this turn? Starts at the unit's base speed;
	/// each handler returns the speed as it sees it, from the speed so far (slow, root, haste).
	///
	/// \code
	/// struct Swiftness
	/// {
	///     Speed bonus;
	///     Speed on(const SpeedQuery& query) const { return Speed{query.speed.get() + bonus.get()}; }
	/// };
	/// \endcode
	struct SpeedQuery
	{
		/// What a handler returns.
		using Answer = Speed;

		/// The speed so far.
		Speed speed;

		/// The answer so far, for the engine to fill in.
		[[nodiscard]]
		Answer& answer()
		{
			return speed;
		}
	};
}
