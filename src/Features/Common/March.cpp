#include <Core/Events/Effects.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/World/Map.hpp>
#include <Core/World/UnitRef.hpp>
#include <Features/Common/March.hpp>

#include <cstdint>

namespace sw::common
{
	Effects March::on(const Turn& turn) const
	{
		if (!target)
		{
			return {};
		}
		const auto self = turn.self().id();
		auto at = turn.self().position();
		// The world does not change while the unit decides: plan each step from where the previous one ends. Ordered to
		// the cell it stands on, the march ends at once, on this turn (as in the original prototype).
		Effects effects;
		const auto steps = turn.speed(speed).get();
		for (uint32_t step = 0; step < steps && at != *target; ++step)
		{
			const auto next = stepToward(at, *target);
			if (!turn.map().canStep(self, at, next))
			{
				break;
			}
			effects |= effect::moveTo(self, next);
			at = next;
		}
		if (at == *target)
		{
			effects |= effect::report(MarchEnded{.unitId = self.get(), .x = at.x, .y = at.y})
					   | effect::change<March>(self, [](March& march) { march.target.reset(); });
		}
		return effects;
	}
}
