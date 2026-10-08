#pragma once

#include <Core/Log/Records.hpp>
#include <Core/Model/Types.hpp>

namespace sw
{
	/// Where the battle log goes: TextLogPrinter on stdout in the application, `RecordingLog` in tests
	/// (`tests/unit/Support`). Features do not write here directly; they report through their Turn or the Simulation.
	///
	/// \code
	/// class CountingLog final : public LogSink
	/// {
	/// public:
	///     void write(Round, const Record&) override { ++count; }
	///     int count = 0;
	/// };
	/// \endcode
	class LogSink
	{
	public:
		virtual ~LogSink() = default;

		/// Logs `record` as having happened in `round`.
		virtual void write(Round round, const Record& record) = 0;

	protected:
		LogSink() = default;
		LogSink(const LogSink&) = default;
		LogSink(LogSink&&) = default;
		LogSink& operator=(const LogSink&) = default;
		LogSink& operator=(LogSink&&) = default;
	};
}
