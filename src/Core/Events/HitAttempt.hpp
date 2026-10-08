#pragma once

#include <Core/Base/RandomSource.hpp>
#include <Core/Events/Context.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>

namespace sw
{
	/// Sent to the attacker when one of its hits (`effect::hit`) lands, before any damage. `self()` is the attacker.
	///
	/// A handler that returns effects takes the hit over: they happen instead of the plain damage, and no other
	/// handler is asked. Returning no effects lets the plain hit land. Damage dealt with `effect::attack` never goes
	/// through this event.
	///
	/// \code
	/// Effects Rending::on(const HitAttempt& attempt) const
	/// {
	///     if (attempt.kind() != common::Melee || !attempt.roll(chance))
	///         return {};                                 // the plain hit lands
	///     return effect::useAbility(attempt.attacker(), Name)          // logged as UNIT_ABILITY_USED
	///          | effect::attack(attempt.attacker(), attempt.target(), damage, Name);
	/// }
	/// \endcode
	class HitAttempt : public Context
	{
	public:
		/// One ability takes the hit over: the first handler that returns effects decides.
		static constexpr bool FirstAnswerWins = true;

		/// The hit as the attacker made it.
		struct Hit
		{
			AttackKind kind;
			UnitId attacker;
			UnitId target;
			Damage damage;
		};

		HitAttempt(const Context& context, Hit hit);

		[[nodiscard]]
		AttackKind kind() const;

		[[nodiscard]]
		UnitId attacker() const;

		[[nodiscard]]
		UnitId target() const;

		[[nodiscard]]
		Damage damage() const;

	private:
		Hit _hit;
	};
}
