#pragma once

#include <Core/Events/Context.hpp>
#include <Core/Model/Types.hpp>

namespace sw
{
	/// The event of a unit's turn. `self()` is the unit; its handlers (`Effects on(const Turn&) const`) decide what it
	/// does, and the unit acts with the first non-empty answer: its applied components first, newest first, then its
	/// own rule if its class declares one, then its kit in order. No answer means the unit stays idle.
	///
	/// Declared with the events, defined with the simulation (Core/Simulation/Turn.cpp), like Context.
	///
	/// \code
	/// Effects MeleeAttack::on(const Turn& turn) const
	/// {
	///     const auto target = turn.units() | attackableBy(turn.self(), melee()) | pickRandom(turn);
	///     if (!target)
	///         return {};                                 // nothing to hit: the next component may act
	///     return effect::hit(Melee, turn.self().id(), target->id(), damage);
	/// }
	/// \endcode
	class Turn : public Context
	{
	public:
		/// The unit acts once: the first handler that returns effects decides.
		static constexpr bool FirstAnswerWins = true;

		using Context::Context;

		/// The unit's speed this turn: `base` as its SpeedQuery handlers (slows, hastes) change it.
		[[nodiscard]]
		Speed speed(Speed base) const;
	};
}
