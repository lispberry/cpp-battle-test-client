#include <Core/Events/Effects.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Model/Types.hpp>
#include <Features/Common/Attacks.hpp>
#include <Features/Swordsman/Rending.hpp>

namespace sw::swordsman
{
	Effects Rending::on(const HitAttempt& attempt) const
	{
		if (attempt.kind() != common::Melee || !attempt.roll(chance))
		{
			return {};
		}
		return effect::useAbility(attempt.attacker(), *this)
			   | effect::attack(attempt.attacker(), attempt.target(), damage, *this);
	}
}
