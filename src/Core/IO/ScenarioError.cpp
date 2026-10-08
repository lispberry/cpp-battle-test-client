#include <Core/IO/ScenarioError.hpp>

#include <format>
#include <string>
#include <string_view>

namespace sw
{
	namespace
	{
		std::string_view kindText(const ScenarioErrorKind kind)
		{
			switch (kind)
			{
				case ScenarioErrorKind::UnknownCommand: return "unknown command";
				case ScenarioErrorKind::MissingArgument: return "missing argument";
				case ScenarioErrorKind::BadNumber: return "not a non-negative number";
				case ScenarioErrorKind::TrailingInput: return "unexpected extra argument";
				case ScenarioErrorKind::Rejected: return "rejected";
			}
			return "error";
		}
	}

	std::string describe(const ScenarioError& error)
	{
		return std::format("line {}: {} {}", error.line, kindText(error.kind), error.detail);
	}
}
