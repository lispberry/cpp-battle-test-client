#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/IO/Command.hpp>
#include <Core/IO/ScenarioError.hpp>
#include <Core/Simulation/Simulation.hpp>
#include <Core/Simulation/SpawnOrder.hpp>

#include <charconv>
#include <concepts>
#include <cstddef>
#include <expected>
#include <format>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace sw
{
	/// The scenario commands, by name: a value holding one handler per command. Each line's words are read into the
	/// command's struct, field by field, and handed to its handler with the simulation the line acts on. Features
	/// register their commands here; the application registers the core ones (registerCoreCommands) and the features'
	/// (registerFeatures), then reads the scenario.
	///
	/// \code
	/// Commands commands;
	/// commands.add<CreateMapCommand>(&createMap);           // CommandResult createMap(Simulation&, const CreateMapCommand&)
	/// commands.spawn<SpawnHunterCommand>(&spawnHunter);     // SpawnOrder spawnHunter(const SpawnHunterCommand&)
	/// const auto done = commands.execute("CREATE_MAP 10 10", 1, simulation);
	/// \endcode
	class Commands
	{
	public:
		/// Runs `handler` for every `C` line. An error string from it stops the scenario as ScenarioErrorKind::Rejected.
		/// Throws std::invalid_argument if `C::Name` is already registered (a setup mistake, not a scenario one).
		template <Command C>
		void add(std::function<CommandResult(Simulation&, const C&)> handler)
		{
			const std::string name = C::Name;
			auto run = [call = std::move(handler)](
							   const std::span<const std::string_view> arguments,
							   const std::size_t line,
							   Simulation& simulation) -> std::expected<void, ScenarioError>
			{
				const auto command = parse<C>(arguments, line);
				if (!command)
				{
					return std::unexpected(command.error());
				}
				const auto result = call(simulation, *command);
				if (!result)
				{
					return std::unexpected(
							ScenarioError{.line = line, .kind = ScenarioErrorKind::Rejected, .detail = result.error()});
				}
				return {};
			};
			if (!_handlers.emplace(name, std::move(run)).second)
			{
				throw std::invalid_argument(std::format("command registered twice: {}", name));
			}
		}

		/// A SPAWN_<UNIT> command: `adapter` turns it into a SpawnOrder and the simulation places the unit. A WorldError
		/// (occupied cell, duplicate id...) stops the scenario with its description.
		template <Command C>
		void spawn(SpawnOrder (*adapter)(const C&))
		{
			add<C>(
					[adapter](Simulation& simulation, const C& command) -> CommandResult
					{
						const auto placed = simulation.spawn(adapter(command));
						if (!placed)
						{
							return std::unexpected(std::string(describe(placed.error())));
						}
						return {};
					});
		}

		/// Parses one scenario line and runs it on `simulation`; `lineNumber` only goes into the error. Empty lines and
		/// `//` comments are skipped.
		[[nodiscard]]
		std::expected<void, ScenarioError> execute(
				std::string_view line, std::size_t lineNumber, Simulation& simulation) const;

	private:
		using Handler = std::function<std::expected<void, ScenarioError>(
				std::span<const std::string_view>, std::size_t, Simulation&)>;

		template <Command C>
		static std::expected<C, ScenarioError> parse(
				const std::span<const std::string_view> arguments, const std::size_t line)
		{
			C command{};
			std::size_t index = 0;
			std::expected<void, ScenarioError> status;
			const auto fail = [&status, line](const ScenarioErrorKind kind, std::string detail)
			{
				status = std::unexpected(ScenarioError{.line = line, .kind = kind, .detail = std::move(detail)});
			};
			reflect::forEachNamedField(
					command,
					[&]<std::unsigned_integral F>(std::string_view field, F& value)
					{
						if (!status)
						{
							return;
						}
						if (index >= arguments.size())
						{
							fail(ScenarioErrorKind::MissingArgument, std::string(field));
							return;
						}
						const std::string_view text = arguments[index++];
						const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
						if (error != std::errc{} || end != text.data() + text.size())
						{
							fail(ScenarioErrorKind::BadNumber, std::format("{}={}", field, text));
						}
					});
			if (!status)
			{
				return std::unexpected(status.error());
			}
			if (index < arguments.size())
			{
				return std::unexpected(
						ScenarioError{
								.line = line,
								.kind = ScenarioErrorKind::TrailingInput,
								.detail = std::string(arguments[index]),
						});
			}
			return command;
		}

		std::unordered_map<std::string, Handler> _handlers;
	};
}
