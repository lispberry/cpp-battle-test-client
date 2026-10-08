#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>

#include <optional>

namespace sw::common
{
	// Walks toward `target` (set by the MARCH command), up to `speed` cells per turn, one of the 8 neighbours at a
	// time. On arrival logs MARCH_ENDED and clears `target`; ordered to the cell it stands on, it does only that, on
	// its turn (as in the original prototype). Idle::NoOrder without a target, Idle::Blocked if the next cell is
	// taken.
	struct March
	{
		Speed speed{1};
		std::optional<Position> target;

		[[nodiscard]]
		Effects on(const Turn& turn) const;
	};
	SW_REFLECT(March, (), (speed, target))
}
