#include <Core/Events/Effects.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Model/Types.hpp>
#include <Features/Common/Attacks.hpp>
#include <Features/Hunter/Poison.hpp>
#include <Features/Hunter/PoisonArrows.hpp>

namespace sw::hunter
{
	Effects PoisonArrows::on(const HitAttempt& attempt) const
	{
		if (attempt.kind() != common::Ranged || !attempt.roll(chance))
		{
			return {};
		}
		return effect::useAbility(attempt.attacker(), *this)
			   | effect::apply(attempt.target(), Poison{attempt.attacker(), total}, attempt.attacker());
	}
}
