#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Events/SpeedQuery.hpp>
#include <Core/Events/Targeted.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/IO/Commands.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Query/Units.hpp>
#include <Core/Simulation/SpawnOrder.hpp>
#include <Core/Units/Health.hpp>
#include <Core/Units/Unit.hpp>
#include <Core/World/UnitRef.hpp>
#include <Features/Common/Body.hpp>
#include <Features/Common/March.hpp>
#include <Features/Common/MeleeAttack.hpp>
#include <Features/Common/RangedAttack.hpp>

#include <cstdint>

// The reference feature: one unit that uses every mechanic of the engine. Start a new unit as a copy of this folder,
// keep what you need, delete the rest. Core's Health and the Features/Common components (Body, Flying, Invulnerable,
// Timed<C>, MeleeAttack, RangedAttack, March) are reused as they are.
namespace sw::blueprint
{
	// --- An applied component: attached during the battle, removes itself. Deals `damage` at each of the next `left` round
	// ends, attributed to `source`.
	struct Burn
	{
		UnitId source;
		Damage damage;
		Rounds left;

		[[nodiscard]]
		Effects on(const RoundEnd& end)
		{
			auto effects = effect::attack(source, end.self().id(), damage);
			left = Rounds{left.get() - 1};
			if (left.get() == 0)
			{
				effects |= effect::expire();
			}
			return effects;
		}
	};

	// --- One component per event and question. An ability's `Name` is what UNIT_ABILITY_USED prints and what the
	// damage it deals carries.

	// Targeted (a question): who may attack this unit. Refuses attacks that only work from farther than `reach` cells.
	struct Ward
	{
		// Why the attack is rejected: an immunity is a name the component declares, like an ability.
		Distance reach;

		[[nodiscard]]
		std::optional<Attack> on(const Targeted& targeted) const
		{
			if (targeted.attack().range.min > reach)
			{
				return std::nullopt;
			}
			return targeted.attack();
		}
	};

	// HitAttempt (on the attacker): an ability that may take over a hit. With chance/1000, sets the target on fire
	// instead: returning effects takes the hit over, so there is no plain damage and no other ability is asked.
	struct Ember
	{
		static constexpr Ability Name{"ember"};

		Chance chance;
		Damage burn;
		Rounds rounds;

		[[nodiscard]]
		Effects on(const HitAttempt& attempt) const
		{
			if (!attempt.roll(chance))
			{
				return {};
			}
			return effect::useAbility(attempt.attacker(), *this)
				   | effect::apply(
						   attempt.target(),
						   Burn{.source = attempt.attacker(), .damage = burn, .left = rounds},
						   attempt.attacker());
		}
	};

	// HitTaken (on the target, after the damage): hits the attacker back. Damage this ability dealt is never answered,
	// so two such units do not echo each other forever.
	struct Retaliation
	{
		static constexpr Ability Name{"retaliation"};

		Damage damage;

		[[nodiscard]]
		Effects on(const HitTaken& taken) const
		{
			if (taken.attacker() == taken.target() || taken.ability() == Name)
			{
				return {};
			}
			return effect::attack(taken.target(), taken.attacker(), damage, *this);
		}
	};

	// SpeedQuery (a question): how far the unit marches per turn.
	struct Swiftness
	{
		Speed bonus;

		[[nodiscard]]
		Speed on(const SpeedQuery& query) const
		{
			return Speed{query.speed.get() + bonus.get()};
		}
	};

	// RoundEnd on a component (permanent, unlike Burn): every `every` rounds, uses an ability.
	struct Vigil
	{
		static constexpr Ability Name{"vigil"};

		Rounds every;

		[[nodiscard]]
		Effects on(const RoundEnd& end) const
		{
			if (end.round().get() % every.get() != 0)
			{
				return {};
			}
			return effect::useAbility(end.self().id(), *this);
		}
	};

	// --- A query adaptor of the feature's own: one `where` over a UnitRef question, used like the built-in ones.
	[[nodiscard]]
	inline auto fragile(const Hp threshold)
	{
		return where(
				[threshold](const UnitRef& unit)
				{
					const auto* health = unit.get<Health>();
					return health != nullptr && health->hp <= threshold;
				});
	}

	struct BlueprintKit
	{
		Health health;
		common::Body body;
		Ward ward;
		common::MeleeAttack strike;
		common::RangedAttack volley;
		Ember ember;
		Retaliation retaliation;
		Swiftness swiftness;
		Vigil vigil;
		common::March march;
	};
	SW_REFLECT(BlueprintKit, (), (health, body, ward, strike, volley, ember, retaliation, swiftness, vigil, march))

	class Blueprint final : public Unit<Blueprint, BlueprintKit>
	{
	public:
		static constexpr UnitName Name{"blueprint"};

		using Unit::Unit;

		// Finishes off a fragile neighbour first; otherwise acts in kit order (kitTurn):
		// hit any neighbour, shoot, march.
		[[nodiscard]]
		Effects on(const Turn& turn) const;
	};

	// --- The scenario command: `SPAWN_BLUEPRINT unitId x y hp`, one field per argument.
	struct SpawnBlueprintCommand
	{
		static constexpr const char* Name = "SPAWN_BLUEPRINT";

		uint32_t unitId{};
		uint32_t x{};
		uint32_t y{};
		uint32_t hp{};
	};
	SW_REFLECT(SpawnBlueprintCommand, (), (unitId, x, y, hp))

	[[nodiscard]]
	SpawnOrder spawnBlueprint(const SpawnBlueprintCommand& command);

	// Not called from Features.cpp: the blueprint is a template, not part of the game. A real feature adds its line
	// there.
	void registerBlueprint(Commands& commands);
}
