#include <Core/Events/Effects.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>

#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace sw
{
	Effects::Effects(Effect effect)
	{
		_effects.push_back(std::move(effect));
	}

	Effects& Effects::operator|=(Effects other)
	{
		_effects.insert(
				_effects.end(),
				std::make_move_iterator(other._effects.begin()),
				std::make_move_iterator(other._effects.end()));
		return *this;
	}

	bool Effects::empty() const
	{
		return _effects.empty();
	}

	std::size_t Effects::size() const
	{
		return _effects.size();
	}

	const Effect& Effects::operator[](const std::size_t index) const
	{
		return _effects[index];
	}

	std::vector<Effect> Effects::take()
	{
		return std::exchange(_effects, {});
	}

	bool Effects::removeExpire()
	{
		return std::erase_if(
					   _effects, [](const Effect& effect) { return std::holds_alternative<ExpireEffect>(effect); })
			   > 0;
	}

	namespace effect
	{
		Effects hit(const AttackKind kind, const UnitId attacker, const UnitId target, const Damage damage)
		{
			return Effects{HitEffect{.kind = kind, .attacker = attacker, .target = target, .damage = damage}};
		}

		Effects attack(
				const UnitId attacker, const UnitId target, const Damage damage, const std::optional<Ability> ability)
		{
			return Effects{AttackEffect{
					.attacker = attacker,
					.target = target,
					.damage = damage,
					.ability = ability,
					.tags = {},
			}};
		}

		Effects moveTo(const UnitId unit, const Position to)
		{
			return Effects{MoveEffect{.unit = unit, .to = to}};
		}

		Effects unapply(const UnitId target, const UnitId source)
		{
			return Effects{UnapplyEffect{.target = target, .source = source}};
		}

		Effects expire()
		{
			return Effects{ExpireEffect{}};
		}

		Effects useAbility(const UnitId unit, const Ability ability)
		{
			return Effects{AbilityEffect{.unit = unit, .ability = ability}};
		}

		Effects report(Record record)
		{
			return Effects{ReportEffect{std::move(record)}};
		}
	}
}
