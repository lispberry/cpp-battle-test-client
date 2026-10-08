#pragma once

#include <Core/Base/RandomSource.hpp>
#include <Core/Base/StrongType.hpp>

#include <cstdint>
#include <string_view>

namespace sw
{
	/// Identifies a unit; chosen by the scenario (`SPAWN_* unitId ...`) and unique within a battle.
	using UnitId = StrongType<uint32_t, struct UnitIdTag>;
	/// Health points. A unit at 0 hp is dead; `Hp - Damage` saturates at zero.
	using Hp = StrongType<uint32_t, struct HpTag>;
	/// Health points taken by a hit.
	using Damage = StrongType<uint32_t, struct DamageTag>;
	/// Probability per mille (in thousandths): `Chance{250}` succeeds a quarter of the time. Roll it with `rolls`.
	using Chance = StrongType<uint32_t, struct ChanceTag>;
	/// Distance in cells, as measured by `distance`.
	using Distance = StrongType<uint32_t, struct DistanceTag>;
	/// Cells a unit may step in one turn.
	using Speed = StrongType<uint32_t, struct SpeedTag>;
	/// A point in simulation time; printed as `[N]` in the battle log.
	using Round = StrongType<uint32_t, struct RoundTag>;
	/// A duration in rounds, e.g. how long an effect lasts.
	using Rounds = StrongType<uint32_t, struct RoundsTag>;
	/// A unit type's name as printed in the log, e.g. "hunter". Views a string literal, so it never owns its text.
	using UnitName = StrongType<std::string_view, struct UnitNameTag>;
	/// An ability, by the name printed in UNIT_ABILITY_USED, e.g. "rending". Views a string literal. Each ability is the
	/// `Name` of the component that implements it; other features react to the kinds of damage it deals (TagList), not
	/// to its name.
	using Ability = StrongType<std::string_view, struct AbilityTag>;

	/// Health after `damage`; never drops below zero.
	[[nodiscard]]
	Hp operator-(Hp hp, Damage damage);

	[[nodiscard]]
	Damage operator+(Damage left, Damage right);

	[[nodiscard]]
	Damage operator*(Damage damage, uint32_t factor);

	/// The round after `round`.
	[[nodiscard]]
	Round next(Round round);

	/// Rolls `chance` against `random`: succeeds when `random.uniform(1, 1000) <= chance`.
	/// Chance{0} never succeeds, Chance{1000} always does.
	[[nodiscard]]
	bool rolls(Chance chance, RandomSource& random);
}
