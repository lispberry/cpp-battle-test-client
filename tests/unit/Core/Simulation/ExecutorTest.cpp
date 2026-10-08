#include <Core/Events/Effects.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Query/Units.hpp>
#include <Core/Simulation/Executor.hpp>
#include <Core/Units/Health.hpp>
#include <Core/Units/Unit.hpp>
#include <Core/Units/UnitOf.hpp>
#include <Core/World/GridMap.hpp>
#include <Core/World/UnitRef.hpp>
#include <Core/World/World.hpp>
#include <Features/Common/Attacks.hpp>
#include <Features/Common/Invulnerable.hpp>
#include <Features/Common/March.hpp>
#include <Support/RecordingLog.hpp>
#include <Support/ScriptedRandom.hpp>
#include <Support/TestUnits.hpp>
#include <Support/TestWorld.hpp>

#include <doctest/doctest.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace sw;

namespace
{
	// Replaces every hit with an ability and the attack it deals, like Rending with a sure roll.
	struct Interceptor
	{
		bool active{false};

		[[nodiscard]]
		Effects on(const HitAttempt& attempt) const
		{
			if (!active)
			{
				return {};
			}
			return effect::useAbility(attempt.attacker(), Ability{"intercept"})
				   | effect::attack(attempt.attacker(), attempt.target(), Damage{7}, Ability{"intercept"});
		}
	};

	// Hits back for 1 whenever hit: with plain damage, or through the hit pipeline when `asHit`.
	struct Thorns
	{
		bool active{false};
		bool asHit{false};

		[[nodiscard]]
		Effects on(const HitTaken& taken) const
		{
			if (!active || taken.attacker() == taken.target())
			{
				return {};
			}
			if (asHit)
			{
				return effect::hit(common::Melee, taken.target(), taken.attacker(), Damage{1});
			}
			return effect::attack(taken.target(), taken.attacker(), Damage{1});
		}
	};

	// At round end, deals `damage` to `victim`.
	struct Doom
	{
		UnitId victim;
		Damage damage;

		[[nodiscard]]
		Effects on(const RoundEnd& end) const
		{
			if (damage.get() == 0)
			{
				return {};
			}
			return effect::attack(end.self().id(), victim, damage);
		}
	};

	struct FighterKit
	{
		Health health{Hp{10}};
		Interceptor interceptor;
		Thorns thorns;
		Doom doom;
		test::Script script;
	};
	SW_REFLECT(FighterKit, (), (health, interceptor, thorns, doom, script))

	class Fighter final : public Unit<Fighter, FighterKit>
	{
	public:
		static constexpr UnitName Name{"fighter"};

		using Unit::Unit;

		[[nodiscard]]
		Effects on(const Turn& turn) const
		{
			if (!kit().script.act)
			{
				return {};
			}
			return kit().script.act(turn);
		}
	};

}

TEST_CASE("A hit damages the target and the log carries the total per attacker and target")
{
	test::TestWorld world;
	world.spawn<Fighter>(
			1,
			{.x = 0, .y = 0},
			{.script = test::acts(
					 [](const Turn& turn) -> Effects
					 {
						 Effects actions;
						 const auto target = (turn.units() | first())->id();
						 actions |= effect::hit(common::Melee, turn.self().id(), target, Damage{2});
						 actions |= effect::hit(common::Melee, turn.self().id(), target, Damage{3});
						 return actions;
					 })});
	world.spawn<test::Dummy>(2, {.x = 1, .y = 0});
	world.round();
	const auto attacked = world.log().of<UnitAttacked>();
	REQUIRE(attacked.size() == 1);
	CHECK(attacked.front().damage == 5);
	CHECK(attacked.front().targetHp == 5);
}

TEST_CASE("A unit at 0 hp takes no more damage in the same action")
{
	World battlefield(std::make_unique<GridMap>(5, 5));
	for (uint32_t id = 1; id <= 2; ++id)
	{
		battlefield.units().insert(makeUnit<test::Dummy>(UnitId{id}, {}));
		battlefield.map().place(UnitId{id}, {.origin = {.x = id, .y = 0}});
	}
	Effects actions;
	actions |= effect::attack(UnitId{1}, UnitId{2}, Damage{10});
	actions |= effect::attack(UnitId{1}, UnitId{2}, Damage{3});
	test::ScriptedRandom random;
	test::RecordingLog log;
	Executor executor(battlefield, random, log);
	executor.execute(std::move(actions), Round{1});
	CHECK(log.text()
		  == "[1] UNIT_ATTACKED attackerUnitId=1 targetUnitId=2 damage=10 targetHp=0 \n"
			 "[1] UNIT_DIED unitId=2 \n");
}

