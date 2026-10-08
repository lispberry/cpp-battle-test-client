#include <Core/Base/MersenneRandom.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Query/Adaptors.hpp>
#include <Core/Query/Units.hpp>
#include <Core/Units/Health.hpp>
#include <Core/World/UnitRef.hpp>
#include <Features/Common/Attacks.hpp>
#include <Features/Common/Body.hpp>
#include <Features/Common/Invulnerable.hpp>
#include <Features/Common/Timed.hpp>
#include <Support/TestUnits.hpp>
#include <Support/TestWorld.hpp>

#include <array>
#include <cstddef>
#include <doctest/doctest.h>
#include <expected>
#include <memory>
#include <optional>
#include <vector>

using namespace sw;

namespace
{
	std::vector<uint32_t> ids(const Units& units)
	{
		std::vector<uint32_t> result;
		for (const UnitRef unit : units)
		{
			result.push_back(unit.id().get());
		}
		return result;
	}
}

TEST_CASE("Units yields the living units in creation order, without the excluded one")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(3, {.x = 0, .y = 0});
	world.spawn<test::Dummy>(1, {.x = 1, .y = 0});
	world.spawn<test::Dummy>(2, {.x = 2, .y = 0});
	CHECK(ids(Units(world.world(), std::nullopt)) == std::vector<uint32_t>{3, 1, 2});
	CHECK(ids(Units(world.world(), UnitId{1})) == std::vector<uint32_t>{3, 2});
	(*world.simulation().component<Health>(UnitId{2}))->hp = Hp{0};
	CHECK(ids(Units(world.world(), std::nullopt)) == std::vector<uint32_t>{3, 1});
}

TEST_CASE("Adaptors filter lazily and take limits")
{
	test::TestWorld world;
	const UnitRef center = world.spawn<test::Dummy>(1, {.x = 5, .y = 5});
	world.spawn<test::Dummy>(2, {.x = 6, .y = 6});
	world.spawn<test::Dummy>(3, {.x = 4, .y = 5});
	world.spawn<test::Dummy>(4, {.x = 8, .y = 5});
	world.spawn<test::Statue>(5, {.x = 5, .y = 4});
	const Units units(world.world(), UnitId{1});

	CHECK(ids(units | adjacentTo(center)) == std::vector<uint32_t>{2, 3, 5});
	CHECK(ids(units | adjacentTo(center) | take(2)) == std::vector<uint32_t>{2, 3});
	CHECK(ids(units | take(3) | take(1)) == std::vector<uint32_t>{2});
	CHECK(ids(units | take(1) | take(3)) == std::vector<uint32_t>{2});
	CHECK(ids(units | within(center, {.min = Distance{2}, .max = Distance{3}})) == std::vector<uint32_t>{4});
	CHECK(ids(units | attackableBy(center, common::melee())) == std::vector<uint32_t>{2, 3});
	CHECK(ids(units | with<common::Body>()) == std::vector<uint32_t>{5});
	CHECK(ids(units | ofKind<test::Statue>()) == std::vector<uint32_t>{5});

	int evaluated = 0;
	const Units counted = units
						  | where(
								  [&evaluated](const UnitRef& /*unit*/)
								  {
									  ++evaluated;
									  return true;
								  })
						  | take(2);
	CHECK(evaluated == 0);
	CHECK(ids(counted) == std::vector<uint32_t>{2, 3});
	CHECK(evaluated == 2);
}

TEST_CASE("with selects units carrying an applied component")
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
						 actions |= effect::apply(
								 (turn.units() | first())->id(),
								 common::Timed<common::Invulnerable>{.component = {}, .left = Rounds{5}},
								 turn.self().id());
						 return actions;
					 })});
	world.spawn<test::Dummy>(2, {.x = 1, .y = 0});
	world.round();
	const Units units(world.world(), std::nullopt);
	CHECK(ids(units | with<common::Timed<common::Invulnerable>>()) == std::vector<uint32_t>{2});
	CHECK(ids(units | with<common::Invulnerable>()) == std::vector<uint32_t>{2});
}

TEST_CASE("Terminals")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	world.spawn<test::Dummy>(2, {.x = 5, .y = 0});
	world.spawn<test::Dummy>(3, {.x = 9, .y = 0});
	const Units all(world.world(), std::nullopt);
	const Units nobody = all | take(0);

	CHECK((all | any()));
	CHECK_FALSE((nobody | any()));
	CHECK((nobody | none()));
	CHECK_FALSE((all | none()));
	CHECK((all | count()) == 3);
	CHECK((all | first())->id() == UnitId{1});
	CHECK((nobody | first()).error() == Idle::NoTarget);

	// Reservoir sampling: unit 2 is kept with oneIn(2) (index 0), unit 3 with oneIn(3); the first needs no draw.
	world.random().script({1, 0});
	CHECK((all | pickRandom(world.random()))->id() == UnitId{3});
	CHECK(world.random().draws() == 2);
	CHECK((nobody | pickRandom(world.random())).error() == Idle::NoTarget);
	CHECK(world.random().draws() == 2);

	auto it = all.begin();
	const auto previous = it++;
	CHECK((*previous).id() == UnitId{1});
	CHECK((*it).id() == UnitId{2});
}

TEST_CASE("pickRandom samples in one pass without storage, uniformly")
{
	test::TestWorld world(20, 20);
	for (uint32_t id = 1; id <= 100; ++id)
	{
		world.spawn<test::Dummy>(id, {.x = (id - 1) % 20, .y = (id - 1) / 20});
	}
	const Units all(world.world(), std::nullopt);

	// ScriptedRandom answers index() with 0 once its script is spent: every later unit replaces the pick.
	CHECK((all | pickRandom(world.random()))->id() == UnitId{100});
	CHECK(world.random().draws() == 99);

	MersenneRandom random(7);
	const Units three = all | take(3);
	std::array<int, 3> picks{};
	constexpr int Trials = 30'000;
	for (int trial = 0; trial < Trials; ++trial)
	{
		++picks.at((three | pickRandom(random))->id().get() - 1);
	}
	for (const int count : picks)
	{
		CHECK(count > Trials / 3 - 600);
		CHECK(count < Trials / 3 + 600);
	}
}
