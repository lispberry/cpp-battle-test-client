#include <Core/Events/Effects.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/PlacementQuery.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Events/SpeedQuery.hpp>
#include <Core/Events/Targeted.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/IO/Commands.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Simulation/Simulation.hpp>
#include <Core/Units/Health.hpp>
#include <Core/Units/Unit.hpp>
#include <Features/Common/Attacks.hpp>
#include <Features/Common/Body.hpp>
#include <Features/Common/Flying.hpp>
#include <Features/Common/Invulnerable.hpp>
#include <Features/Common/March.hpp>
#include <Features/Common/MarchCommand.hpp>
#include <Features/Common/MeleeAttack.hpp>
#include <Features/Common/RangedAttack.hpp>
#include <Features/Common/Timed.hpp>
#include <Support/RecordingLog.hpp>
#include <Support/ScriptedRandom.hpp>
#include <Support/TestUnits.hpp>
#include <Support/TestWorld.hpp>

#include <doctest/doctest.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace sw;
using common::March;
using common::march;
using common::MeleeAttack;
using common::RangedAttack;

namespace
{
	enum class Uses : uint8_t
	{
		Melee,
		Ranged,
		March,
	};

	// Which component the soldier acts with: a component itself, since only components may make up a unit.
	struct Mode
	{
		Uses uses{Uses::Melee};
	};

	struct SoldierKit
	{
		Health health{Hp{10}};
		MeleeAttack melee{Damage{2}};
		RangedAttack ranged{.range = {.min = Distance{2}, .max = Distance{3}}, .damage = Damage{4}};
		March march;
		Mode mode;
	};
	SW_REFLECT(SoldierKit, (), (health, melee, ranged, march, mode))

	// A unit that uses whichever common component the test configures.
	class Soldier final : public Unit<Soldier, SoldierKit>
	{
	public:
		static constexpr UnitName Name{"soldier"};

		using Unit::Unit;

		[[nodiscard]]
		Effects on(const Turn& turn) const
		{
			switch (kit().mode.uses)
			{
				case Uses::Melee: return kit().melee.on(turn);
				case Uses::Ranged: return kit().ranged.on(turn);
				case Uses::March: break;
			}
			return kit().march.on(turn);
		}
	};

	SoldierKit soldier(
			const Uses uses, const std::optional<Position> target = std::nullopt, const Speed speed = Speed{1})
	{
		return {.march = {.speed = speed, .target = target}, .mode = {uses}};
	}
}

TEST_CASE("MeleeAttack hits a random adjacent unit")
{
	test::TestWorld world;
	world.spawn<Soldier>(1, {.x = 5, .y = 5}, soldier(Uses::Melee));
	CHECK(world.decide(1).empty());
	world.spawn<test::Dummy>(2, {.x = 6, .y = 6});
	world.spawn<test::Dummy>(3, {.x = 4, .y = 5});
	world.random().script({0});	 // the second neighbour replaces the first (reservoir sampling)
	const auto decision = world.decide(1);
	REQUIRE_FALSE(decision.empty());
	const auto& hit = std::get<HitEffect>(decision[0]);
	CHECK(hit.kind == common::Melee);
	CHECK(hit.target == UnitId{3});
	CHECK(hit.damage == Damage{2});
}

TEST_CASE("RangedAttack shoots a unit within its range")
{
	test::TestWorld world;
	world.spawn<Soldier>(1, {.x = 0, .y = 0}, soldier(Uses::Ranged));
	world.spawn<test::Dummy>(2, {.x = 1, .y = 0});
	CHECK(world.decide(1).empty());
	world.spawn<test::Dummy>(3, {.x = 3, .y = 0});
	const auto decision = world.decide(1);
	const auto& hit = std::get<HitEffect>(decision[0]);
	CHECK(hit.kind == common::Ranged);
	CHECK(hit.target == UnitId{3});
	CHECK(hit.damage == Damage{4});
}

TEST_CASE("RangedAttack that holds when engaged does not shoot while a unit is adjacent")
{
	test::TestWorld world;
	auto kit = soldier(Uses::Ranged);
	kit.ranged.holdWhenEngaged = true;
	world.spawn<Soldier>(1, {.x = 0, .y = 0}, kit);
	world.spawn<test::Dummy>(2, {.x = 3, .y = 0});
	CHECK_FALSE(world.decide(1).empty());
	world.spawn<test::Dummy>(3, {.x = 1, .y = 1});
	CHECK(world.decide(1).empty());
}

TEST_CASE("March walks toward its target and reports arrival")
{
	test::TestWorld world;
	world.spawn<Soldier>(1, {.x = 0, .y = 0}, soldier(Uses::March));
	CHECK(world.decide(1).empty());

	world.spawn<Soldier>(3, {.x = 0, .y = 9}, soldier(Uses::March, Position{2, 9}));
	world.round();
	world.round();
	CHECK(world.unit(3).position() == Position{2, 9});
	CHECK(world.log().of<MarchEnded>().size() == 1);
	CHECK(world.log().of<UnitMoved>().size() == 2);

	// Arrival finishes the order: the target is cleared and the unit stays idle.
	CHECK_FALSE(world.unit(3).get<common::March>()->target);
	CHECK(world.decide(3).empty());
}

TEST_CASE("March to the cell the unit stands on ends on its first turn, as in the original prototype")
{
	test::TestWorld world;
	world.spawn<Soldier>(1, {.x = 5, .y = 5}, soldier(Uses::March, Position{5, 5}));
	world.round();
	CHECK(world.log().text() == "[1] MARCH_ENDED unitId=1 x=5 y=5 \n");
	CHECK_FALSE(world.unit(1).get<common::March>()->target);

	world.log().clear();
	world.round();
	CHECK(world.log().text().empty());
	CHECK(world.decide(1).empty());
}

