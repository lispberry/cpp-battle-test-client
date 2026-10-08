#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

namespace sw
{
	/// What kind of mistake a scenario line has; ScenarioError::detail says which word or field.
	enum class ScenarioErrorKind : uint8_t
	{
		UnknownCommand,
		MissingArgument,
		BadNumber,
		TrailingInput,
		Rejected,
	};

	/// What went wrong on which line of a scenario (lines count from 1).
	struct ScenarioError
	{
		std::size_t line{};
		ScenarioErrorKind kind{ScenarioErrorKind::UnknownCommand};
		std::string detail;
	};

	/// A message for the user, e.g. "line 3: unknown command FOO".
	[[nodiscard]]
	std::string describe(const ScenarioError& error);

	/// What a command handler returns: done, or why the command was rejected, e.g. describe(WorldError).
	///
	/// \code
	/// CommandResult createMap(Simulation& simulation, const CreateMapCommand& command)
	/// {
	///     const auto created = simulation.createMap(command.width, command.height);
	///     if (!created)
	///     {
	///         return std::unexpected(std::string(describe(created.error())));
	///     }
	///     return {};
	/// }
	/// \endcode
	using CommandResult = std::expected<void, std::string>;
}
