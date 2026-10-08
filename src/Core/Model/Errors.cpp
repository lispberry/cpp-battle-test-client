#include <Core/Model/Errors.hpp>

#include <string_view>

namespace sw
{
	std::string_view describe(const WorldError error)
	{
		switch (error)
		{
			case WorldError::NoMap: return "the map has not been created";
			case WorldError::MapAlreadyCreated: return "the map has already been created";
			case WorldError::MapTooLarge: return "the map is too large (at most 1000 cells per side)";
			case WorldError::MapEmpty: return "the map needs at least one cell per side";
			case WorldError::OutOfBounds: return "position is outside the map";
			case WorldError::CellOccupied: return "cell is occupied";
			case WorldError::DuplicateId: return "unit id is already taken";
			case WorldError::UnknownUnit: return "no such unit";
			case WorldError::MissingComponent: return "the unit cannot do that";
		}
		return "unknown error";
	}
}
