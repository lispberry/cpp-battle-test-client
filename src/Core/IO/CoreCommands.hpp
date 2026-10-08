#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/IO/Commands.hpp>
#include <Core/IO/ScenarioError.hpp>
#include <Core/Simulation/Simulation.hpp>

#include <cstdint>

namespace sw
{
	/// `CREATE_MAP width height`: creates the map. Must come before any spawn, and only once.
	struct CreateMapCommand
	{
		static constexpr const char* Name = "CREATE_MAP";

		uint32_t width{};
		uint32_t height{};
	};
	SW_REFLECT(CreateMapCommand, (), (width, height))

	/// Handles CREATE_MAP: a WorldError (a second map, a side over Map::MaxSide) rejects the line.
	[[nodiscard]]
	CommandResult createMap(Simulation& simulation, const CreateMapCommand& command);

	/// The commands that are not about any feature: CREATE_MAP. The application registers these, then the features'.
	void registerCoreCommands(Commands& commands);
}
