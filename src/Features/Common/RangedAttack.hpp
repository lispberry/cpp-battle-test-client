#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>

namespace sw::common
{
	// Shoots a random unit whose distance lies in `range`, for `damage`. With `holdWhenEngaged`, it does not shoot
	// while any unit is adjacent (Idle::Blocked), so a melee component after it in the kit takes over.
	struct RangedAttack
	{
		Range range;
		Damage damage;
		bool holdWhenEngaged{false};

		[[nodiscard]]
		Effects on(const Turn& turn) const;
	};
	SW_REFLECT(RangedAttack, (), (range, damage, holdWhenEngaged))
}
