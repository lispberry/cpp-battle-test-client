#include <Core/Events/Effects.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Query/Adaptors.hpp>
#include <Core/Query/Units.hpp>
#include <Core/World/UnitRef.hpp>
#include <Features/Common/Attacks.hpp>
#include <Features/Common/RangedAttack.hpp>

namespace sw::common
{
	Effects RangedAttack::on(const Turn& turn) const
	{
		if (holdWhenEngaged && (turn.units() | adjacentTo(turn.self()) | any()))
		{
			return {};
		}
		const auto target = turn.units() | attackableBy(turn.self(), ranged(range)) | pickRandom(turn);
		if (!target)
		{
			return {};
		}
		return effect::hit(Ranged, turn.self().id(), target->id(), damage);
	}
}
