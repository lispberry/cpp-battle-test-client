#include <Core/Events/Turn.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Query/Units.hpp>
#include <Core/Simulation/Simulation.hpp>
#include <Core/Units/Health.hpp>
#include <Core/Units/UnitOf.hpp>
#include <Core/World/Map.hpp>
#include <Features/Common/Body.hpp>
#include <Features/Common/Invulnerable.hpp>
#include <Features/Common/Timed.hpp>
#include <Support/RecordingLog.hpp>
#include <Support/ScriptedRandom.hpp>
#include <Support/TestUnits.hpp>
#include <Support/TestWorld.hpp>

#include <doctest/doctest.h>
#include <expected>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

using namespace sw;

TEST_CASE("Simulation creates the world and spawns units where they fit")
{
	auto random = std::make_unique<test::ScriptedRandom>();
	auto recording = std::make_unique<test::RecordingLog>();
	const auto* randomSource = random.get();
	const auto& log = *recording;
	Simulation simulation(std::move(random), std::move(recording));
	CHECK(&simulation.randomSource() == randomSource);
	CHECK_FALSE(simulation.hasMap());
	CHECK_THROWS_AS((void)simulation.world(), std::logic_error);
	CHECK(simulation.isFinished());
	simulation.step();
	CHECK(simulation.round() == Round{0});
	CHECK(simulation.spawn({.position = {}, .unit = makeUnit<test::Dummy>(UnitId{1}, {})}).error()
		  == WorldError::NoMap);
	CHECK(simulation.component<Health>(UnitId{1}).error() == WorldError::NoMap);
	CHECK_THROWS_AS((void)simulation.decide(UnitId{1}, Round{1}), std::logic_error);
	CHECK_THROWS_AS(simulation.perform({}), std::logic_error);

	CHECK(simulation.createMap(0, 4).error() == WorldError::MapEmpty);
	CHECK(simulation.createMap(4, 0).error() == WorldError::MapEmpty);
	CHECK(simulation.createMap(Map::MaxSide + 1, 4).error() == WorldError::MapTooLarge);
	CHECK(simulation.createMap(4, Map::MaxSide + 1).error() == WorldError::MapTooLarge);
	CHECK(simulation.isFinished());
	REQUIRE(simulation.createMap(4, 4));
	CHECK(simulation.hasMap());
	CHECK(simulation.createMap(5, 5).error() == WorldError::MapAlreadyCreated);
	CHECK(simulation.world().map().width() == 4);
	CHECK_FALSE(simulation.isFinished());
	REQUIRE(simulation.spawn({.position = {.x = 1, .y = 1}, .unit = makeUnit<test::Dummy>(UnitId{1}, {})}));
	CHECK(simulation.spawn({.position = {.x = 2, .y = 2}, .unit = makeUnit<test::Dummy>(UnitId{1}, {})}).error()
		  == WorldError::DuplicateId);
	CHECK(simulation.spawn({.position = {.x = 3, .y = 3}, .unit = makeUnit<test::Fortress>(UnitId{2}, {})}).error()
		  == WorldError::OutOfBounds);
	CHECK(simulation.spawn({.position = {.x = 0, .y = 0}, .unit = makeUnit<test::Fortress>(UnitId{2}, {})}).error()
		  == WorldError::CellOccupied);
	REQUIRE(simulation.spawn({.position = {.x = 2, .y = 2}, .unit = makeUnit<test::Fortress>(UnitId{2}, {})}));
	CHECK(simulation.world().map().occupant({.x = 3, .y = 3}) == UnitId{2});
	CHECK(log.text()
		  == "[0] MAP_CREATED width=4 height=4 \n"
			 "[0] UNIT_SPAWNED unitId=1 unitType=dummy x=1 y=1 \n"
			 "[0] UNIT_SPAWNED unitId=2 unitType=fortress x=2 y=2 \n");
}

