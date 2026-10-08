#include <Core/Events/Effects.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Events/SpeedQuery.hpp>
#include <Core/Events/Targeted.hpp>
#include <Core/IO/Commands.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Query/Units.hpp>
#include <Core/Units/Health.hpp>
#include <Features/Blueprint/Blueprint.hpp>
#include <Features/Common/Attacks.hpp>
#include <Support/ScriptedRandom.hpp>
#include <Support/TestUnits.hpp>
#include <Support/TestWorld.hpp>

#include <cstdint>
#include <doctest/doctest.h>
#include <optional>
#include <typeindex>
#include <variant>
#include <vector>

using namespace sw;
using namespace sw::blueprint;

namespace
{
	BlueprintKit makeBlueprintKit(const Hp hp = Hp{10}, const std::optional<Position> target = std::nullopt)
	{
		return {.health = {hp},
				.body = {},
				.ward = {Distance{3}},
				.strike = {Damage{2}},
				.volley = {.range = {.min = Distance{2}, .max = Distance{4}}, .damage = Damage{1}},
				.ember = {.chance = Chance{0}, .burn = Damage{1}, .rounds = Rounds{2}},
				.retaliation = {Damage{1}},
				.swiftness = {Speed{1}},
				.vigil = {Rounds{2}},
				.march = {.speed = Speed{1}, .target = target}};
	}
}

TEST_CASE("Ward refuses attacks that only work from afar")
{
	const Ward ward{Distance{3}};
	const Targeted near{.attacker = UnitId{1}, .exposure = common::ranged({.min = Distance{2}, .max = Distance{5}})};
	REQUIRE(ward.on(near).has_value());
	CHECK(ward.on(near)->range.max == Distance{5});
	const Targeted far{.attacker = UnitId{1}, .exposure = common::ranged({.min = Distance{4}, .max = Distance{6}})};
	CHECK_FALSE(ward.on(far).has_value());
}

TEST_CASE("Ember sets the target on fire instead of hitting it, with a chance")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	world.random().script({5, 6});
	const Ember ember{.chance = Chance{5}, .burn = Damage{1}, .rounds = Rounds{2}};
	const auto lucky = ember.on(HitAttempt(world.context(1), {.attacker = UnitId{1}, .target = UnitId{2}}));
	CHECK(test::only<AbilityEffect>(lucky).front().ability == Ember::Name);
	CHECK(std::get<ApplyEffect>(lucky[1]).component->type() == std::type_index(typeid(Burn)));
	CHECK(ember.on(HitAttempt(world.context(1), {.attacker = UnitId{1}, .target = UnitId{2}})).empty());
}

TEST_CASE("Retaliation, Swiftness and Vigil")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(2, {.x = 0, .y = 0});
	const Retaliation retaliation{Damage{2}};
	auto reactions = retaliation.on(HitTaken(world.context(2), {.attacker = UnitId{1}, .target = UnitId{2}}));
	reactions |= retaliation.on(HitTaken(world.context(2), {.attacker = UnitId{2}, .target = UnitId{2}}));
	reactions |= retaliation.on(
			HitTaken(world.context(2), {.attacker = UnitId{1}, .target = UnitId{2}, .ability = Retaliation::Name}));
	const auto attacks = test::only<AttackEffect>(reactions);
	REQUIRE(attacks.size() == 1);
	CHECK(attacks.front().target == UnitId{1});
	CHECK(attacks.front().ability == Retaliation::Name);

	CHECK(Swiftness{Speed{2}}.on(SpeedQuery{.speed = Speed{1}}) == Speed{3});

	const Vigil vigil{Rounds{2}};
	auto rounds = vigil.on(RoundEnd(world.context(2, Round{1})));
	rounds |= vigil.on(RoundEnd(world.context(2, Round{2})));
	const auto abilities = test::only<AbilityEffect>(rounds);
	REQUIRE(abilities.size() == 1);
	CHECK(abilities.front().ability == Vigil::Name);
}

TEST_CASE("Burn deals its damage at each round end, then expires")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(3, {.x = 0, .y = 0});
	Burn burn{.source = UnitId{1}, .damage = Damage{2}, .left = Rounds{2}};
	auto first = burn.on(RoundEnd(world.context(3, Round{1})));
	CHECK_FALSE(first.removeExpire());
	auto second = burn.on(RoundEnd(world.context(3, Round{2})));
	CHECK(second.removeExpire());
	const auto attacks = test::only<AttackEffect>(std::move(first) | std::move(second));
	REQUIRE(attacks.size() == 2);
	CHECK(attacks.front().attacker == UnitId{1});
	CHECK(attacks.front().target == UnitId{3});
}

TEST_CASE("fragile selects damageable units at or below a threshold")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0}, {.health = {Hp{3}}});
	world.spawn<test::Dummy>(2, {.x = 1, .y = 0}, {.health = {Hp{4}}});
	world.spawn<test::Statue>(3, {.x = 2, .y = 0});
	const Units units(world.world(), std::nullopt);
	CHECK((units | fragile(Hp{3}) | count()) == 1);
	CHECK((units | fragile(Hp{3}) | first())->id() == UnitId{1});
}

TEST_CASE("Blueprint decides: finish a fragile neighbour, hit a neighbour, shoot, march")
{
	test::TestWorld world;
	world.spawn<Blueprint>(1, {.x = 5, .y = 5}, makeBlueprintKit(Hp{10}, Position{5, 0}));
	CHECK(std::holds_alternative<MoveEffect>(world.decide(1)[0]));

	world.spawn<test::Dummy>(2, {.x = 8, .y = 5});
	CHECK(std::get<HitEffect>(world.decide(1)[0]).kind == common::Ranged);

	world.spawn<test::Dummy>(3, {.x = 6, .y = 5});
	const auto strike = std::get<HitEffect>(world.decide(1)[0]);
	CHECK(strike.kind == common::Melee);
	CHECK(strike.target == UnitId{3});

	world.spawn<test::Dummy>(4, {.x = 4, .y = 5}, {.health = {Hp{2}}});
	const auto finish = std::get<HitEffect>(world.decide(1)[0]);
	CHECK(finish.target == UnitId{4});
	CHECK(finish.damage == Damage{2});
}

TEST_CASE("SPAWN_BLUEPRINT and a blueprint battle")
{
	test::TestWorld world;
	Commands commands;
	registerBlueprint(commands);
	REQUIRE(commands.execute("SPAWN_BLUEPRINT 1 0 0 10", 1, world.simulation()));
	REQUIRE(commands.execute("SPAWN_BLUEPRINT 2 1 0 10", 2, world.simulation()));
	CHECK(world.unit(1).name() == UnitName{"blueprint"});
	CHECK(world.unit(2).get<Health>()->hp == Hp{10});
	world.round();
	world.round();
	// Each strike is answered by Retaliation; Vigil fires on round 2.
	CHECK(world.log().of<UnitAttacked>().size() == 8);
	CHECK(world.log().of<UnitAbilityUsed>().size() == 2);
}
