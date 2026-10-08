#include <Core/IO/Commands.hpp>
#include <Core/IO/ScenarioError.hpp>
#include <Core/Simulation/Simulation.hpp>

#include <algorithm>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sw
{
	namespace
	{
		std::vector<std::string_view> tokenize(const std::string_view line)
		{
			std::vector<std::string_view> tokens;
			std::size_t position = 0;
			while (position < line.size())
			{
				const std::size_t start = line.find_first_not_of(" \t\r", position);
				if (start == std::string_view::npos)
				{
					break;
				}
				const std::size_t end = std::min(line.find_first_of(" \t\r", start), line.size());
				tokens.push_back(line.substr(start, end - start));
				position = end;
			}
			return tokens;
		}
	}

	std::expected<void, ScenarioError> Commands::execute(
			const std::string_view line, const std::size_t lineNumber, Simulation& simulation) const
	{
		const auto tokens = tokenize(line);
		if (tokens.empty() || tokens.front().starts_with("//"))
		{
			return {};
		}
		const auto handler = _handlers.find(std::string(tokens.front()));
		if (handler == _handlers.end())
		{
			return std::unexpected(
					ScenarioError{
							.line = lineNumber,
							.kind = ScenarioErrorKind::UnknownCommand,
							.detail = std::string(tokens.front()),
					});
		}
		return handler->second(std::span(tokens).subspan(1), lineNumber, simulation);
	}
}
