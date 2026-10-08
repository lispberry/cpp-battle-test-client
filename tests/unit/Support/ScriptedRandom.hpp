#pragma once

#include <Core/Base/RandomSource.hpp>

#include <cstdint>
#include <deque>
#include <initializer_list>

namespace sw::test
{
	// A RandomSource that returns scripted values, then a fixed fallback: uniform() gives `max` (so a roll succeeds
	// only with Chance{1000}), index() gives 0 (the first candidate).
	class ScriptedRandom final : public RandomSource
	{
	public:
		ScriptedRandom() = default;

		ScriptedRandom(std::initializer_list<uint32_t> values) :
				_values(values)
		{}

		void script(std::initializer_list<uint32_t> values)
		{
			_values.insert(_values.end(), values);
		}

		[[nodiscard]]
		uint32_t uniform(const uint32_t /*min*/, const uint32_t max) override
		{
			++_draws;
			return take(max);
		}

		[[nodiscard]]
		uint32_t index(const uint32_t /*size*/) override
		{
			++_draws;
			return take(0);
		}

		[[nodiscard]]
		int draws() const
		{
			return _draws;
		}

	private:
		uint32_t take(const uint32_t fallback)
		{
			if (_values.empty())
			{
				return fallback;
			}
			const uint32_t value = _values.front();
			_values.pop_front();
			return value;
		}

		std::deque<uint32_t> _values;
		int _draws{0};
	};
}
