#include <App/Options.hpp>

#include <charconv>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>

namespace sw
{
	namespace
	{
		std::optional<uint32_t> parseSeed(const std::string_view text)
		{
			uint32_t value = 0;
			const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
			if (error != std::errc{} || end != text.data() + text.size())
			{
				return std::nullopt;
			}
			return value;
		}
	}

	std::optional<Options> parseOptions(const std::span<const char* const> arguments)
	{
		if (arguments.size() == 2)
		{
			return Options{.scenarioPath = arguments[1], .seed = std::nullopt};
		}
		if (arguments.size() != 4 || std::string_view(arguments[2]) != "--seed")
		{
			return std::nullopt;
		}
		const auto seed = parseSeed(arguments[3]);
		if (!seed)
		{
			return std::nullopt;
		}
		return Options{.scenarioPath = arguments[1], .seed = seed};
	}
}
