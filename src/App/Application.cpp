#include <App/Application.hpp>
#include <App/Options.hpp>
#include <Core/Base/MersenneRandom.hpp>
#include <Core/IO/Commands.hpp>
#include <Core/IO/CoreCommands.hpp>
#include <Core/IO/Scenario.hpp>
#include <Core/IO/ScenarioError.hpp>
#include <Core/Log/TextLogPrinter.hpp>
#include <Core/Simulation/Simulation.hpp>
#include <Features/Features.hpp>

#include <cstdint>
#include <cstdlib>
#include <expected>
#include <fstream>
#include <istream>
#include <memory>
#include <optional>
#include <ostream>
#include <print>
#include <span>
#include <string>

namespace sw
{
	int runScenario(std::istream& scenario, const uint32_t seed, std::ostream& output, std::ostream& errors)
	{
		Simulation simulation(std::make_unique<MersenneRandom>(seed), std::make_unique<TextLogPrinter>(output));

		Commands commands;
		registerCoreCommands(commands);
		registerFeatures(commands);

		if (const auto read = readScenario(scenario, commands, simulation); !read)
		{
			std::println(errors, "Error: {}", describe(read.error()));
			return EXIT_FAILURE;
		}
		simulation.run();
		return EXIT_SUCCESS;
	}

	int run(const std::span<const char* const> arguments, std::ostream& output, std::ostream& errors)
	{
		const auto options = parseOptions(arguments);
		if (!options)
		{
			std::println(errors, "{}", Usage);
			return EXIT_FAILURE;
		}

		std::ifstream file{std::string(options->scenarioPath)};
		if (!file)
		{
			std::println(errors, "Error: File not found - {}", options->scenarioPath);
			return EXIT_FAILURE;
		}

		// Without --seed the run is random; the seed goes to stderr (stdout format is fixed) so it can be replayed.
		const uint32_t seed = options->seed.value_or(MersenneRandom::makeSeed());
		if (!options->seed)
		{
			std::println(errors, "seed: {}", seed);
		}
		return runScenario(file, seed, output, errors);
	}
}
