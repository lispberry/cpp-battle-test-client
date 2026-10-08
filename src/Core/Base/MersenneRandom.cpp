#include <Core/Base/MersenneRandom.hpp>

#include <cstdint>
#include <limits>
#include <random>

namespace sw
{
	MersenneRandom::MersenneRandom(const uint32_t seed) :
			_seed(seed),
			_engine(seed)
	{}

	uint32_t MersenneRandom::makeSeed()
	{
		// result_type is unsigned int; no cast, since it already is uint32_t where GCC flags one as useless.
		static_assert(sizeof(std::random_device::result_type) == sizeof(uint32_t));
		std::random_device device;
		return device();
	}

	uint32_t MersenneRandom::uniform(const uint32_t min, const uint32_t max)
	{
		if (min == 0 && max == std::numeric_limits<uint32_t>::max())
		{
			return next();
		}
		return min + index(max - min + 1);
	}

	uint32_t MersenneRandom::index(const uint32_t size)
	{
		// Lemire, "Fast Random Integer Generation in an Interval" (2019).
		uint64_t product = uint64_t{next()} * size;
		auto low = static_cast<uint32_t>(product);
		if (low < size)
		{
			const uint32_t threshold = (0U - size) % size;
			while (low < threshold)
			{
				product = uint64_t{next()} * size;
				low = static_cast<uint32_t>(product);
			}
		}
		return static_cast<uint32_t>(product >> 32U);
	}

	uint32_t MersenneRandom::next()
	{
		// mt19937::result_type may be wider than 32 bits (uint_fast32_t), but its values never are.
		return static_cast<uint32_t>(_engine());
	}
}
