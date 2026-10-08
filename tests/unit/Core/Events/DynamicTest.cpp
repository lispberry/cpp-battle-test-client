#include <Core/Events/Dynamic.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/PlacementQuery.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Events/SpeedQuery.hpp>
#include <Core/Events/Targeted.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Query/Units.hpp>
#include <Core/Units/Unit.hpp>
#include <Core/World/UnitRef.hpp>
#include <Features/Common/Attacks.hpp>
#include <Features/Common/Timed.hpp>
#include <Support/TestUnits.hpp>
#include <Support/TestWorld.hpp>

#include <doctest/doctest.h>
#include <optional>
#include <variant>

using namespace sw;

static_assert(Applicable<Dynamic>);
static_assert(Acting<Dynamic>);

TEST_CASE("Dynamic without functions causes nothing and keeps every answer")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	const Dynamic none;

	CHECK(none.on(Turn(world.world(), UnitId{1}, Round{1}, world.random())).empty());
	CHECK(none.on(HitAttempt(world.context(1), {.attacker = UnitId{1}, .target = UnitId{2}})).empty());
	CHECK(none.on(HitTaken(world.context(1), {.attacker = UnitId{2}, .target = UnitId{1}})).empty());
	CHECK(none.on(RoundEnd(world.context(1))).empty());
	CHECK(none.on(Targeted{.attacker = UnitId{2}, .exposure = common::melee()})->kind == common::Melee);
	CHECK(none.on(PlacementQuery{.placement = {.origin = {.x = 3, .y = 4}}}).origin == Position{3, 4});
	CHECK(none.on(SpeedQuery{.speed = Speed{2}}) == Speed{2});
}

TEST_CASE("Dynamic runs the function given for each event and question")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	const Dynamic dynamic{
			.onTurn = [](const Turn& turn) { return effect::useAbility(turn.self().id(), Ability{"turn"}); },
			.onHitAttempt
			= [](const HitAttempt& attempt) { return effect::useAbility(attempt.attacker(), Ability{"attempt"}); },
			.onHitTaken = [](const HitTaken& taken) { return effect::useAbility(taken.target(), Ability{"taken"}); },
			.onRoundEnd = [](const RoundEnd& end) { return effect::useAbility(end.self().id(), Ability{"end"}); },
			.onTargeted = [](const Targeted& /*targeted*/) -> std::optional<Attack> { return std::nullopt; },
			.onPlacement =
					[](const PlacementQuery& query)
			{
				auto placement = query.placement;
				placement.layer = Layer::Air;
				return placement;
			},
			.onSpeed = [](const SpeedQuery& query) { return Speed{query.speed.get() * 2}; },
	};

	const auto ability = [](const Effects& effects)
	{
		return std::get<AbilityEffect>(effects[0]).ability;
	};
	CHECK(ability(dynamic.on(Turn(world.world(), UnitId{1}, Round{1}, world.random()))) == Ability{"turn"});
	CHECK(ability(dynamic.on(HitAttempt(world.context(1), {.attacker = UnitId{1}, .target = UnitId{2}})))
		  == Ability{"attempt"});
	CHECK(ability(dynamic.on(HitTaken(world.context(1), {.attacker = UnitId{2}, .target = UnitId{1}})))
		  == Ability{"taken"});
	CHECK(ability(dynamic.on(RoundEnd(world.context(1)))) == Ability{"end"});
	CHECK_FALSE(dynamic.on(Targeted{.attacker = UnitId{2}, .exposure = common::melee()}).has_value());
	CHECK(dynamic.on(PlacementQuery{}).layer == Layer::Air);
	CHECK(dynamic.on(SpeedQuery{.speed = Speed{2}}) == Speed{4});
}

TEST_CASE("An applied Dynamic takes the unit's turn while its time lasts")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	world.spawn<test::Dummy>(2, {.x = 1, .y = 0});
	CHECK(world.decide(1).empty());

	const Dynamic striker{
			.onTurn =
					[](const Turn& turn)
			{
				const auto prey = turn.units() | first();
				return prey ? effect::hit(common::Melee, turn.self().id(), prey->id(), Damage{3}) : Effects{};
			},
	};
	world.simulation().perform(
			effect::apply(UnitId{1}, common::Timed<Dynamic>{.component = striker, .left = Rounds{2}}, UnitId{1}));
	CHECK(world.unit(1).has<Dynamic>());

	world.round();
	CHECK(world.log().of<UnitAttacked>().size() == 1);
	CHECK(world.unit(1).has<Dynamic>());
	world.round();
	CHECK(world.log().of<UnitAttacked>().size() == 2);
	CHECK_FALSE(world.unit(1).has<Dynamic>());
	CHECK(world.decide(1).empty());
}
