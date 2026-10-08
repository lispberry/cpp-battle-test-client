#pragma once

#include <Core/Log/Records.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Units/AnyUnit.hpp>
#include <Core/Units/Health.hpp>
#include <Core/World/Map.hpp>
#include <Core/World/World.hpp>
#include <Support/Target.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace sw::fuzz
{
	// Ground cells hold exactly the units whose footprint covers them, given every placement on the map.
	inline void expectOccupancyMatches(const Map& map, const std::span<const UnitId> placed)
	{
		std::map<std::pair<uint32_t, uint32_t>, UnitId> expected;
		for (const UnitId id : placed)
		{
			const std::optional<Placement> placement = map.placement(id);
			expect(placement.has_value(), "every placed object has a placement");
			expect(map.contains(*placement), "placements lie inside the map");
			if (placement->layer == Layer::Air)
			{
				continue;
			}
			for (uint32_t y = placement->origin.y; y < placement->origin.y + placement->size.height; ++y)
			{
				for (uint32_t x = placement->origin.x; x < placement->origin.x + placement->size.width; ++x)
				{
					expect(expected.emplace(std::pair{x, y}, id).second, "two ground objects never share a cell");
				}
			}
		}
		// Every covered cell names its unit, and no other cell is occupied (cost follows the units, not the map).
		for (const auto& [cell, id] : expected)
		{
			expect(map.occupant({.x = cell.first, .y = cell.second}) == id, "covered cells name their unit");
		}
		expect(map.occupiedCells() == expected.size(), "no cell is occupied without a unit covering it");
	}

	// Every unit is registered once, placed consistently, and alive after an action.
	inline void expectConsistent(const World& world)
	{
		const std::span<const UnitId> order = world.units().order();
		expect(std::set(order.begin(), order.end()).size() == order.size(), "creation order has no duplicates");
		for (const UnitId id : order)
		{
			const AnyUnit* unit = world.units().find(id);
			expect(unit != nullptr, "every id in creation order is registered");
			const Health* health = unit->get<Health>();
			expect(health == nullptr || health->hp.get() > 0, "units at 0 hp are removed after the action");
		}
		expectOccupancyMatches(world.map(), order);
	}

	// Hit points never go up (nothing heals yet), and every unit dies at most once.
	inline void expectLogConsistent(const std::span<const std::pair<Round, Record>> records)
	{
		std::map<uint32_t, uint32_t> lastHp;
		std::set<uint32_t> dead;
		Round previous{0};
		for (const auto& [round, record] : records)
		{
			expect(round >= previous, "rounds in the log never go back");
			previous = round;
			if (const auto* attacked = std::get_if<UnitAttacked>(&record))
			{
				const auto [entry, fresh] = lastHp.emplace(attacked->targetUnitId, attacked->targetHp);
				expect(fresh || attacked->targetHp <= entry->second, "a unit's hp never goes up");
				entry->second = attacked->targetHp;
				expect(!dead.contains(attacked->targetUnitId), "dead units are not attacked");
			}
			if (const auto* died = std::get_if<UnitDied>(&record))
			{
				expect(dead.insert(died->unitId).second, "a unit dies once");
			}
		}
	}
}
