#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/World/GridMap.hpp>
#include <Core/World/Map.hpp>

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>

namespace sw
{
	namespace
	{
		uint32_t checkedSide(const uint32_t side)
		{
			if (side == 0 || side > Map::MaxSide)
			{
				throw std::invalid_argument(std::format("a map side must be 1..{}, not {}", Map::MaxSide, side));
			}
			return side;
		}
	}

	GridMap::GridMap(const uint32_t width, const uint32_t height) :
			_width(checkedSide(width)),
			_height(checkedSide(height)),
			_cells(std::size_t{width} * height)
	{}

	uint32_t GridMap::width() const
	{
		return _width;
	}

	uint32_t GridMap::height() const
	{
		return _height;
	}

	std::optional<UnitId> GridMap::occupant(const Position cell) const
	{
		if (!isInside(cell))
		{
			return std::nullopt;
		}
		return _cells[index(cell)];
	}

	std::size_t GridMap::occupiedCells() const
	{
		return _occupied;
	}

	std::optional<Placement> GridMap::placement(const UnitId unit) const
	{
		const auto found = _placements.find(unit);
		if (found == _placements.end())
		{
			return std::nullopt;
		}
		return found->second;
	}

	void GridMap::place(const UnitId unit, const Placement& placement)
	{
		_placements[unit] = placement;
		occupy(unit, placement);
	}

	void GridMap::move(const UnitId unit, const Position origin)
	{
		Placement& placement = _placements.at(unit);
		vacate(placement);
		placement.origin = origin;
		occupy(unit, placement);
	}

	void GridMap::remove(const UnitId unit)
	{
		const auto found = _placements.find(unit);
		if (found == _placements.end())
		{
			return;
		}
		vacate(found->second);
		_placements.erase(found);
	}

	std::size_t GridMap::index(const Position cell) const
	{
		return (std::size_t{cell.y} * _width) + cell.x;
	}

	void GridMap::occupy(const UnitId unit, const Placement& placement)
	{
		if (placement.layer == Layer::Air)
		{
			return;
		}
		for (const Position cell : cells(placement))
		{
			_cells[index(cell)] = unit;
			++_occupied;
		}
	}

	void GridMap::vacate(const Placement& placement)
	{
		if (placement.layer == Layer::Air)
		{
			return;
		}
		for (const Position cell : cells(placement))
		{
			_cells[index(cell)].reset();
			--_occupied;
		}
	}
}
