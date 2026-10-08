#pragma once

#include <Core/Events/PlacementQuery.hpp>
#include <Core/Model/Geometry.hpp>

#include <cstdint>

namespace sw::common
{
	// A unit's footprint in cells, counted from its position (the top-left cell). Units without a Body take one cell.
	struct Body
	{
		uint32_t width{1};
		uint32_t height{1};

		[[nodiscard]]
		Placement on(const PlacementQuery& query) const
		{
			auto placement = query.placement;
			placement.size = {.width = width, .height = height};
			return placement;
		}
	};
}
