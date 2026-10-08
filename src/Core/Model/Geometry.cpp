#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sw
{
	namespace
	{
		uint32_t
		gap(const uint32_t firstStart, const uint32_t firstSize, const uint32_t secondStart, const uint32_t secondSize)
		{
			const uint32_t firstEnd = firstStart + firstSize - 1;
			const uint32_t secondEnd = secondStart + secondSize - 1;
			if (secondStart > firstEnd)
			{
				return secondStart - firstEnd;
			}
			if (firstStart > secondEnd)
			{
				return firstStart - secondEnd;
			}
			return 0;
		}

		uint32_t stepAxis(const uint32_t from, const uint32_t to)
		{
			if (to > from)
			{
				return from + 1;
			}
			if (to < from)
			{
				return from - 1;
			}
			return from;
		}

		uint32_t shrink(const uint32_t value, const uint32_t amount)
		{
			return value > amount ? value - amount : 0;
		}
	}

	bool Placement::covers(const Position cell) const
	{
		return cell.x >= origin.x && cell.x < origin.x + size.width && cell.y >= origin.y
			   && cell.y < origin.y + size.height;
	}

	Distance distance(const Position from, const Position to)
	{
		return distance(
				Placement{.origin = from, .size = {.width = 1, .height = 1}, .layer = Layer::Ground},
				Placement{.origin = to, .size = {.width = 1, .height = 1}, .layer = Layer::Ground});
	}

	std::vector<Position> cells(const Placement& placement)
	{
		std::vector<Position> covered;
		covered.reserve(std::size_t{placement.size.width} * placement.size.height);
		for (uint32_t y = placement.origin.y; y < placement.origin.y + placement.size.height; ++y)
		{
			for (uint32_t x = placement.origin.x; x < placement.origin.x + placement.size.width; ++x)
			{
				covered.push_back({.x = x, .y = y});
			}
		}
		return covered;
	}

	Distance distance(const Placement& from, const Placement& to)
	{
		const uint32_t dx = gap(from.origin.x, from.size.width, to.origin.x, to.size.width);
		const uint32_t dy = gap(from.origin.y, from.size.height, to.origin.y, to.size.height);
		return Distance{std::max(dx, dy)};
	}

	Position stepToward(const Position from, const Position to)
	{
		return Position{.x = stepAxis(from.x, to.x), .y = stepAxis(from.y, to.y)};
	}

	bool Range::contains(const Distance distance) const
	{
		return distance >= min && distance <= max;
	}

	Range Range::shrunk(const Distance amount) const
	{
		return Range{
				.min = Distance{shrink(min.get(), amount.get())},
				.max = Distance{shrink(max.get(), amount.get())},
		};
	}
}
