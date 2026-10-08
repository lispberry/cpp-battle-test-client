#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/IO/Commands.hpp>
#include <Core/IO/ScenarioError.hpp>
#include <Core/Simulation/Simulation.hpp>

#include <cstdint>

namespace sw::common
{
	// `MARCH unitId targetX targetY`: orders a unit with a March component to walk to the target cell, replacing any
	// earlier order. Logs MARCH_STARTED.
	struct MarchCommand
	{
		static constexpr const char* Name = "MARCH";

		uint32_t unitId{};
		uint32_t targetX{};
		uint32_t targetY{};
	};
	SW_REFLECT(MarchCommand, (), (unitId, targetX, targetY))

	[[nodiscard]]
	CommandResult march(Simulation& simulation, const MarchCommand& command);

	void registerCommon(Commands& commands);
}