TEST_CASE("Damage is tallied per attacker and target pair")
{
	World battlefield(std::make_unique<GridMap>(5, 5));
	for (uint32_t id = 1; id <= 3; ++id)
	{
		battlefield.units().insert(makeUnit<test::Dummy>(UnitId{id}, {}));
		battlefield.map().place(UnitId{id}, {.origin = {.x = id, .y = 0}});
	}
	Effects actions;
	actions |= effect::attack(UnitId{1}, UnitId{2}, Damage{1});
	actions |= effect::attack(UnitId{1}, UnitId{3}, Damage{2});
	actions |= effect::attack(UnitId{3}, UnitId{2}, Damage{3});
	actions |= effect::attack(UnitId{1}, UnitId{2}, Damage{4});
	test::ScriptedRandom random;
	test::RecordingLog log;
	Executor executor(battlefield, random, log);
	executor.execute(std::move(actions), Round{1});
	// The records read as applied in log order: unit 2 goes 10 - 5 = 5, then 5 - 3 = 2 (its real hp at the end).
	CHECK(log.text()
		  == "[1] UNIT_ATTACKED attackerUnitId=1 targetUnitId=2 damage=5 targetHp=5 \n"
			 "[1] UNIT_ATTACKED attackerUnitId=1 targetUnitId=3 damage=2 targetHp=8 \n"
			 "[1] UNIT_ATTACKED attackerUnitId=3 targetUnitId=2 damage=3 targetHp=2 \n");
}

TEST_CASE("Each attack record carries the target's hp right after that attacker's damage")
{
	World battlefield(std::make_unique<GridMap>(5, 5));
	for (uint32_t id = 1; id <= 3; ++id)
	{
		battlefield.units().insert(makeUnit<test::Dummy>(UnitId{id}, {}));
		battlefield.map().place(UnitId{id}, {.origin = {.x = id, .y = 0}});
	}
	Effects actions;
	actions |= effect::attack(UnitId{1}, UnitId{2}, Damage{2});
	actions |= effect::attack(UnitId{3}, UnitId{2}, Damage{1});
	test::ScriptedRandom random;
	test::RecordingLog log;
	Executor executor(battlefield, random, log);
	executor.execute(std::move(actions), Round{1});
	// 10 - 2 = 8 after unit 1, not the 7 the target is left with after the whole action.
	CHECK(log.text()
		  == "[1] UNIT_ATTACKED attackerUnitId=1 targetUnitId=2 damage=2 targetHp=8 \n"
			 "[1] UNIT_ATTACKED attackerUnitId=3 targetUnitId=2 damage=1 targetHp=7 \n");
}

TEST_CASE("Interleaved damage on one target logs hp that only goes down, down to the real final hp")
{
	World battlefield(std::make_unique<GridMap>(5, 5));
	for (uint32_t id = 1; id <= 3; ++id)
	{
		battlefield.units().insert(makeUnit<test::Dummy>(UnitId{id}, {}));
		battlefield.map().place(UnitId{id}, {.origin = {.x = id, .y = 0}});
	}
	test::ScriptedRandom random;
	test::RecordingLog log;
	Executor executor(battlefield, random, log);

	// Two poisons of unit 1 tick around one of unit 3 (two hunters at a round end): 10 -> 6 -> 5.
	Effects ticks;
	ticks |= effect::attack(UnitId{1}, UnitId{2}, Damage{2});
	ticks |= effect::attack(UnitId{3}, UnitId{2}, Damage{1});
	ticks |= effect::attack(UnitId{1}, UnitId{2}, Damage{2});
	executor.execute(std::move(ticks), Round{1});
	CHECK(log.text()
		  == "[1] UNIT_ATTACKED attackerUnitId=1 targetUnitId=2 damage=4 targetHp=6 \n"
			 "[1] UNIT_ATTACKED attackerUnitId=3 targetUnitId=2 damage=1 targetHp=5 \n");

	// The killing damage: 5 -> 0, and the record of the other attacker cannot show more than 0.
	log.clear();
	Effects kill;
	kill |= effect::attack(UnitId{1}, UnitId{2}, Damage{3});
	kill |= effect::attack(UnitId{3}, UnitId{2}, Damage{1});
	kill |= effect::attack(UnitId{1}, UnitId{2}, Damage{3});
	executor.execute(std::move(kill), Round{2});
	CHECK(log.text()
		  == "[2] UNIT_ATTACKED attackerUnitId=1 targetUnitId=2 damage=6 targetHp=0 \n"
			 "[2] UNIT_ATTACKED attackerUnitId=3 targetUnitId=2 damage=1 targetHp=0 \n"
			 "[2] UNIT_DIED unitId=2 \n");
}

