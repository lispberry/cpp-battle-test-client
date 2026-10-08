#include <Core/Events/Context.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>

namespace sw
{
	HitAttempt::HitAttempt(const Context& context, const Hit hit) :
			Context(context),
			_hit(hit)
	{}

	AttackKind HitAttempt::kind() const
	{
		return _hit.kind;
	}

	UnitId HitAttempt::attacker() const
	{
		return _hit.attacker;
	}

	UnitId HitAttempt::target() const
	{
		return _hit.target;
	}

	Damage HitAttempt::damage() const
	{
		return _hit.damage;
	}
}
