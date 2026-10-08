#pragma once

#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>

// The attack kinds of the game, declared once so that abilities and defences of any feature can react to them
// (Rending to melee, PoisonArrows to ranged, Flying against melee).
namespace sw::common
{
	inline constexpr AttackKind Melee{"melee"};
	inline constexpr AttackKind Ranged{"ranged"};

	[[nodiscard]]
	inline Attack melee()
	{
		return Attack{.kind = Melee, .range = {.min = Distance{1}, .max = Distance{1}}};
	}

	[[nodiscard]]
	inline Attack ranged(const Range range)
	{
		return Attack{.kind = Ranged, .range = range};
	}
}
