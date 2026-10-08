#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Model/Types.hpp>

namespace sw::hunter
{
	// With probability chance/1000, a ranged hit becomes poisoned arrows: no immediate damage, the target is poisoned
	// for `total` damage over five rounds instead.
	struct PoisonArrows
	{
		static constexpr Ability Name{"poison_arrows"};

		Chance chance;
		Damage total;

		[[nodiscard]]
		Effects on(const HitAttempt& attempt) const;
	};
	SW_REFLECT(PoisonArrows, (), (chance, total))
}
