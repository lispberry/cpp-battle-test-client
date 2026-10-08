#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Units/AnyUnit.hpp>
#include <Core/Units/Health.hpp>
#include <Core/Units/UnitOf.hpp>
#include <Core/World/GridMap.hpp>
#include <Core/World/Map.hpp>
#include <Core/World/UnitRef.hpp>
#include <Core/World/UnitRegistry.hpp>
#include <Core/World/World.hpp>
#include <Features/Common/Attacks.hpp>
#include <Features/Common/Invulnerable.hpp>
#include <Features/Common/Timed.hpp>
#include <Support/TestUnits.hpp>
#include <Support/TestWorld.hpp>

#include <doctest/doctest.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace sw;

TEST_CASE("Map dimensions")
{
	CHECK_THROWS_AS(GridMap(0, 5), std::invalid_argument);
	CHECK_THROWS_AS(GridMap(5, 0), std::invalid_argument);
	CHECK_THROWS_AS(GridMap(Map::MaxSide + 1, 5), std::invalid_argument);
	const GridMap map(4, 3);
	CHECK(map.width() == 4);
	CHECK(map.height() == 3);
	CHECK(map.contains({.origin = {.x = 2, .y = 1}, .size = {.width = 2, .height = 2}}));
	CHECK_FALSE(map.contains({.origin = {.x = 3, .y = 1}, .size = {.width = 2, .height = 1}}));
	CHECK_FALSE(map.contains({.origin = {.x = 0, .y = 2}, .size = {.width = 1, .height = 2}}));
}

TEST_CASE("Map places ground units on cells and air units over them")
{
	GridMap map(5, 5);
	const Placement tower{.origin = {.x = 1, .y = 1}, .size = {.width = 2, .height = 2}};
	REQUIRE(map.canPlace(UnitId{1}, tower));
	map.place(UnitId{1}, tower);
	CHECK(map.occupant({.x = 2, .y = 2}) == UnitId{1});
	CHECK(map.occupiedCells() == 4);
	CHECK(map.occupant({.x = 3, .y = 3}) == std::nullopt);
	CHECK(map.occupant({.x = 9, .y = 0}) == std::nullopt);
	CHECK(map.occupant({.x = 0, .y = 9}) == std::nullopt);
	CHECK(map.placement(UnitId{1})->size.width == 2);
	CHECK_FALSE(map.placement(UnitId{9}).has_value());

	CHECK_FALSE(map.canPlace(UnitId{2}, {.origin = {.x = 2, .y = 2}}));
	CHECK(map.canPlace(UnitId{1}, {.origin = {.x = 2, .y = 2}, .size = {.width = 2, .height = 2}}));
	CHECK_FALSE(map.canPlace(UnitId{2}, {.origin = {.x = 5, .y = 0}}));
	CHECK(map.canPlace(UnitId{2}, {.origin = {.x = 2, .y = 2}, .layer = Layer::Air}));

	map.place(UnitId{2}, {.origin = {.x = 2, .y = 2}, .layer = Layer::Air});
	CHECK(map.occupant({.x = 2, .y = 2}) == UnitId{1});
	map.move(UnitId{2}, {.x = 4, .y = 4});
	CHECK(map.placement(UnitId{2})->origin == Position{4, 4});
	map.remove(UnitId{2});
	CHECK(map.occupant({.x = 2, .y = 2}) == UnitId{1});

	map.move(UnitId{1}, {.x = 3, .y = 3});
	CHECK(map.occupant({.x = 1, .y = 1}) == std::nullopt);
	CHECK(map.occupant({.x = 4, .y = 4}) == UnitId{1});
	map.remove(UnitId{1});
	CHECK(map.occupant({.x = 4, .y = 4}) == std::nullopt);
	CHECK(map.occupiedCells() == 0);
	map.remove(UnitId{1});
}

TEST_CASE("Map queries: inside, free, step")
{
	GridMap map(5, 5);
	CHECK(map.isInside({.x = 4, .y = 4}));
	CHECK_FALSE(map.isInside({.x = 5, .y = 0}));
	CHECK_FALSE(map.isInside({.x = 0, .y = 5}));

	map.place(UnitId{1}, {.origin = {.x = 1, .y = 1}, .size = {.width = 2, .height = 2}});
	map.place(UnitId{2}, {.origin = {.x = 3, .y = 1}});
	map.place(UnitId{3}, {.origin = {.x = 0, .y = 4}, .layer = Layer::Air});
	CHECK(map.isFree({.x = 0, .y = 0}));
	CHECK_FALSE(map.isFree({.x = 2, .y = 2}));
	CHECK_FALSE(map.isFree({.x = 5, .y = 5}));
	CHECK(map.at(UnitId{1}).origin == Position{1, 1});
	CHECK_THROWS_AS((void)map.at(UnitId{9}), std::out_of_range);

	CHECK(map.canStep(UnitId{1}, {.x = 2, .y = 2}));		// its own cells do not block it
	CHECK_FALSE(map.canStep(UnitId{1}, {.x = 3, .y = 3}));	// not a neighbour of (1, 1)
	CHECK_FALSE(map.canStep(UnitId{1}, {.x = 2, .y = 1}));	// the 2x2 footprint would cover unit 2
	CHECK_FALSE(map.canStep(UnitId{9}, {.x = 0, .y = 0}));	// not on the map
	CHECK_FALSE(map.canStep(UnitId{9}, {.x = 0, .y = 0}, {.x = 1, .y = 0}));
	CHECK(map.canStep(UnitId{1}, {.x = 2, .y = 2}, {.x = 1, .y = 1}));	// planned from where it will be
	CHECK(map.canStep(UnitId{3}, {.x = 1, .y = 3}));					// air units fly over the ground
}

