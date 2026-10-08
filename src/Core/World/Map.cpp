#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/World/Map.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace sw
{
	bool Map::isInside(const Position cell) const
	{
		return cell.x < width() && cell.y < height();
	}

	bool Map::contains(const Placement& placement) const
	{
		const uint64_t right = uint64_t{placement.origin.x} + placement.size.width;
		const uint64_t bottom = uint64_t{placement.origin.y} + placement.size.height;
		return right <= width() && bottom <= height();
	}

	bool Map::isFree(const Position cell) const
	{
		return isInside(cell) && !occupant(cell);
	}

	bool Map::canPlace(const UnitId self, const Placement& placement) const
	{
		if (!contains(placement))
		{
			return false;
		}
		if (placement.layer == Layer::Air)
		{
			return true;
		}
		return std::ranges::none_of(
				cells(placement),
				[this, self](const Position cell)
				{
					const auto other = occupant(cell);
					return other && *other != self;
				});
	}

	bool Map::canStep(const UnitId unit, const Position from, const Position to) const
	{
		auto moved = placement(unit);
		if (!moved || distance(from, to) != Distance{1})
		{
			return false;
		}
		moved->origin = to;
		return canPlace(unit, *moved);
	}

	bool Map::canStep(const UnitId unit, const Position to) const
	{
		const auto current = placement(unit);
		return current && canStep(unit, current->origin, to);
	}

	Placement Map::at(const UnitId unit) const
	{
		const auto found = placement(unit);
		if (!found)
		{
			throw std::out_of_range("the unit is not on the map");
		}
		return *found;
	}
}
