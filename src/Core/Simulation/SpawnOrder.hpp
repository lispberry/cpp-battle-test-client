#pragma once

#include <Core/Model/Geometry.hpp>
#include <Core/Units/AnyUnit.hpp>

#include <memory>

namespace sw
{
	/// A unit to put on the map at `position`: built by a feature's spawn adapter (`makeUnit<Hunter>(...)`),
	/// placed by `Simulation::spawn`.
	struct SpawnOrder
	{
		Position position;
		std::unique_ptr<AnyUnit> unit;
	};
}