TEST_CASE("A world is created with its map and moves as a value")
{
	CHECK_THROWS_AS(World(nullptr), std::invalid_argument);
	World world(std::make_unique<GridMap>(3, 2));
	world.units().insert(makeUnit<test::Dummy>(UnitId{1}, {}));
	const World moved = std::move(world);
	CHECK(moved.map().width() == 3);
	CHECK(moved.map().height() == 2);
	CHECK(moved.units().contains(UnitId{1}));
}

TEST_CASE("The largest map has a cell for every position")
{
	GridMap map(Map::MaxSide, Map::MaxSide);
	map.place(UnitId{1}, {.origin = {.x = Map::MaxSide - 1, .y = Map::MaxSide - 1}});
	CHECK(map.occupant({.x = Map::MaxSide - 1, .y = Map::MaxSide - 1}) == UnitId{1});
	CHECK(map.occupiedCells() == 1);
}

TEST_CASE("UnitRegistry keeps creation order")
{
	UnitRegistry registry;
	registry.insert(makeUnit<test::Dummy>(UnitId{3}, {}));
	registry.insert(makeUnit<test::Dummy>(UnitId{1}, {}));
	registry.insert(makeUnit<test::Dummy>(UnitId{2}, {}));
	CHECK(std::vector<UnitId>(registry.order().begin(), registry.order().end())
		  == std::vector<UnitId>{UnitId{3}, UnitId{1}, UnitId{2}});
	CHECK(registry.contains(UnitId{1}));
	CHECK(registry.find(UnitId{1}) != nullptr);
	const UnitRegistry& constant = registry;
	CHECK(constant.find(UnitId{2})->id() == UnitId{2});
	CHECK(constant.find(UnitId{9}) == nullptr);
	CHECK(registry.find(UnitId{9}) == nullptr);
	CHECK(registry.createdBefore(UnitId{3}, UnitId{2}));
	CHECK_FALSE(registry.createdBefore(UnitId{2}, UnitId{1}));
	registry.removeIf([](const AnyUnit& unit) { return unit.id() == UnitId{1}; });
	CHECK_FALSE(registry.contains(UnitId{1}));
	CHECK(std::vector<UnitId>(registry.order().begin(), registry.order().end())
		  == std::vector<UnitId>{UnitId{3}, UnitId{2}});
	registry.insert(makeUnit<test::Dummy>(UnitId{1}, {}));
	CHECK(registry.createdBefore(UnitId{2}, UnitId{1}));
}

TEST_CASE("UnitRef reads a unit through the world")
{
	test::TestWorld world;
	const UnitRef dummy = world.spawn<test::Dummy>(1, {.x = 2, .y = 2});
	const UnitRef statue = world.spawn<test::Statue>(2, {.x = 5, .y = 2});
	CHECK(dummy.id() == UnitId{1});
	CHECK(dummy.name() == UnitName{"dummy"});
	CHECK(dummy.position() == Position{2, 2});
	CHECK(distance(dummy, statue) == Distance{3});
	CHECK(dummy.has<Health>());
	CHECK(dummy.get<Health>()->hp == Hp{10});
	CHECK_FALSE(statue.has<Health>());
	CHECK(dummy == world.unit(1));
	CHECK_FALSE(dummy == statue);

	CHECK(dummy.isAlive());
	CHECK(statue.isAlive());
	CHECK_FALSE(world.unit(9).isAlive());
	(*world.simulation().component<Health>(UnitId{1}))->hp = Hp{0};
	CHECK_FALSE(dummy.isAlive());
}

TEST_CASE("UnitRef answers who may be attacked")
{
	test::TestWorld world;
	const UnitRef attacker = world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	const UnitRef near = world.spawn<test::Dummy>(2, {.x = 1, .y = 1});
	const UnitRef far = world.spawn<test::Dummy>(3, {.x = 5, .y = 0});
	const UnitRef statue = world.spawn<test::Statue>(4, {.x = 0, .y = 1});
	const UnitRef fortress = world.spawn<test::Fortress>(5, {.x = 1, .y = 2});
	const UnitRef flyer = world.spawn<test::Flyer>(6, {.x = 0, .y = 2});

	CHECK(near.isAttackableBy(attacker, common::melee()));
	CHECK_FALSE(far.isAttackableBy(attacker, common::melee()));
	CHECK_FALSE(statue.isAttackableBy(attacker, common::melee()));
	CHECK_FALSE(fortress.isAttackableBy(attacker, common::melee()));
	CHECK_FALSE(fortress.exposureTo(attacker, common::melee()).has_value());
	CHECK_FALSE(flyer.isAttackableBy(attacker, common::melee()));
	// Ranged 2..3 against a flyer becomes 1..2: the flyer two cells away is in range.
	CHECK(flyer.isAttackableBy(attacker, common::ranged({.min = Distance{2}, .max = Distance{3}})));
	CHECK(flyer.exposureTo(attacker, common::ranged({.min = Distance{2}, .max = Distance{3}}))->range.min
		  == Distance{1});

	CHECK_FALSE(near.has<common::Invulnerable>());
}
