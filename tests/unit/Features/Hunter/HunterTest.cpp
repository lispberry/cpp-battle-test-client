#include <Core/Events/Effects.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Events/Tags.hpp>
#include <Core/IO/Commands.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Units/Health.hpp>
#include <Features/Common/Attacks.hpp>
#include <Features/Common/March.hpp>
#include <Features/Common/MeleeAttack.hpp>
#include <Features/Common/RangedAttack.hpp>
#include <Features/Common/Tags.hpp>
#include <Features/Features.hpp>
#include <Features/Hunter/Hunter.hpp>
#include <Features/Hunter/HunterCommands.hpp>
#include <Features/Hunter/Poison.hpp>
#include <Features/Hunter/PoisonArrows.hpp>
#include <Features/Swordsman/Swordsman.hpp>
#include <Support/ScriptedRandom.hpp>
#include <Support/TestUnits.hpp>
#include <Support/TestWorld.hpp>

#include <cstdint>
#include <doctest/doctest.h>
#include <numeric>
#include <optional>
#include <typeindex>
#include <variant>
#include <vector>

using namespace sw;
using hunter::Hunter;
using hunter::HunterKit;
using hunter::Poison;
using hunter::PoisonArrows;

namespace
{
	HunterKit makeHunterKit(const Chance chance = Chance{0}, const std::optional<Position> target = std::nullopt)
	{
		return {.health = {Hp{10}},
				.bow
				= {.range = {.min = Distance{2}, .max = Distance{4}}, .damage = Damage{3}, .holdWhenEngaged = true},
				.knife = {Damage{1}},
				.poison = {.chance = chance, .total = Damage{5}},
				.march = {.speed = Speed{1}, .target = target}};
	}

	std::vector<Damage> poisonTicks(const Effects& reactions)
	{
		std::vector<Damage> ticks;
		for (const auto& attack : test::only<AttackEffect>(reactions))
		{
			ticks.push_back(attack.damage);
		}
		return ticks;
	}
}

TEST_CASE("Hunter shoots only when no unit is adjacent")
{
	test::TestWorld world;
	world.spawn<Hunter>(1, {.x = 2, .y = 2}, makeHunterKit());
	world.spawn<test::Dummy>(2, {.x = 5, .y = 2});

	SUBCASE("free: ranged hit for agility")
	{
		const auto decision = world.decide(1);
		const auto& hit = std::get<HitEffect>(decision[0]);
		CHECK(hit.kind == common::Ranged);
		CHECK(hit.target == UnitId{2});
		CHECK(hit.damage == Damage{3});
	}
	SUBCASE("engaged: melee hit on the neighbour for strength")
	{
		world.spawn<test::Dummy>(3, {.x = 3, .y = 3});
		const auto decision = world.decide(1);
		const auto& hit = std::get<HitEffect>(decision[0]);
		CHECK(hit.kind == common::Melee);
		CHECK(hit.target == UnitId{3});
		CHECK(hit.damage == Damage{1});
	}
}

TEST_CASE("Hunter without a target marches, or stays idle")
{
	test::TestWorld world;
	world.spawn<Hunter>(1, {.x = 0, .y = 0}, makeHunterKit(Chance{0}, Position{0, 2}));
	world.spawn<test::Dummy>(2, {.x = 9, .y = 9});
	CHECK(std::holds_alternative<MoveEffect>(world.decide(1)[0]));
	world.spawn<Hunter>(3, {.x = 0, .y = 5}, makeHunterKit());
	CHECK(world.decide(3).empty());
}

TEST_CASE("PoisonArrows replace a ranged hit with a chance")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	world.random().script({10, 11});
	const PoisonArrows arrows{.chance = Chance{10}, .total = Damage{5}};

	CHECK(arrows.on(HitAttempt(world.context(1), {.kind = common::Melee, .attacker = UnitId{1}, .target = UnitId{2}}))
				  .empty());

	const auto lucky = arrows.on(
			HitAttempt(world.context(1), {.kind = common::Ranged, .attacker = UnitId{1}, .target = UnitId{2}}));
	REQUIRE(lucky.size() == 2);
	CHECK(std::get<AbilityEffect>(lucky[0]).ability == PoisonArrows::Name);
	CHECK(std::get<ApplyEffect>(lucky[1]).target == UnitId{2});
	CHECK(std::get<ApplyEffect>(lucky[1]).source == UnitId{1});
	CHECK(std::get<ApplyEffect>(lucky[1]).component->type() == std::type_index(typeid(Poison)));

	CHECK(arrows.on(HitAttempt(world.context(1), {.kind = common::Ranged, .attacker = UnitId{1}, .target = UnitId{2}}))
				  .empty());
}

TEST_CASE("split spreads the remainder over the first portions and keeps the sum")
{
	const std::vector<Damage> portions = hunter::split(Damage{7}, Rounds{5});
	REQUIRE(portions.size() == 5);
	CHECK(portions == std::vector<Damage>{Damage{2}, Damage{2}, Damage{1}, Damage{1}, Damage{1}});
	const uint32_t sum = std::accumulate(
			portions.begin(),
			portions.end(),
			0U,
			[](const uint32_t total, const Damage part) { return total + part.get(); });
	CHECK(sum == 7);
	CHECK(hunter::split(Damage{0}, Rounds{2}) == std::vector<Damage>{Damage{0}, Damage{0}});
}

