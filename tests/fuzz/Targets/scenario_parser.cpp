// Scenario text, mostly well-formed: reading it either succeeds or reports a line that exists, and the battle it
// describes keeps the world consistent. Here stats come straight from the text (0 damage included), so battles
// may legitimately never end: they are cut after a fixed number of rounds.

#include <Core/Base/MersenneRandom.hpp>
#include <Core/IO/Commands.hpp>
#include <Core/IO/CoreCommands.hpp>
#include <Core/IO/Scenario.hpp>
#include <Core/IO/ScenarioError.hpp>
#include <Core/Simulation/Simulation.hpp>
#include <Features/Features.hpp>
#include <Support/Battle.hpp>
#include <Support/GameObjects.hpp>
#include <Support/Invariants.hpp>
#include <Support/Target.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <sstream>
#include <utility>

namespace
{
	constexpr int MaxRounds = 200;

	void scenarioTextNeverBreaksTheEngine(const sw::fuzz::ScenarioText& scenario)
	{
		auto capture = std::make_unique<sw::fuzz::LogCapture>();
		const auto& log = *capture;
		sw::Simulation simulation(std::make_unique<sw::MersenneRandom>(42), std::move(capture));
		sw::Commands commands;
		sw::registerCoreCommands(commands);
		sw::registerFeatures(commands);

		std::istringstream input(scenario.text);
		const auto read = sw::readScenario(input, commands, simulation);
		if (!read)
		{
			const auto lines = static_cast<std::size_t>(std::ranges::count(scenario.text, '\n'));
			sw::fuzz::expect(read.error().line >= 1 && read.error().line <= lines, "errors name an existing line");
			sw::fuzz::expect(!sw::describe(read.error()).empty(), "errors are described");
			return;
		}
		for (int round = 0; round < MaxRounds && !simulation.isFinished(); ++round)
		{
			simulation.step();
			sw::fuzz::expectConsistent(simulation.world());
		}
		sw::fuzz::expectLogConsistent(log.records());
	}
}

SW_FUZZ_TARGET(scenarioTextNeverBreaksTheEngine)
