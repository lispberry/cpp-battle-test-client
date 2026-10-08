#include <Core/Events/Effects.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/IO/Commands.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Units/Health.hpp>
#include <Features/Common/Attacks.hpp>
#include <Features/Common/March.hpp>
#include <Features/Common/MeleeAttack.hpp>
#include <Features/Common/Tags.hpp>
#include <Features/Swordsman/Rending.hpp>
#include <Features/Swordsman/Swordsman.hpp>
#include <Features/Swordsman/SwordsmanCommands.hpp>
#include <Support/ScriptedRandom.hpp>
#include <Support/TestUnits.hpp>
#include <Support/TestWorld.hpp>

#include <doctest/doctest.h>
#include <optional>
#include <variant>

using namespace sw;
using swordsman::Rending;
using swordsman::Swordsman;
using swordsman::SwordsmanKit;

namespace
{
	SwordsmanKit makeSwordsmanKit(const Chance chance = Chance{0}, const std::optional<Position> target = std::nullopt)
	{
		return {.health = {Hp{10}},
				.sword = {Damage{2}},
				.rending = {.chance = chance, .damage = Damage{5}},
				.march = {.speed = Speed{1}, .target = target}};
	}
}

TEST_CASE("Rending replaces a melee hit with a chance")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	world.random().script({10, 11});
	const Rending rending{.chance = Chance{10}, .damage = Damage{5}};

	CHECK(rending.on(HitAttempt(world.context(1), {.kind = common::Ranged, .attacker = UnitId{1}, .target = UnitId{2}}))
				  .empty());
	CHECK(world.random().draws() == 0);

	const auto lucky = rending.on(
			HitAttempt(world.context(1), {.kind = common::Melee, .attacker = UnitId{1}, .target = UnitId{2}}));
	REQUIRE(lucky.size() == 2);
	CHECK(std::get<AbilityEffect>(lucky[0]).ability == Rending::Name);
	CHECK(std::get<AttackEffect>(lucky[1]).damage == Damage{5});
	CHECK(std::get<AttackEffect>(lucky[1]).ability == Rending::Name);
	CHECK(std::get<AttackEffect>(lucky[1]).tags.has<common::Wound>());

	CHECK(rending.on(HitAttempt(world.context(1), {.kind = common::Melee, .attacker = UnitId{1}, .target = UnitId{2}}))
				  .empty());
}

TEST_CASE("Swordsman strikes an adjacent unit, otherwise marches")
{
	test::TestWorld world;
	world.spawn<Swordsman>(1, {.x = 0, .y = 0}, makeSwordsmanKit(Chance{0}, Position{3, 0}));
	const auto marching = world.decide(1);
	CHECK(std::holds_alternative<MoveEffect>(marching[0]));

	world.spawn<test::Dummy>(2, {.x = 1, .y = 1});
	const auto striking = world.decide(1);
	CHECK(std::get<HitEffect>(striking[0]).target == UnitId{2});
}

TEST_CASE("A rending swordsman logs the ability, then the attack")
{
	test::TestWorld world;
	world.spawn<Swordsman>(1, {.x = 0, .y = 0}, makeSwordsmanKit(Chance{1000}));
	world.spawn<test::Dummy>(2, {.x = 1, .y = 0});
	world.round();
	CHECK(world.log().text()
		  == "[1] UNIT_ABILITY_USED abilityUnitId=1 abilityName=rending \n"
			 "[1] UNIT_ATTACKED attackerUnitId=1 targetUnitId=2 damage=5 targetHp=5 \n");
}

TEST_CASE("SPAWN_SWORDSMAN creates a swordsman from the command")
{
	test::TestWorld world;
	Commands commands;
	swordsman::registerSwordsman(commands);
	REQUIRE(commands.execute("SPAWN_SWORDSMAN 1 5 2 10 3 100 5", 1, world.simulation()));

	const UnitRef unit = world.unit(1);
	CHECK(unit.name() == UnitName{"swordsman"});
	CHECK(unit.position() == Position{5, 2});
	CHECK(unit.get<Health>()->hp == Hp{10});
	CHECK(unit.get<common::MeleeAttack>()->damage == Damage{3});
	CHECK(unit.get<Rending>()->chance == Chance{100});
	CHECK(unit.get<Rending>()->damage == Damage{5});
	CHECK_FALSE(unit.get<common::March>()->target.has_value());
}
