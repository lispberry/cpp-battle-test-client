#include <Core/Events/Effects.hpp>
#include <Core/Events/Targeted.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Units/AnyUnit.hpp>
#include <Core/Units/Health.hpp>
#include <Core/World/UnitRef.hpp>
#include <Core/World/World.hpp>

#include <optional>

namespace sw
{
	UnitRef::UnitRef(const World& world, const UnitId id) :
			_world(&world),
			_id(id)
	{}

	UnitId UnitRef::id() const
	{
		return _id;
	}

	UnitName UnitRef::name() const
	{
		return unit().name();
	}

	Placement UnitRef::placement() const
	{
		return _world->map().at(_id);
	}

	Position UnitRef::position() const
	{
		return placement().origin;
	}

	bool UnitRef::isAlive() const
	{
		const auto* found = _world->units().find(_id);
		if (found == nullptr)
		{
			return false;
		}
		const auto* health = found->get<Health>();
		return health == nullptr || health->hp.get() > 0;
	}

	std::optional<Attack> UnitRef::exposureTo(const UnitRef attacker, const Attack attack) const
	{
		Targeted targeted{.attacker = attacker.id(), .exposure = attack};
		unit().dispatch(targeted);
		return targeted.exposure;
	}

	bool UnitRef::isAttackableBy(const UnitRef attacker, const Attack attack) const
	{
		return isAttackableBy(attacker, attacker.placement(), attack);
	}

	bool UnitRef::isAttackableBy(const UnitRef attacker, const Placement& from, const Attack attack) const
	{
		// One registry lookup for the Health check and the Targeted dispatch.
		const AnyUnit& target = unit();
		if (target.get<Health>() == nullptr)
		{
			return false;
		}
		Targeted targeted{.attacker = attacker.id(), .exposure = attack};
		target.dispatch(targeted);
		return targeted.exposure && targeted.exposure->range.contains(distance(from, placement()));
	}

	const AnyUnit& UnitRef::unit() const
	{
		return *_world->units().find(_id);
	}

	Distance distance(const UnitRef& from, const UnitRef& to)
	{
		return distance(from.placement(), to.placement());
	}
}
