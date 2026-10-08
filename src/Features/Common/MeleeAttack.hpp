#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Types.hpp>

namespace sw::common
{
	struct MeleeAttack
	{
		Damage damage;

		[[nodiscard]]
		Effects on(const Turn& turn) const;
	};
	SW_REFLECT(MeleeAttack, (), (damage))
}
