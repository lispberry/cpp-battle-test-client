#pragma once

#include <Core/World/Map.hpp>
#include <Core/World/UnitRegistry.hpp>

#include <memory>

namespace sw
{
	/// All simulation state: the units and the map they stand on. A world always has its map: it is created with it,
	/// at CREATE_MAP. A movable value; Simulation and its Executor change it, feature code only reads it, through
	/// queries and UnitRef.
	///
	/// \code
	/// World world(std::make_unique<GridMap>(10, 10));
	/// for (const UnitId id : world.units().order())
	/// {
	///     const UnitRef unit(world, id);
	///     std::println("{} at ({}, {})", unit.name().get(), unit.position().x, unit.position().y);
	/// }
	/// \endcode
	class World
	{
	public:
		/// A world on `map`, with no units yet. Throws std::invalid_argument if `map` is null.
		explicit World(std::unique_ptr<Map> map);

		/// The units, in creation order.
		[[nodiscard]]
		UnitRegistry& units()
		{
			return _units;
		}

		[[nodiscard]]
		const UnitRegistry& units() const
		{
			return _units;
		}

		[[nodiscard]]
		Map& map()
		{
			return *_map;
		}

		[[nodiscard]]
		const Map& map() const
		{
			return *_map;
		}

	private:
		UnitRegistry _units;
		std::unique_ptr<Map> _map;
	};
}
