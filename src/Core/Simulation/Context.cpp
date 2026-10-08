#include <Core/Base/RandomSource.hpp>
#include <Core/Events/Context.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Query/Units.hpp>
#include <Core/World/Map.hpp>
#include <Core/World/UnitRef.hpp>
#include <Core/World/World.hpp>

namespace sw
{
	Context::Context(const World& world, const UnitId self, const Round round, RandomSource& random) :
			_world(&world),
			_self(self),
			_round(round),
			_random(&random)
	{}

	UnitRef Context::self() const
	{
		return {*_world, _self};
	}

	Round Context::round() const
	{
		return _round;
	}

	Units Context::units() const
	{
		return {*_world, _self};
	}

	const Map& Context::map() const
	{
		return _world->map();
	}

	RandomSource& Context::randomSource() const
	{
		return *_random;
	}

	bool Context::roll(const Chance chance) const
	{
		return rolls(chance, *_random);
	}

	const World& Context::world() const
	{
		return *_world;
	}

	UnitId Context::selfId() const
	{
		return _self;
	}
}
