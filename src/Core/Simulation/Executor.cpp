#include <Core/Base/Concepts.hpp>
#include <Core/Base/RandomSource.hpp>
#include <Core/Events/Context.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Log/LogSink.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Simulation/Executor.hpp>
#include <Core/Units/AnyUnit.hpp>
#include <Core/Units/Health.hpp>
#include <Core/World/World.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace sw
{
	namespace
	{
		// A reaction may trigger reactions (a hit triggers thorns, whose damage triggers more thorns...). Chains deeper
		// than this are cut, so no combination of hooks can loop forever. It also bounds the recursion of run.
		constexpr std::size_t MaxReactionDepth = 16;

		// A target's hp as the records written so far show it.
		struct Logged
		{
			uint32_t target;
			Hp hp;
		};
	}

	Executor::Executor(World& world, RandomSource& random, LogSink& log) :
			_world(world),
			_random(random),
			_log(log)
	{}

	void Executor::execute(Effects effects, const Round round)
	{
		_round = round;
		for (Effect& effect : effects.take())
		{
			run(effect, 0);
		}
		flush();
	}

	void Executor::endRound(const Round round)
	{
		_round = round;
		// The order does not change during a round: dead units stay in the registry until it ends.
		for (const UnitId id : _world.get().units().order())
		{
			AnyUnit& unit = *_world.get().units().find(id);
			if (unit.isDead())
			{
				continue;
			}
			runAll(unit.dispatch(RoundEnd(context(id))), 0);
		}
		flush();
	}

	// Performs `effect`, then the effects its handlers returned, before the caller moves on.
	void Executor::run(Effect& effect, const std::size_t depth)
	{
		auto caused = std::visit([this]<ClassType E>(E& alternative) { return this->apply(alternative); }, effect);
		runAll(std::move(caused), depth + 1);
	}

	// Caused effects run now, depth-first, in the order they were returned; too deep a chain is cut.
	void Executor::runAll(Effects effects, const std::size_t depth)
	{
		if (depth > MaxReactionDepth)
		{
			return;
		}

		for (Effect& effect : effects.take())
		{
			run(effect, depth);
		}
	}

	Context Executor::context(const UnitId self) const
	{
		return {_world.get(), self, _round, _random.get()};
	}

	// The hit pipeline: an ability of the attacker may take the hit over (HitAttempt); if none did, plain damage.
	Effects Executor::apply(const HitEffect& hit)
	{
		if (livingHealth(hit.target) == nullptr)
		{
			return {};
		}

		const HitAttempt attempt(
				context(hit.attacker),
				{.kind = hit.kind, .attacker = hit.attacker, .target = hit.target, .damage = hit.damage});
		if (auto instead = _world.get().units().find(hit.attacker)->dispatch(attempt); !instead.empty())
		{
			return instead;
		}
		return damage(hit.attacker, hit.target, hit.damage, std::nullopt, TagSet{});
	}

	Effects Executor::apply(const AttackEffect& attack)
	{
		return damage(attack.attacker, attack.target, attack.damage, attack.ability, attack.tags);
	}

	Effects Executor::apply(const MoveEffect& move)
	{
		if (_world.get().map().canStep(move.unit, move.to))
		{
			_world.get().map().move(move.unit, move.to);
			_records.emplace_back(UnitMoved{.unitId = move.unit.get(), .x = move.to.x, .y = move.to.y});
		}
		return {};
	}

	Effects Executor::apply(ApplyEffect& application)
	{
		if (auto* target = _world.get().units().find(application.target); target != nullptr)
		{
			target->applied().add(std::move(application.component), application.source);
		}
		return {};
	}

	Effects Executor::apply(const UnapplyEffect& removal)
	{
		if (auto* target = _world.get().units().find(removal.target); target != nullptr)
		{
			target->applied().remove(removal.source);
		}
		return {};
	}

	Effects Executor::apply(const AbilityEffect& ability)
	{
		_records.emplace_back(
				UnitAbilityUsed{
						.abilityUnitId = ability.unit.get(),
						.abilityName = std::string(ability.ability.get()),
				});
		return {};
	}

	Effects Executor::apply(ReportEffect& report)
	{
		_records.push_back(std::move(report.record));
		return {};
	}

	Effects Executor::apply(const ChangeEffect& change)
	{
		if (auto* unit = _world.get().units().find(change.unit); unit != nullptr)
		{
			change.change(*unit);
		}
		return {};
	}

	// An expire from a kit component: the kit stays for the unit's life. Applied components' expires never get here.
	Effects Executor::apply(const ExpireEffect& /*expire*/)
	{
		return {};
	}

	// Damage pipeline: lower the target's hp, count it for the log, then let the target react (HitTaken).
	Effects Executor::damage(
			const UnitId attacker,
			const UnitId target,
			const Damage amount,
			const std::optional<Ability> ability,
			const TagSet& tags)
	{
		auto* health = livingHealth(target);
		if (health == nullptr)
		{
			return {};
		}

		tally(attacker, target, amount, health->hp);
		health->hp = health->hp - amount;
		if (health->hp.get() == 0)
		{
			_killed.push_back(target);
		}

		const HitTaken taken(
				context(target),
				{.attacker = attacker, .target = target, .damage = amount, .ability = ability, .tags = tags});
		return _world.get().units().find(target)->dispatch(taken);
	}

	// The target's Health if it exists and is above zero.
	Health* Executor::livingHealth(const UnitId id)
	{
		auto* unit = _world.get().units().find(id);
		if (unit == nullptr)
		{
			return nullptr;
		}
		auto* health = unit->get<Health>();
		if (health == nullptr || health->hp.get() == 0)
		{
			return nullptr;
		}
		return health;
	}

	// Adds `amount` to the UNIT_ATTACKED of (attacker, target), or starts one. Until flush, a record's targetHp holds
	// the target's hp before that record's first damage.
	void Executor::tally(const UnitId attacker, const UnitId target, const Damage amount, const Hp hpBefore)
	{
		for (Record& record : _records)
		{
			auto* attacked = std::get_if<UnitAttacked>(&record);
			if (attacked != nullptr && attacked->attackerUnitId == attacker.get()
				&& attacked->targetUnitId == target.get())
			{
				attacked->damage += amount.get();
				return;
			}
		}
		_records.emplace_back(
				UnitAttacked{
						.attackerUnitId = attacker.get(),
						.targetUnitId = target.get(),
						.damage = amount.get(),
						.targetHp = hpBefore.get(),
				});
	}

	// Writes the log of the action, then takes the units it killed off the map.
	//
	// The UNIT_ATTACKED records of one target read as if applied in log order: each one's hp is the previous one's minus
	// its damage, from the hp before the action, so the last one shows the real hp even when attackers interleave (two
	// hunters' poisons ticking in turn). The first record of a target was started by its first damage, so its targetHp
	// is the hp before the action.
	void Executor::flush()
	{
		std::vector<Logged> logged;
		for (Record& record : _records)
		{
			if (auto* attacked = std::get_if<UnitAttacked>(&record))
			{
				auto known = std::ranges::find(logged, attacked->targetUnitId, &Logged::target);
				if (known == logged.end())
				{
					known = logged.insert(
							logged.end(), Logged{.target = attacked->targetUnitId, .hp = Hp{attacked->targetHp}});
				}
				known->hp = known->hp - Damage{attacked->damage};
				attacked->targetHp = known->hp.get();
			}
			_log.get().write(_round, record);
		}
		_records.clear();
		buryKilled();
	}

	// The units this action killed die in creation order: logged, and their cells freed. A unit reaches 0 hp once, so
	// each is buried once.
	void Executor::buryKilled()
	{
		const auto& units = _world.get().units();
		std::ranges::sort(
				_killed, [&units](const UnitId left, const UnitId right) { return units.createdBefore(left, right); });

		for (const UnitId id : _killed)
		{
			_log.get().write(_round, UnitDied{.unitId = id.get()});
			_world.get().map().remove(id);
		}
		_killed.clear();
	}
}
