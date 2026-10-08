// The map on its own: any sequence of place / move / remove, each applied only when canPlace allows it. The ground
// grid must always match the placements, and canPlace must agree with a naive check.

#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/World/GridMap.hpp>
#include <Core/World/Map.hpp>
#include <Support/GameObjects.hpp>
#include <Support/Invariants.hpp>
#include <Support/Target.hpp>

#include <cstdint>
#include <set>
#include <variant>
#include <vector>

namespace
{
	using sw::Placement;
	using sw::UnitId;

	// canPlace, written the slow and obvious way: inside the map, and on the ground no other object's footprint
	// shares a cell.
	bool naiveCanPlace(
			const sw::Map& map, const std::set<UnitId>& placed, const UnitId self, const Placement& placement)
	{
		if (!map.contains(placement))
		{
			return false;
		}
		if (placement.layer == sw::Layer::Air)
		{
			return true;
		}
		for (const UnitId other : placed)
		{
			const Placement existing = map.at(other);
			if (other != self && existing.layer == sw::Layer::Ground
				&& sw::distance(existing, placement) == sw::Distance{0})
			{
				return false;
			}
		}
		return true;
	}

	void mapOccupancyMatchesPlacements(
			const sw::fuzz::MapSize& size, const std::vector<sw::fuzz::MapOperation>& operations)
	{
		sw::GridMap map(size.width, size.height);
		std::set<UnitId> placed;
		for (const sw::fuzz::MapOperation& operation : operations)
		{
			if (const auto* place = std::get_if<sw::fuzz::Place>(&operation))
			{
				const UnitId id{place->object.value};
				if (placed.contains(id))
				{
					continue;
				}
				const bool allowed = map.canPlace(id, place->placement);
				sw::fuzz::expect(allowed == naiveCanPlace(map, placed, id, place->placement), "canPlace is right");
				if (allowed)
				{
					map.place(id, place->placement);
					placed.insert(id);
				}
			}
			else if (const auto* move = std::get_if<sw::fuzz::Move>(&operation))
			{
				const UnitId id{move->object.value};
				if (!placed.contains(id))
				{
					continue;
				}
				Placement moved = map.at(id);
				moved.origin = move->origin;
				const bool allowed = map.canPlace(id, moved);
				sw::fuzz::expect(allowed == naiveCanPlace(map, placed, id, moved), "canPlace is right for moves");
				if (allowed)
				{
					map.move(id, move->origin);
				}
			}
			else
			{
				const UnitId id{std::get<sw::fuzz::Remove>(operation).object.value};
				map.remove(id);
				placed.erase(id);
			}
			const std::vector<UnitId> ids(placed.begin(), placed.end());
			sw::fuzz::expectOccupancyMatches(map, ids);
		}
	}
}

SW_FUZZ_TARGET(mapOccupancyMatchesPlacements)