TEST_CASE("Poison deals its total over five round ends, double in a round with Rending")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(2, {.x = 0, .y = 0});
	Effects reactions;
	Poison poison(UnitId{1}, Damage{7});
	bool expired = false;
	for (uint32_t round = 1; round <= 5; ++round)
	{
		const auto context = world.context(2, Round{round});
		if (round == 2)
		{
			CHECK(poison.on(HitTaken(
									context,
									{.attacker = UnitId{3},
									 .target = UnitId{2},
									 .damage = Damage{1},
									 .ability = Ability{"rending"},
									 .tags = TagSet(TagList<common::Wound>{})}))
						  .empty());
		}
		CHECK(poison.on(HitTaken(context, {.attacker = UnitId{3}, .target = UnitId{2}})).empty());
		auto ticked = poison.on(RoundEnd(context));
		expired = ticked.removeExpire();
		reactions |= std::move(ticked);
	}
	CHECK(expired);
	CHECK(poisonTicks(reactions) == std::vector<Damage>{Damage{2}, Damage{4}, Damage{1}, Damage{1}, Damage{1}});
	CHECK(test::only<AttackEffect>(reactions).front().attacker == UnitId{1});
	CHECK(test::only<AttackEffect>(reactions).front().target == UnitId{2});

	Poison weak(UnitId{1}, Damage{0});
	auto none = weak.on(RoundEnd(world.context(2)));
	CHECK_FALSE(none.removeExpire());
	CHECK(none.empty());
}

TEST_CASE("Poisoned arrows: the hunter's damage arrives at round end, doubled by a swordsman's Rending")
{
	test::TestWorld world;
	world.spawn<Hunter>(1, {.x = 0, .y = 0}, makeHunterKit(Chance{1000}));
	world.spawn<test::Dummy>(2, {.x = 3, .y = 0});
	world.round();
	CHECK(world.log().text()
		  == "[1] UNIT_ABILITY_USED abilityUnitId=1 abilityName=poison_arrows \n"
			 "[1] UNIT_ATTACKED attackerUnitId=1 targetUnitId=2 damage=1 targetHp=9 \n");

	world.log().clear();
	world.spawn<swordsman::Swordsman>(
			3,
			{.x = 4, .y = 0},
			{.health = {Hp{10}},
			 .sword = {Damage{1}},
			 .rending = {.chance = Chance{1000}, .damage = Damage{1}},
			 .march = {}});
	world.random().script({1});	 // the hunter keeps the dummy over the swordsman, also in range
	world.round();
	// Round 2: the hunter poisons the dummy again (two poisons now), the swordsman rends it, and at round end both
	// poisons deal double: 2 + 2, in one record because both come from the hunter.
	const auto attacked = world.log().of<UnitAttacked>();
	REQUIRE(attacked.size() == 2);
	CHECK(attacked[0].attackerUnitId == 3);
	CHECK(attacked[1].attackerUnitId == 1);
	CHECK(attacked[1].damage == 4);
}

TEST_CASE("Poison of two hunters on one unit: each tick's record shows the hp after it")
{
	test::TestWorld world;
	world.spawn<Hunter>(1, {.x = 0, .y = 0}, makeHunterKit(Chance{1000}));
	world.spawn<test::Dummy>(2, {.x = 3, .y = 0});
	world.spawn<Hunter>(3, {.x = 6, .y = 0}, makeHunterKit(Chance{1000}));
	world.round();
	CHECK(world.log().text()
		  == "[1] UNIT_ABILITY_USED abilityUnitId=1 abilityName=poison_arrows \n"
			 "[1] UNIT_ABILITY_USED abilityUnitId=3 abilityName=poison_arrows \n"
			 "[1] UNIT_ATTACKED attackerUnitId=1 targetUnitId=2 damage=1 targetHp=9 \n"
			 "[1] UNIT_ATTACKED attackerUnitId=3 targetUnitId=2 damage=1 targetHp=8 \n");
}

TEST_CASE("SPAWN_HUNTER creates a hunter and registerFeatures registers every unit")
{
	test::TestWorld world;
	Commands commands;
	registerFeatures(commands);
	REQUIRE(commands.execute("SPAWN_HUNTER 2 2 2 10 1 4 3 100 5", 1, world.simulation()));
	REQUIRE(commands.execute("SPAWN_SWORDSMAN 3 8 8 10 1 100 5", 2, world.simulation()));

	const UnitRef unit = world.unit(2);
	CHECK(unit.name() == UnitName{"hunter"});
	CHECK(unit.position() == Position{2, 2});
	CHECK(unit.get<Health>()->hp == Hp{10});
	CHECK(unit.get<common::RangedAttack>()->damage == Damage{1});
	CHECK(unit.get<common::RangedAttack>()->range.min == Distance{2});
	CHECK(unit.get<common::RangedAttack>()->range.max == Distance{3});
	CHECK(unit.get<common::MeleeAttack>()->damage == Damage{4});
	CHECK(unit.get<PoisonArrows>()->chance == Chance{100});
	CHECK(unit.get<PoisonArrows>()->total == Damage{5});
	CHECK(world.unit(3).name() == UnitName{"swordsman"});
}
