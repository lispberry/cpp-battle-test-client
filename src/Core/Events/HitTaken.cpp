#include <Core/Events/Context.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Model/Types.hpp>

#include <optional>
#include <utility>

namespace sw
{
	HitTaken::HitTaken(const Context& context, Hit hit) :
			Context(context),
			_hit(std::move(hit))
	{}

	UnitId HitTaken::attacker() const
	{
		return _hit.attacker;
	}

	UnitId HitTaken::target() const
	{
		return _hit.target;
	}

	Damage HitTaken::damage() const
	{
		return _hit.damage;
	}

	std::optional<Ability> HitTaken::ability() const
	{
		return _hit.ability;
	}
}
