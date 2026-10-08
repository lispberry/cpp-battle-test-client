#pragma once

#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/World/Map.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace sw
{
	/// The game's map: a `width` x `height` grid of cells, each holding the ground unit that stands on it, if any.
	/// See Map for the rules and an example.
	class GridMap final : public Map
	{
	public:
		/// An empty grid. Throws std::invalid_argument unless both sides are between 1 and Map::MaxSide.
		GridMap(uint32_t width, uint32_t height);

		[[nodiscard]]
		uint32_t width() const override;

		[[nodiscard]]
		uint32_t height() const override;

		[[nodiscard]]
		std::optional<UnitId> occupant(Position cell) const override;

		[[nodiscard]]
		std::size_t occupiedCells() const override;

		[[nodiscard]]
		std::optional<Placement> placement(UnitId unit) const override;

		void place(UnitId unit, const Placement& placement) override;

		void move(UnitId unit, Position origin) override;

		void remove(UnitId unit) override;

	private:
		// The cell's slot in `_cells`, row by row. Requires `isInside(cell)`.
		[[nodiscard]]
		std::size_t index(Position cell) const;

		void occupy(UnitId unit, const Placement& placement);

		void vacate(const Placement& placement);

		uint32_t _width;
		uint32_t _height;
		std::vector<std::optional<UnitId>> _cells;
		std::size_t _occupied{0};
		std::unordered_map<UnitId, Placement> _placements;
	};
}