TEST_CASE("Hits on missing or Health-less units, applied components on missing units, and a kit expire do nothing")
{
	World battlefield(std::make_unique<GridMap>(5, 5));
	battlefield.units().insert(makeUnit<Fighter>(UnitId{1}, {}));
	battlefield.map().place(UnitId{1}, {.origin = {.x = 0, .y = 0}});
	battlefield.units().insert(makeUnit<test::Statue>(UnitId{2}, {}));
	battlefield.map().place(UnitId{2}, {.origin = {.x = 1, .y = 0}});

	Effects actions;
	actions |= effect::hit(common::Melee, UnitId{1}, UnitId{2}, Damage{5});
	actions |= effect::hit(common::Melee, UnitId{1}, UnitId{99}, Damage{5});
	actions |= effect::attack(UnitId{1}, UnitId{2}, Damage{5});
	actions |= effect::apply(UnitId{99}, common::Invulnerable{}, UnitId{1});
	actions |= effect::unapply(UnitId{99}, UnitId{1});
	actions |= effect::expire();  // from no applied component: nothing to remove
	test::ScriptedRandom random;
	test::RecordingLog log;
	Executor executor(battlefield, random, log);
	executor.execute(std::move(actions), Round{1});
	CHECK(log.records().empty());
	CHECK(battlefield.units().contains(UnitId{2}));
}

TEST_CASE("Changes to a missing unit, or to a component the unit does not have, do nothing")
{
	World battlefield(std::make_unique<GridMap>(5, 5));
	battlefield.units().insert(makeUnit<test::Statue>(UnitId{1}, {}));
	battlefield.map().place(UnitId{1}, {.origin = {.x = 0, .y = 0}});

	bool changed = false;
	Effects actions;
	actions |= effect::change<common::March>(UnitId{1}, [&changed](common::March& /*march*/) { changed = true; });
	actions |= effect::change<common::March>(UnitId{99}, [&changed](common::March& /*march*/) { changed = true; });
	test::ScriptedRandom random;
	test::RecordingLog log;
	Executor executor(battlefield, random, log);
	executor.execute(std::move(actions), Round{1});
	CHECK_FALSE(changed);
	CHECK(log.records().empty());
}

TEST_CASE("Abilities replace hits and reactions run depth-first in recording order")
{
	test::TestWorld world;
	world.spawn<Fighter>(
			1,
			{.x = 0, .y = 0},
			{.interceptor = {.active = true},
			 .script = test::acts(
					 [](const Turn& turn) -> Effects
					 {
						 Effects actions;
						 actions |= effect::hit(
								 common::Melee, turn.self().id(), (turn.units() | first())->id(), Damage{2});
						 actions |= effect::useAbility(turn.self().id(), Ability{"after"});
						 return actions;
					 })});
	world.spawn<Fighter>(2, {.x = 1, .y = 0}, {.thorns = {.active = true}});
	world.round();
	CHECK(world.log().text()
		  == "[1] UNIT_ABILITY_USED abilityUnitId=1 abilityName=intercept \n"
			 "[1] UNIT_ATTACKED attackerUnitId=1 targetUnitId=2 damage=7 targetHp=3 \n"
			 "[1] UNIT_ATTACKED attackerUnitId=2 targetUnitId=1 damage=1 targetHp=9 \n"
			 "[1] UNIT_ABILITY_USED abilityUnitId=1 abilityName=after \n");
}

TEST_CASE("A reaction may itself be a hit, which abilities may replace")
{
	test::TestWorld world;
	world.spawn<Fighter>(
			1,
			{.x = 0, .y = 0},
			{.interceptor = {.active = true},
			 .script = test::acts(
					 [](const Turn& turn) -> Effects
					 {
						 Effects actions;
						 actions |= effect::attack(turn.self().id(), (turn.units() | first())->id(), Damage{2});
						 return actions;
					 })});
	world.spawn<Fighter>(
			2, {.x = 1, .y = 0}, {.interceptor = {.active = true}, .thorns = {.active = true, .asHit = true}});
	world.round();
	// Unit 2's thorns hit goes through HitAttempt, where its own interceptor replaces it.
	CHECK(world.log().text()
		  == "[1] UNIT_ATTACKED attackerUnitId=1 targetUnitId=2 damage=2 targetHp=8 \n"
			 "[1] UNIT_ABILITY_USED abilityUnitId=2 abilityName=intercept \n"
			 "[1] UNIT_ATTACKED attackerUnitId=2 targetUnitId=1 damage=7 targetHp=3 \n");
}

