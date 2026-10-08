#pragma once

#include <Core/Base/Concepts.hpp>
#include <Core/Events/Context.hpp>
#include <Core/Events/Tags.hpp>
#include <Core/Model/Types.hpp>

#include <optional>

namespace sw
{
	/// Sent to the target after damage was applied, from a plain hit or from `effect::attack`, including the hit that
	/// kills it. `self()` is the target. Every handler is asked; the effects they return all happen.
	///
	/// \code
	/// Effects Retaliation::on(const HitTaken& taken) const
	/// {
	///     // Checking the ability stops two retaliating units from echoing each other forever.
	///     if (taken.attacker() == taken.target() || taken.ability() == Name)
	///         return {};
	///     return effect::attack(taken.target(), taken.attacker(), damage, Name);
	/// }
	/// \endcode
	class HitTaken : public Context
	{
	public:
		/// Every handler is asked; all the effects happen.
		static constexpr bool FirstAnswerWins = false;

		/// The damage as it was applied, the ability that dealt it, if any, and the kinds of damage it is.
		struct Hit
		{
			UnitId attacker;
			UnitId target;
			Damage damage;
			std::optional<Ability> ability;
			TagSet tags;
		};

		HitTaken(const Context& context, Hit hit);

		[[nodiscard]]
		UnitId attacker() const;

		[[nodiscard]]
		UnitId target() const;

		[[nodiscard]]
		Damage damage() const;

		[[nodiscard]]
		std::optional<Ability> ability() const;

		/// True if the damage is of kind `Tag` (see TagList): `taken.is<common::Wound>()`.
		template <ClassType Tag>
		[[nodiscard]]
		bool is() const
		{
			return _hit.tags.has<Tag>();
		}

	private:
		Hit _hit;
	};
}
