#include <Core/Base/RandomSource.hpp>
#include <Core/Model/Types.hpp>

#include <cstdint>

namespace sw
{
	Hp operator-(const Hp hp, const Damage damage)
	{
		if (damage.get() >= hp.get())
		{
			return Hp{0};
		}
		return Hp{hp.get() - damage.get()};
	}

	Damage operator+(const Damage left, const Damage right)
	{
		return Damage{left.get() + right.get()};
	}

	Damage operator*(const Damage damage, const uint32_t factor)
	{
		return Damage{damage.get() * factor};
	}

	Round next(const Round round)
	{
		return Round{round.get() + 1};
	}

	bool rolls(const Chance chance, RandomSource& random)
	{
		return random.uniform(1, 1000) <= chance.get();
	}
}
