#pragma once

#include <Core/Events/PlacementQuery.hpp>
#include <Core/Events/Targeted.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Features/Common/Attacks.hpp>

#include <optional>

namespace sw::common
{
	// Flies over the map: occupies no cell (air layer), cannot be attacked in melee, and ranged attacks against it have
	// their minimum and maximum distance reduced by one (a bow of range 2..5 reaches it at 1..4).
	struct Flying
	{
		[[nodiscard]]
		Placement on(const PlacementQuery& query) const
		{
			auto placement = query.placement;
			placement.layer = Layer::Air;
			return placement;
		}

		[[nodiscard]]
		std::optional<Attack> on(const Targeted& targeted) const
		{
			if (targeted.attack().kind == Melee)
			{
				return std::nullopt;
			}
			return Attack{.kind = targeted.attack().kind, .range = targeted.attack().range.shrunk(Distance{1})};
		}
	};
}