TEST_CASE("March with speed 2 moves two cells, and stops early when blocked")
{
	test::TestWorld world;
	world.spawn<Soldier>(1, {.x = 0, .y = 0}, soldier(Uses::March, Position{4, 0}, Speed{2}));
	world.spawn<test::Dummy>(2, {.x = 2, .y = 0});
	world.round();
	CHECK(world.unit(1).position() == Position{1, 0});
	CHECK(world.decide(1).empty());

	world.spawn<Soldier>(3, {.x = 0, .y = 5}, soldier(Uses::March, Position{2, 5}, Speed{2}));
	world.round();
	CHECK(world.unit(3).position() == Position{2, 5});
	CHECK(world.log().of<MarchEnded>().size() == 1);

	world.spawn<Soldier>(4, {.x = 0, .y = 8}, soldier(Uses::March, Position{1, 8}, Speed{3}));
	world.round();
	CHECK(world.unit(4).position() == Position{1, 8});
}

TEST_CASE("MARCH gives a unit with a March component its target")
{
	test::TestWorld world(6, 6);
	world.spawn<Soldier>(1, {.x = 0, .y = 0}, soldier(Uses::March));
	world.spawn<test::Dummy>(2, {.x = 5, .y = 5});
	Commands commands;
	common::registerCommon(commands);

	Simulation empty(std::make_unique<test::ScriptedRandom>(), std::make_unique<test::RecordingLog>());
	CHECK(march(empty, {.unitId = 1, .targetX = 1, .targetY = 1}).error() == "the map has not been created");
	CHECK(empty.component<March>(UnitId{1}).error() == WorldError::NoMap);
	CHECK(march(world.simulation(), {.unitId = 1, .targetX = 6, .targetY = 0}).error()
		  == "position is outside the map");
	CHECK(march(world.simulation(), {.unitId = 9, .targetX = 1, .targetY = 1}).error() == "no such unit");
	CHECK(march(world.simulation(), {.unitId = 2, .targetX = 1, .targetY = 1}).error() == "the unit cannot do that");
	REQUIRE(commands.execute("MARCH 1 3 0", 1, world.simulation()));
	CHECK(world.unit(1).get<March>()->target == Position{3, 0});
	CHECK(world.log().text() == "[0] MARCH_STARTED unitId=1 x=0 y=0 targetX=3 targetY=0 \n");
}

TEST_CASE("melee and ranged attacks")
{
	const Attack melee = common::melee();
	CHECK(melee.kind == common::Melee);
	CHECK(melee.range.min == Distance{1});
	CHECK(melee.range.max == Distance{1});
	const Attack ranged = common::ranged({.min = Distance{2}, .max = Distance{5}});
	CHECK(ranged.kind == common::Ranged);
	CHECK(ranged.range.max == Distance{5});
}

TEST_CASE("Body, Flying and Invulnerable")
{
	PlacementQuery query{.placement = {.origin = {.x = 4, .y = 4}}};
	query.placement = common::Body{.width = 2, .height = 3}.on(query);
	CHECK(query.placement.size.width == 2);
	CHECK(query.placement.size.height == 3);
	query.placement = common::Flying{}.on(query);
	CHECK(query.placement.layer == Layer::Air);
	CHECK(query.placement.origin == Position{4, 4});

	CHECK_FALSE(common::Flying{}.on(Targeted{.attacker = UnitId{1}, .exposure = common::melee()}).has_value());
	const auto ranged = common::Flying{}.on(
			Targeted{.attacker = UnitId{1}, .exposure = common::ranged({.min = Distance{2}, .max = Distance{4}})});
	REQUIRE(ranged.has_value());
	CHECK(ranged->range.min == Distance{1});
	CHECK(ranged->range.max == Distance{3});

	CHECK_FALSE(
			common::Invulnerable{}
					.on(Targeted{
							.attacker = UnitId{1},
							.exposure = common::ranged({.min = Distance{2}, .max = Distance{4}})})
					.has_value());
}

TEST_CASE("Timed forwards the handlers of its component and expires")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	const auto calls = std::make_shared<std::vector<std::string>>();
	common::Timed<test::Probe> timed{.component = {.calls = calls, .label = "t"}, .left = Rounds{2}};

	const Targeted targeted{.attacker = UnitId{1}, .exposure = common::melee()};
	CHECK(timed.on(targeted).has_value());
	CHECK(timed.on(PlacementQuery{}).size.width == 1);
	CHECK(timed.on(SpeedQuery{.speed = Speed{2}}) == Speed{2});
	CHECK(timed.on(HitAttempt(world.context(1), {.attacker = UnitId{1}, .target = UnitId{2}})).empty());
	CHECK(timed.on(HitTaken(world.context(1), {.attacker = UnitId{1}, .target = UnitId{2}})).empty());

	auto first = timed.on(RoundEnd(world.context(1, Round{1})));
	CHECK_FALSE(first.removeExpire());
	CHECK(timed.left == Rounds{1});
	auto second = timed.on(RoundEnd(world.context(1, Round{2})));
	CHECK(second.removeExpire());
	CHECK(timed.left == Rounds{0});
	CHECK(calls->size() == 7);

	// A component without its own RoundEnd handler still expires.
	common::Timed<common::Invulnerable> shield{.component = {}, .left = Rounds{1}};
	CHECK_FALSE(shield.on(targeted).has_value());
	auto end = shield.on(RoundEnd(world.context(1)));
	CHECK(end.removeExpire());
}
