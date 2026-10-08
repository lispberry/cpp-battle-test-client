#pragma once

#include <cstdint>

namespace sw
{
	/// The battle's only source of randomness. Production code uses MersenneRandom; tests script the values
	/// (`ScriptedRandom` in `tests/unit/Support`), so every random decision can be tested.
	///
	/// \code
	/// void throwDice(RandomSource& random)
	/// {
	///     const uint32_t roll = random.uniform(1, 6);    // 1..6
	///     const uint32_t pick = random.index(3);         // 0, 1 or 2
	/// }
	/// \endcode
	class RandomSource
	{
	public:
		virtual ~RandomSource() = default;

		/// Uniform integer in `[min, max]`, both inclusive. Requires `min <= max`.
		[[nodiscard]]
		virtual uint32_t uniform(uint32_t min, uint32_t max) = 0;

		/// Uniform index in `[0, size)`, e.g. to pick an element of a container. Requires `size > 0`.
		[[nodiscard]]
		virtual uint32_t index(uint32_t size) = 0;

		/// True with probability `1 / n`: the step of a one-pass uniform pick (reservoir sampling), where the k-th
		/// candidate replaces the pick if `oneIn(k)`. Requires `n > 0`.
		[[nodiscard]]
		bool oneIn(const uint32_t n)
		{
			return index(n) == 0;
		}

	protected:
		RandomSource() = default;
		RandomSource(const RandomSource&) = default;
		RandomSource(RandomSource&&) = default;
		RandomSource& operator=(const RandomSource&) = default;
		RandomSource& operator=(RandomSource&&) = default;
	};
}
