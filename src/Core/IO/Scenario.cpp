#include <Core/IO/Commands.hpp>
#include <Core/IO/Scenario.hpp>
#include <Core/IO/ScenarioError.hpp>
#include <Core/Simulation/Simulation.hpp>

#include <cstddef>
#include <expected>
#include <istream>
#include <string>

namespace sw
{
	std::expected<void, ScenarioError> readScenario(
			std::istream& scenario, const Commands& commands, Simulation& simulation)
	{
		std::string line;
		std::size_t number = 0;
		while (std::getline(scenario, line))
		{
			++number;
			if (auto result = commands.execute(line, number, simulation); !result)
			{
				return result;
			}
		}
		return {};
	}
}
