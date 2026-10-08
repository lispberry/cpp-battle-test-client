#include <Core/Events/Effects.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Model/Types.hpp>
#include <Core/World/UnitRef.hpp>
#include <Features/Common/Tags.hpp>
#include <Features/Hunter/Poison.hpp>

#include <cstdint>
#include <vector>

namespace sw::hunter
{
	std::vector<Damage> split(const Damage total, const Rounds parts)
	{
		const uint32_t count = parts.get();
		const uint32_t base = total.get() / count;
		const uint32_t remainder = total.get() % count;
		std::vector<Damage> portions;
		portions.reserve(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			portions.emplace_back(i < remainder ? base + 1 : base);
		}
		return portions;
	}

	Poison::Poison(const UnitId source, const Damage total) :
			_source(source),
			_portions(split(total, Duration))
	{}

	Effects Poison::on(const HitTaken& taken)
	{
		if (taken.is<common::Wound>())
		{
			_amplifiedIn = taken.round();
		}
		return {};
	}

	Effects Poison::on(const RoundEnd& end)
	{
		auto portion = _portions.front();
		if (_amplifiedIn == end.round())
		{
			portion = portion * 2;
		}
		_portions.erase(_portions.begin());
		Effects effects;
		if (portion.get() > 0)
		{
			effects = effect::attack(_source, end.self().id(), portion);
		}
		if (_portions.empty())
		{
			effects |= effect::expire();
		}
		return effects;
	}
}
