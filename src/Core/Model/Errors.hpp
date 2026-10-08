#pragma once

#include <cstdint>
#include <string_view>

namespace sw
{
	/// Why a unit did not act this turn.
	enum class Idle : uint8_t
	{
		NoTarget,
		NoOrder,
		Blocked,
	};

	/// Why a scenario command could not change the world.
	enum class WorldError : uint8_t
	{
		NoMap,
		MapAlreadyCreated,
		MapTooLarge,
		MapEmpty,
		OutOfBounds,
		CellOccupied,
		DuplicateId,
		UnknownUnit,
		MissingComponent,
	};

	/// A message for the user, to return as a rejected command's reason (see CommandResult).
	///
	/// \code
	/// const std::string_view reason = describe(WorldError::CellOccupied);   // "cell is occupied"
	/// \endcode
	[[nodiscard]]
	std::string_view describe(WorldError error);
}
