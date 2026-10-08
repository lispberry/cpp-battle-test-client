#pragma once

#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace sw
{
	/// The battlefield: where every unit stands. The single source of truth for positions.
	///
	/// Every unit has a placement; units on the ground layer also occupy the cells of their footprint, so two ground
	/// units never share a cell. Air units occupy nothing. An implementation provides storage (the pure virtual
	/// functions); the rules built on it (`canPlace`, `canStep`, ...) are the same for every map. `place` and `move`
	/// do not check their target: ask `canPlace` or `canStep` first. GridMap is the game's map; a test or a new
	/// terrain can provide another.
	///
	/// \code
	/// GridMap map(10, 10);
	/// const Placement tower{.origin = {.x = 1, .y = 1}, .size = {.width = 2, .height = 2}};
	/// if (map.canPlace(UnitId{1}, tower))
	/// {
	///     map.place(UnitId{1}, tower);
	/// }
	/// map.occupant({.x = 2, .y = 2});                    // UnitId{1}: the tower covers cells (1..2, 1..2)
	/// map.isFree({.x = 3, .y = 3});                      // true
	/// map.canStep(UnitId{1}, {.x = 2, .y = 2});          // true: the tower's own cells do not block it
	/// \endcode
	class Map
	{
	public:
		/// The largest map side. Battles are fought by a handful of units on small grids; the limit keeps any scenario
		/// (and any fuzz input) from describing a world nobody could play or check.
		static constexpr uint32_t MaxSide = 1000;

		virtual ~Map() = default;

		[[nodiscard]]
		virtual uint32_t width() const = 0;

		[[nodiscard]]
		virtual uint32_t height() const = 0;

		/// The ground unit covering `cell`, or nullopt. Air units are never occupants.
		[[nodiscard]]
		virtual std::optional<UnitId> occupant(Position cell) const = 0;

		/// How many cells ground units occupy.
		[[nodiscard]]
		virtual std::size_t occupiedCells() const = 0;

		/// Where `unit` is, or nullopt if it is not on the map.
		[[nodiscard]]
		virtual std::optional<Placement> placement(UnitId unit) const = 0;

		/// Puts `unit` on the map. Requires `canPlace(unit, placement)`; not checked.
		virtual void place(UnitId unit, const Placement& placement) = 0;

		/// Moves the footprint of `unit` so that it starts at `origin`. Requires the unit on the map and `canPlace` for
		/// the new placement; not checked.
		virtual void move(UnitId unit, Position origin) = 0;

		/// Takes `unit` off the map and frees its cells. Does nothing if it is not on the map.
		virtual void remove(UnitId unit) = 0;

		[[nodiscard]]
		bool isInside(Position cell) const;

		/// True if the whole footprint of `placement` lies inside the map.
		[[nodiscard]]
		bool contains(const Placement& placement) const;

		/// Inside the map and no ground unit stands on it.
		[[nodiscard]]
		bool isFree(Position cell) const;

		/// True if `placement` is inside the map and, on the ground, no unit other than `self` occupies its cells. Pass
		/// the moving unit as `self`, so that its own cells do not block it.
		[[nodiscard]]
		bool canPlace(UnitId self, const Placement& placement) const;

		/// May `unit`, with its origin at `from`, take one step to `to`: `to` is one of the 8 neighbours of `from`,
		/// the unit is on the map, and its footprint fits at `to`. `from` need not be where the unit stands now, so a
		/// unit can plan several steps of one turn against a world that does not change while it decides.
		[[nodiscard]]
		bool canStep(UnitId unit, Position from, Position to) const;

		/// `canStep` from where `unit` stands now.
		[[nodiscard]]
		bool canStep(UnitId unit, Position to) const;

		/// Where `unit` is. Throws std::out_of_range if it is not on the map.
		[[nodiscard]]
		Placement at(UnitId unit) const;

	protected:
		Map() = default;
		Map(const Map&) = default;
		Map(Map&&) = default;
		Map& operator=(const Map&) = default;
		Map& operator=(Map&&) = default;
	};
}
