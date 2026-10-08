#include <Core/IO/Commands.hpp>
#include <Core/IO/ScenarioError.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Simulation/Simulation.hpp>
#include <Core/World/UnitRef.hpp>
#include <Features/Common/March.hpp>
#include <Features/Common/MarchCommand.hpp>

#include <expected>
#include <string>

namespace sw::common
{
	CommandResult march(Simulation& simulation, const MarchCommand& command)
	{
		const UnitId id{command.unitId};
		const Position target{.x = command.targetX, .y = command.targetY};
		if (!simulation.hasMap())
		{
			return std::unexpected(std::string(describe(WorldError::NoMap)));
		}
		if (!simulation.world().map().isInside(target))
		{
			return std::unexpected(std::string(describe(WorldError::OutOfBounds)));
		}
		const auto order = simulation.component<March>(id);
		if (!order)
		{
			return std::unexpected(std::string(describe(order.error())));
		}
		(*order)->target = target;
		const auto from = UnitRef(simulation.world(), id).position();
		simulation.report(
				MarchStarted{
						.unitId = command.unitId,
						.x = from.x,
						.y = from.y,
						.targetX = target.x,
						.targetY = target.y,
				});
		return {};
	}

	void registerCommon(Commands& commands)
	{
		commands.add<MarchCommand>(&march);
	}
}