TEST_CASE("Reaction chains are cut at a fixed depth")
{
	// Two units with thorns answer each other's thorns; without the cap only death would end the chain (and with
	// zero-damage thorns, nothing would).
	test::TestWorld world;
	world.spawn<Fighter>(
			1,
			{.x = 0, .y = 0},
			{.thorns = {.active = true},
			 .script = test::acts(
					 [](const Turn& turn) -> Effects
					 {
						 Effects actions;
						 actions |= effect::attack(turn.self().id(), (turn.units() | first())->id(), Damage{0});
						 return actions;
					 })});
	world.spawn<Fighter>(2, {.x = 1, .y = 0}, {.thorns = {.active = true}});
	world.round();
	const auto attacked = world.log().of<UnitAttacked>();
	REQUIRE(attacked.size() == 2);
	// 16 levels of reactions, alternating between the two: 8 thorns hits each way (plus the 0-damage attack).
	CHECK(attacked[0].damage == 8);
	CHECK(attacked[1].damage == 8);
	CHECK(attacked[1].targetHp == 2);
}

TEST_CASE("Moves, effects and reports")
{
	test::TestWorld world;
	world.spawn<Fighter>(
			1,
			{.x = 0, .y = 0},
			{.script = test::acts(
					 [](const Turn& turn) -> Effects
					 {
						 Effects actions;
						 actions |= effect::moveTo(turn.self().id(), {.x = 1, .y = 1});
						 actions |= effect::moveTo(
								 turn.self().id(), {.x = 5, .y = 5});  // not a neighbour any more: dropped
						 actions |= effect::apply(
								 (turn.units() | first())->id(), common::Invulnerable{}, turn.self().id());
						 actions |= effect::report(MarchEnded{.unitId = 1, .x = 1, .y = 1});
						 return actions;
					 })});
	world.spawn<test::Dummy>(2, {.x = 5, .y = 5});
	world.round();
	CHECK(world.unit(1).position() == Position{1, 1});
	CHECK(world.unit(2).has<common::Invulnerable>());
	CHECK(world.log().text()
		  == "[1] UNIT_MOVED unitId=1 x=1 y=1 \n"
			 "[1] MARCH_ENDED unitId=1 x=1 y=1 \n");
}

TEST_CASE("A lethal hit logs the attack, then the death, and frees the cell")
{
	test::TestWorld world;
	world.spawn<Fighter>(
			1,
			{.x = 0, .y = 0},
			{.script = test::acts(
					 [](const Turn& turn) -> Effects
					 {
						 Effects actions;
						 actions |= effect::hit(
								 common::Melee, turn.self().id(), (turn.units() | first())->id(), Damage{50});
						 return actions;
					 })});
	world.spawn<test::Dummy>(2, {.x = 1, .y = 0});
	world.round();
	CHECK(world.log().text()
		  == "[1] UNIT_ATTACKED attackerUnitId=1 targetUnitId=2 damage=50 targetHp=0 \n"
			 "[1] UNIT_DIED unitId=2 \n");
	CHECK_FALSE(world.world().units().contains(UnitId{2}));
	CHECK_FALSE(world.world().map().occupant({.x = 1, .y = 0}).has_value());
}

TEST_CASE("Round end reaches every living unit and skips units killed during it")
{
	test::TestWorld world;
	world.spawn<Fighter>(1, {.x = 0, .y = 0}, {.doom = {.victim = UnitId{2}, .damage = Damage{100}}});
	const auto calls = std::make_shared<std::vector<std::string>>();
	world.spawn<test::Probed>(
			2,
			{.x = 5, .y = 5},
			{.first = {.calls = calls, .label = "a"}, .health = {Hp{10}}, .second = {.calls = calls, .label = "b"}});
	world.spawn<test::Probed>(
			3,
			{.x = 7, .y = 7},
			{.first = {.calls = calls, .label = "c"}, .health = {Hp{10}}, .second = {.calls = calls, .label = "d"}});
	world.spawn<test::Statue>(4, {.x = 9, .y = 9});
	calls->clear();	 // placement queries from spawning
	world.round();
	CHECK(*calls == std::vector<std::string>{"a:hitTaken", "b:hitTaken", "c:roundEnd", "d:roundEnd"});
	CHECK(world.log().text()
		  == "[1] UNIT_ATTACKED attackerUnitId=1 targetUnitId=2 damage=100 targetHp=0 \n"
			 "[1] UNIT_DIED unitId=2 \n");
}
