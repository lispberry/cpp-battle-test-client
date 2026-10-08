#pragma once

#include <Core/Log/LogSink.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Types.hpp>

#include <ostream>

namespace sw
{
	/// Prints the battle log in the fixed output format: `[round] NAME field=value field=value `, one record per line,
	/// fields in declaration order (note the trailing space).
	///
	/// \code
	/// TextLogPrinter printer(std::cout);
	/// printer.write(Round{0}, MapCreated{.width = 10, .height = 10});
	/// // prints: [0] MAP_CREATED width=10 height=10
	/// \endcode
	class TextLogPrinter final : public LogSink
	{
	public:
		/// Prints to `output` (a stream the program owns, such as std::cout), which must outlive the printer.
		explicit TextLogPrinter(std::ostream& output);

		void write(Round round, const Record& record) override;

	private:
		std::ostream* _output;
	};
}
