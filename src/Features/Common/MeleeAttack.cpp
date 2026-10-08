#include <Core/Events/Effects.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Query/Adaptors.hpp>
#include <Core/Query/Units.hpp>
#include <Core/World/UnitRef.hpp>
#include <Features/Common/Attacks.hpp>
#include <Features/Common/MeleeAttack.hpp>

#include <expected>

namespace sw::common
{
	Effects MeleeAttack::on(const Turn& turn) const
	{
		const auto target = turn.units() | attackableBy(turn.self(), melee()) | pickRandom(turn);
		if (!target)
		{
			return {};
		}
		return effect::hit(Melee, turn.self().id(), target->id(), damage);
	}
}
