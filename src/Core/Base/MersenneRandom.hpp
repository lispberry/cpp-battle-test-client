#pragma once

#include <Core/Base/RandomSource.hpp>

#include <cstdint>
#include <random>

namespace sw
{
	/// Seeded pseudo-random source, identical on every platform for the same seed, so a battle is reproduced
	/// exactly by `--seed`.
	///
	/// std::mt19937 output is fully specified by the standard, but std::uniform_int_distribution is
	/// implementation-defined (libc++ and libstdc++ give different results), so ranges are mapped here
	/// with Lemire's unbiased multiply-and-reject method instead.
	///
	/// \code
	/// MersenneRandom random(MersenneRandom::makeSeed());
	/// std::println("replay with --seed {}", random.seed());
	/// const uint32_t roll = random.uniform(1, 1000);
	/// \endcode
	class MersenneRandom final : public RandomSource
	{
	public:
		explicit MersenneRandom(uint32_t seed);

		/// A fresh seed from std::random_device, for runs without an explicit seed.
		[[nodiscard]]
		static uint32_t makeSeed();

		/// The seed this source was created with: print it so that the run can be replayed.
		[[nodiscard]]
		uint32_t seed() const noexcept
		{
			return _seed;
		}

		[[nodiscard]]
		uint32_t uniform(uint32_t min, uint32_t max) override;

		[[nodiscard]]
		uint32_t index(uint32_t size) override;

	private:
		uint32_t next();

		uint32_t _seed;
		std::mt19937 _engine;
	};
}
