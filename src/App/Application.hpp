#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <span>

namespace sw
{
	// Reads and runs a scenario: the battle log goes to `output`, problems to `errors`. Returns the exit code.
	[[nodiscard]]
	int runScenario(std::istream& scenario, uint32_t seed, std::ostream& output, std::ostream& errors);

	// The command line program: `sw_battle_test <scenario-file> [--seed <uint32>]`.
	[[nodiscard]]
	int run(std::span<const char* const> arguments, std::ostream& output, std::ostream& errors);
}
