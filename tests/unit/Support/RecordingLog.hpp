#pragma once

#include <Core/Log/LogSink.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Log/TextLogPrinter.hpp>
#include <Core/Model/Types.hpp>

#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace sw::test
{
	// Keeps every battle-log record, typed and as printed text.
	class RecordingLog final : public LogSink
	{
	public:
		void write(const Round round, const Record& record) override
		{
			_records.emplace_back(round, record);
			_printer.write(round, record);
		}

		template <class R>
		[[nodiscard]]
		std::vector<R> of() const
		{
			std::vector<R> found;
			for (const auto& [round, record] : _records)
			{
				if (const R* match = std::get_if<R>(&record))
				{
					found.push_back(*match);
				}
			}
			return found;
		}

		[[nodiscard]]
		const std::vector<std::pair<Round, Record>>& records() const
		{
			return _records;
		}

		[[nodiscard]]
		std::string text() const
		{
			return _text.str();
		}

		void clear()
		{
			_records.clear();
			_text.str({});
		}

	private:
		std::vector<std::pair<Round, Record>> _records;
		std::ostringstream _text;
		TextLogPrinter _printer{_text};
	};
}
