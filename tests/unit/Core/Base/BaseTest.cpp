#include <Core/Base/MersenneRandom.hpp>
#include <Core/Base/StrongType.hpp>

#include <cstdint>
#include <doctest/doctest.h>
#include <functional>
#include <limits>

namespace
{
	using Meters = sw::StrongType<uint32_t, struct MetersTag>;
}

TEST_CASE("StrongType wraps, compares and hashes its value")
{
	Meters length{3};
	CHECK(length.get() == 3);
	length.get() = 4;
	const Meters& constant = length;
	CHECK(constant.get() == 4);
	CHECK(Meters{1} < Meters{2});
	CHECK(Meters{2} == Meters{2});
	CHECK(std::hash<Meters>{}(Meters{7}) == std::hash<uint32_t>{}(7));
}

TEST_CASE("MersenneRandom repeats its sequence for the same seed")
{
	sw::MersenneRandom first(42);
	sw::MersenneRandom second(42);
	CHECK(first.seed() == 42);
	for (int i = 0; i < 100; ++i)
	{
		const uint32_t value = first.uniform(1, 6);
		CHECK(value == second.uniform(1, 6));
		CHECK(value >= 1);
		CHECK(value <= 6);
	}
}

TEST_CASE("MersenneRandom covers the full range and rejects biased draws")
{
	sw::MersenneRandom random(7);
	(void)random.uniform(0, std::numeric_limits<uint32_t>::max());
	CHECK(random.uniform(0, 5) <= 5);

	// A size just above 2^31 rejects about half of the raw draws (Lemire's threshold), so the retry loop runs.
	constexpr uint32_t size = 0x80000001U;
	for (int i = 0; i < 64; ++i)
	{
		CHECK(random.index(size) < size);
	}
}

TEST_CASE("MersenneRandom::makeSeed returns a seed")
{
	const uint32_t seed = sw::MersenneRandom::makeSeed();
	sw::MersenneRandom random(seed);
	CHECK(random.seed() == seed);
}
