#include <Core/IO/Commands.hpp>
#include <Core/IO/CoreCommands.hpp>
#include <Core/IO/ScenarioError.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Simulation/Simulation.hpp>

#include <expected>
#include <string>

namespace sw
{
	CommandResult createMap(Simulation& simulation, const CreateMapCommand& command)
	{
		const auto created = simulation.createMap(command.width, command.height);
		if (!created)
		{
			return std::unexpected(std::string(describe(created.error())));
		}
		return {};
	}

	void registerCoreCommands(Commands& commands)
	{
		commands.add<CreateMapCommand>(&createMap);
	}
}