TEST_CASE("Simulation is a movable value that owns its parts")
{
	// Not nothrow: MSVC's std::unordered_map (the unit registry) allocates in its move constructor.
	static_assert(std::is_move_constructible_v<Simulation> && std::is_move_assignable_v<Simulation>);
	static_assert(std::is_move_constructible_v<World> && std::is_move_assignable_v<World>);
	CHECK_THROWS_AS(Simulation(nullptr, std::make_unique<test::RecordingLog>()), std::invalid_argument);
	CHECK_THROWS_AS(Simulation(std::make_unique<test::ScriptedRandom>(), nullptr), std::invalid_argument);

	Simulation original(std::make_unique<test::ScriptedRandom>(), std::make_unique<test::RecordingLog>());
	REQUIRE(original.createMap(3, 3));
	REQUIRE(original.spawn({.position = {.x = 1, .y = 1}, .unit = makeUnit<test::Dummy>(UnitId{1}, {})}));
	Simulation moved = std::move(original);
	CHECK(moved.world().units().contains(UnitId{1}));
	moved.run();
	CHECK(moved.isFinished());
}

TEST_CASE("A unit spawned dead is reported and removed at the end of the first round")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0}, {.health = {Hp{0}}});
	world.spawn<test::Dummy>(2, {.x = 1, .y = 0});
	world.round();
	CHECK(world.log().text() == "[1] UNIT_DIED unitId=1 \n");
	CHECK_FALSE(world.world().units().contains(UnitId{1}));
	CHECK_FALSE(world.world().map().occupant({.x = 0, .y = 0}).has_value());
}

TEST_CASE("Simulation::component")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	CHECK(world.simulation().component<Health>(UnitId{9}).error() == WorldError::UnknownUnit);
	CHECK(world.simulation().component<common::Body>(UnitId{1}).error() == WorldError::MissingComponent);
	CHECK((*world.simulation().component<Health>(UnitId{1}))->hp == Hp{10});
}

TEST_CASE("A round runs units in creation order and skips one killed earlier in the round")
{
	test::TestWorld world;
	world.spawn<test::Scripted>(
			1,
			{.x = 0, .y = 0},
			{.health = {Hp{10}},
			 .script = test::acts(
					 [](const Turn& turn) -> Effects
					 {
						 Effects actions;
						 actions |= effect::attack(turn.self().id(), (turn.units() | first())->id(), Damage{10});
						 return actions;
					 })});
	world.spawn<test::Scripted>(
			2,
			{.x = 1, .y = 0},
			{.health = {Hp{10}},
			 .script = test::acts(
					 [](const Turn& turn) -> Effects
					 {
						 Effects actions;
						 actions |= effect::useAbility(turn.self().id(), Ability{"never"});
						 return actions;
					 })});
	world.round();
	CHECK(world.simulation().round() == Round{1});
	CHECK(world.log().of<UnitAbilityUsed>().empty());
	CHECK(world.log().of<UnitDied>().size() == 1);
	CHECK_FALSE(world.simulation().isFinished());
}

TEST_CASE("The battle ends after a round without actions or pending effects")
{
	test::TestWorld world;
	world.spawn<test::Scripted>(
			1,
			{.x = 0, .y = 0},
			{.health = {Hp{10}},
			 .script
			 = {[](const Turn& turn) -> Effects
				{
					if (turn.round() != Round{1})
					{
						return {};
					}
					return effect::apply(
							turn.self().id(), common::Timed<common::Invulnerable>{{}, Rounds{3}}, turn.self().id());
				}}});
	world.simulation().report(UnitDied{.unitId = 42});
	CHECK(world.log().text() == "[0] UNIT_DIED unitId=42 \n");
	world.simulation().run();
	// Round 1 acts; round 2 only has the pending effect, which expires at the end of round 3.
	CHECK(world.simulation().round() == Round{3});
	CHECK(world.simulation().isFinished());
}
