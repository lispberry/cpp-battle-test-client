#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Units/Health.hpp>
#include <Core/Units/Unit.hpp>
#include <Features/Common/March.hpp>
#include <Features/Common/MeleeAttack.hpp>
#include <Features/Common/RangedAttack.hpp>
#include <Features/Hunter/PoisonArrows.hpp>

namespace sw::hunter
{
	struct HunterKit
	{
		Health health;
		common::RangedAttack bow;
		common::MeleeAttack knife;
		PoisonArrows poison;
		common::March march;
	};
	SW_REFLECT(HunterKit, (), (health, bow, knife, poison, march))

	class Hunter final : public Unit<Hunter, HunterKit>
	{
	public:
		static constexpr UnitName Name{"hunter"};

		using Unit::Unit;
	};
}
