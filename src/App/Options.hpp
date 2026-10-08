#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace sw
{
	inline constexpr std::string_view Usage = "Usage: sw_battle_test <scenario-file> [--seed <uint32>]";

	struct Options
	{
		std::string_view scenarioPath;
		std::optional<uint32_t> seed;
	};

	// Takes argv as is (no copy). Accepts `<program> <scenario-file>` optionally followed by `--seed <uint32>`.
	[[nodiscard]]
	std::optional<Options> parseOptions(std::span<const char* const> arguments);
}
