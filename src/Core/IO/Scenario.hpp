#pragma once

#include <Core/IO/Commands.hpp>
#include <Core/IO/ScenarioError.hpp>
#include <Core/Simulation/Simulation.hpp>

#include <expected>
#include <istream>

namespace sw
{
	/// Runs every line of the scenario text `scenario` through `commands` on `simulation`, in order: one command per line, arguments
	/// separated by spaces or tabs; empty lines and lines starting with `//` are skipped. Stops at the first error;
	/// the commands before it have run.
	///
	/// \code
	/// std::ifstream file("scenario.txt");
	/// if (const auto read = readScenario(file, commands, simulation); !read)
	/// {
	///     std::println(stderr, "Error: {}", describe(read.error()));
	/// }
	/// \endcode
	[[nodiscard]]
	std::expected<void, ScenarioError> readScenario(
			std::istream& scenario, const Commands& commands, Simulation& simulation);
}
