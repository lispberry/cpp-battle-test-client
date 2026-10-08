#include <Core/Base/Reflect.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Log/TextLogPrinter.hpp>
#include <Core/Model/Types.hpp>

#include <format>
#include <ostream>
#include <print>
#include <string_view>
#include <variant>

namespace sw
{
	namespace
	{
		template <class R>
		concept LogRecord = requires { R::Name; };
	}

	TextLogPrinter::TextLogPrinter(std::ostream& output) :
			_output(&output)
	{}

	void TextLogPrinter::write(const Round round, const Record& record)
	{
		std::visit(
				[&output = *_output, round]<LogRecord R>(const R& entry)
				{
					std::print(output, "[{}] {} ", round.get(), R::Name);
					reflect::forEachNamedField(
							entry,
							[&output]<std::formattable<char> V>(const std::string_view name, const V& value)
							{ std::print(output, "{}={} ", name, value); });
					std::print(output, "\n");
				},
				record);
	}
}
