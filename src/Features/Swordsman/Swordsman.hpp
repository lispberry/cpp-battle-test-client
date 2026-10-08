#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Units/Health.hpp>
#include <Core/Units/Unit.hpp>
#include <Features/Common/March.hpp>
#include <Features/Common/MeleeAttack.hpp>
#include <Features/Swordsman/Rending.hpp>

namespace sw::swordsman
{
	struct SwordsmanKit
	{
		Health health;
		common::MeleeAttack sword;
		Rending rending;
		common::March march;
	};
	SW_REFLECT(SwordsmanKit, (), (health, sword, rending, march))

	class Swordsman final : public Unit<Swordsman, SwordsmanKit>
	{
	public:
		static constexpr UnitName Name{"swordsman"};

		using Unit::Unit;
	};
}
